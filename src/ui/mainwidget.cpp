#include "mainwidget.h"
#include "ui_mainwidget.h"
#include "threadpool/threadpool.h" // 引入自定义线程池


MainWidget::MainWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MainWidget),
    _curDialogId(-1),
    setChanger(false),
    _isExpand(true)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/img/logo.png"));

    //UI初始化
    ui->stackedWidget->setCurrentIndex(0);
    ui->sidebarWidget->setMinimumWidth(250);
    ui->listWidgetChat->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); // 平滑滚动
    ui->listWidgetChat->setWordWrap(true);
    ui->sidebarWidget->setAttribute(Qt::WA_StyledBackground);

    //语言初始化
    ui->comboLanguage->clear();
    ui->comboLanguage->addItem("简体中文", "zh_CN");
    ui->comboLanguage->addItem("English", "en_US");

    //信号与槽连接
    connect(ui->comboLanguage, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWidget::on_comboLanguage_currentIndexChanged);

    //Aiclient信号
    connect(&Aiclient::getInstance(), &Aiclient::answerChunkReceived, this, &MainWidget::onAiChunkReceived);
    connect(&Aiclient::getInstance(), &Aiclient::answerFinished, this, &MainWidget::onAiAnswerFinished);
    connect(&Aiclient::getInstance(), &Aiclient::errorOccurred, this, &MainWidget::onErrorOccurred);

    connect(&ExceptHandler::getInstance(), &ExceptHandler::errorOccurred, this, [this](ErrorCode code, const QString& msg){
        if (code == ErrorCode::FileNotFound || msg.contains("Pandoc") || msg.contains("不支持的文件")) {
            // 只在控制台打日志，方便开发者排查
            qWarning() << "[非致命警告]" << msg;
            // 只在底部的状态栏显示3秒钟的黄色警告
            ui->labelStatus->setText(tr("🟡️ 文件处理跳过: %1").arg(msg.left(30) + "..."));
            ui->labelStatus->setStyleSheet("color: #DE7B52;");
            // 3 秒后恢复状态栏默认显示
            QTimer::singleShot(3000, this, [this]() {
                if (ui->labelStatus->text().startsWith("🟡️")) {
                    ui->labelStatus->setText(tr("🟢 模型已就绪"));
                    ui->labelStatus->setStyleSheet("color: #334155;");
                }
            });
        } else {
            //网络超时、LLM 崩溃、数据库损坏
            this->onErrorOccurred(msg);
        }
    });

    // 模型切换与意图解析
    connect(ui->comboModel, &QComboBox::currentTextChanged, this, [](const QString &text){
        if (text != tr("未检测到模型") && !text.isEmpty()) {
            Aiclient::getInstance().setAI(text);
            qDebug() << "模型已切换为：" << text;
        }
    });
    connect(&Aiclient::getInstance(), &Aiclient::keywordParsed, this, &MainWidget::onKeywordParsed);

    // 模型自动轮询
    llmCheaker = new QTimer(this);
    connect(llmCheaker, &QTimer::timeout, this, &MainWidget::refreshModels);
    refreshModels();
    refreshSidebar();
}

MainWidget::~MainWidget()
{
    delete ui;
}

// 核心聊天逻辑 第一阶段：发送请求
void MainWidget::on_btnSend_clicked()
{
    _finalHtml.clear();
    QString userInput = ui->textEditInput->toPlainText().trimmed();
    if (userInput.isEmpty()) return;
    if (_curDialogId == -1) on_btnNewChat_clicked();

    _nowDialogId = _curDialogId;
    _aiTxt.clear();

    // 标准化：用户输入存入本地数据库
    QString userHtml = userInput.toHtmlEscaped().replace("\n", "<br>");
    addChatMsg(userHtml, true);
    Datamanager::getInstance().saveMessage(_curDialogId, "user", userHtml);

    ui->textEditInput->clear();
    ui->labelStatus->setText(tr("🤔 正在思考..."));
    ui->labelStatus->setStyleSheet("color: green;");
    ui->btnSend->setEnabled(false);

    // 预创建 AI 回语气泡
    addChatMsg("", false);
    _currentInput = userInput;

    // 动态组装意图识别 Prompt
    QString statusStr = _uploadedFilePath.isEmpty() ? tr("当前无附件") :
                            tr("当前已挂载附件：[") + QFileInfo(_uploadedFilePath).fileName() + "]";

    QString historyContext = DialogBuilder(_curDialogId, 2);
    QString intentQuery = PromptForGetIntent.arg(statusStr, userInput, historyContext);

    // 请求大模型识别意图 (异步信号 keywordParsed 触发下一阶段)
    Aiclient::getInstance().getKeyword(intentQuery);
    _curHistoryContext = historyContext;
}

// RAG 处理逻辑 第二阶段：检索与回答)
void MainWidget::onKeywordParsed(ParsedIntent intent)
{
    QString currentStatus = tr("🔍正在并发检索本地知识库...");
    if (intent.intentType == IntentType::ListCrossSearch) {
        currentStatus = tr("📄正在提取关键实体并交叉比对...");
    } else if (intent.intentType == IntentType::DocumentInsight || intent.intentType == IntentType::HybridCompare) {
        currentStatus = tr("📄正在深度分析附件内容...");
    }
    ui->labelStatus->setStyleSheet("color: green;");

    // 处理驻留附件
    if (!_uploadedFileText.isEmpty()) {
        intent.uploadedFileName = QFileInfo(_uploadedFilePath).fileName();
        intent.uploadedFilePath = _uploadedFilePath;
        intent.uploadedFileText = _uploadedFileText;
        currentStatus += tr(" (已挂载附件)");
    }

    ui->labelStatus->setText(currentStatus);

    QPointer<MainWidget> safeThis = this;
    QString userInput = _currentInput;

    // 4. 将高负载任务扔进线程池
    ThreadPool::getInstance().addTask([safeThis, intent, userInput](){
        TaskResult result;
        auto router = TaskFactory::createRouter(intent.intentType);
        if (router) {
            result = router->execute(intent);
            // router 为 unique_ptr，函数结束自动销毁
        }

        // 5. 检索完毕，切回主线程进行流式生成
        QMetaObject::invokeMethod(safeThis, [safeThis, result, userInput]() {
            if (!safeThis) return;
            safeThis->ui->labelStatus->setText(tr("💡信息收集完毕，正在思考回答..."));
            safeThis->ui->labelStatus->setStyleSheet("color: green;");
            Aiclient::getInstance().printAnswerStream(result, userInput);
        }, Qt::QueuedConnection);
    });
}

// UI 流式渲染回调
// UI 流式渲染回调
void MainWidget::onAiChunkReceived(const QString &chunk, int type)
{
    _aiTxt += chunk;
    _finalHtml = parseAiStreamToHtml(_aiTxt);

    if (_curAiBubble && (_curDialogId == _nowDialogId))
    {
        _curAiBubble->updateContent(_finalHtml);

        // 修复 不盲目取最后一个，而是倒序查找挂载了当前 AI 气泡的 Item
        QListWidgetItem* targetItem = nullptr;
        for (int i = ui->listWidgetChat->count() - 1; i >= 0; --i) {
            QListWidgetItem* item = ui->listWidgetChat->item(i);
            if (ui->listWidgetChat->itemWidget(item) == _curAiBubble) {
                targetItem = item;
                break;
            }
        }

        // 如果找到了对应的 Item，才更新它的高度
        if (targetItem) {
            targetItem->setSizeHint(_curAiBubble->sizeHint());

            // 异步微延时二次重算，防止长文本瞬间插入导致的排版截断
            QPointer<ChatBubbleWidget> safeBubble = _curAiBubble;
            QTimer::singleShot(50, this, [this, targetItem, safeBubble]() {
                // 在延时闭包中，为了绝对安全，再次确认气泡对象还在
                if (safeBubble) {
                    // Qt 内部机制：只要 itemWidget 匹配，说明 targetItem 没有被销毁
                    // 我们直接更新高度即可
                    targetItem->setSizeHint(safeBubble->sizeHint());
                    ui->listWidgetChat->scrollToBottom();
                }
            });
        }
    }
}

void MainWidget::onAiAnswerFinished()
{
    ui->labelStatus->setText(tr("🟢 模型已就绪"));
    ui->labelStatus->setStyleSheet("color: green;");
    ui->btnSend->setEnabled(true);

    if (!_finalHtml.isEmpty() && _nowDialogId != -1) {
        Datamanager::getInstance().saveMessage(_nowDialogId, "ai", _finalHtml);
    }
    _curAiBubble = nullptr;
    _nowDialogId = -1;
}

// 辅助逻辑：文件与历史管理
void MainWidget::on_btnUploadFile_clicked() {
    if (!_uploadedFilePath.isEmpty()) {
        auto reply = QMessageBox::question(this, tr("替换附件"),
                                           tr("当前已挂载一个附件，继续上传将覆盖旧附件。是否继续？"),
                                           QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) return;
    }

    QString fileName = QFileDialog::getOpenFileName(this, tr("选择文件"), "", tr("文档 (*.txt *.pdf *.docx)"));
    if (fileName.isEmpty()) return;

    auto parser = FileFactory::createRouter(FileFactory::detectFileType(fileName));
    if (!parser) {
        ExceptHandler::getInstance().reportError(ErrorCode::FileNotFound, tr("不支持的文件格式"));
        return;
    }

    FileTxt fileResult = parser->extractText(fileName);
    if (!fileResult.isOpen || fileResult.Text.trimmed().isEmpty()) {
        ExceptHandler::getInstance().reportError(ErrorCode::FileNotFound, tr("无法读取文件内容，可能是文件损坏或加密。"));
        _uploadedFilePath.clear();
        _uploadedFileText.clear(); // 清除幽灵文本
        ui->labelStatus->setText(tr("🟢 模型已就绪"));
        return;
    }

    _uploadedFilePath = fileName;
    ui->labelStatus->setText(tr("已挂载附件: ") + QFileInfo(fileName).fileName());

    // 启发式文本采样
    QString rawText = fileResult.Text;
    if (rawText.length() > 3000) {
        QString head = rawText.left(1000);
        QString tail = rawText.right(1000);
        QString mid = rawText.mid(rawText.length() / 2 - 500, 1000);
        _uploadedFileText = head + "\n\n...[内容省略]...\n\n" + mid + "\n\n...[内容省略]...\n\n" + tail;
        QMessageBox::information(this, tr("文件过长"), tr("系统已自动提取首、中、尾关键段落进入记忆。"));
    } else {
        _uploadedFileText = rawText;
    }
}

void MainWidget::reNameDialog(int sessionId, const QString& newTitle)
{
    if (Datamanager::getInstance().renameSession(sessionId, newTitle)) {
        qDebug() << "会话" << sessionId << "重命名为:" << newTitle;
    } else {
        ExceptHandler::getInstance().reportError(ErrorCode::DatabaseQueryFailed,
                                                 tr("重命名会话失败: 会话 ID %1").arg(sessionId));
    }
}

void MainWidget::onErrorOccurred(const QString& errorMsg)
{
    ui->labelStatus->setText(tr("🔴 系统异常"));
    ui->labelStatus->setStyleSheet("color: red;");

    // 【修复闪退】：安全移除幽灵气泡
    if (_curAiBubble) {
        // 倒序查找是哪个 Item 挂载了这个气泡
        for (int i = ui->listWidgetChat->count() - 1; i >= 0; --i) {
            QListWidgetItem* item = ui->listWidgetChat->item(i);
            if (ui->listWidgetChat->itemWidget(item) == _curAiBubble) {
                // 1. 从列表中摘除并删除 Item
                delete ui->listWidgetChat->takeItem(i);
                break;
            }
        }
        // 2. 延迟安全删除 Widget 本身，绝对防止闪退
        _curAiBubble->deleteLater();
        _curAiBubble = nullptr;
    }

    addChatMsg(QString("<span style='color:red;'>[系统异常] %1</span>").arg(errorMsg), false);
    ui->btnSend->setEnabled(true);
}

void MainWidget::on_btnBackToChat_clicked()
{
    ui->stackedWidget->setCurrentIndex(0); // 返回对话
    qDebug() << "返回对话页面";
}


// 辅助工具逻辑
void MainWidget::addChatMsg(const QString &text, bool isUser)
{
    // 1. 创建原生的 Item 占位
    QListWidgetItem *item = new QListWidgetItem(ui->listWidgetChat);

    // 2. 创建我们写的富文本气泡组件
    ChatBubbleWidget *bubbleWidget = new ChatBubbleWidget(text, isUser, this);

    // 3. 拦截链接点击信号，执行系统拉起文件
    connect(bubbleWidget, &ChatBubbleWidget::linkClicked, this, [](const QUrl &url) {
        qDebug() << "正在通过操作系统外部唤醒:" << url.toString();
        QDesktopServices::openUrl(url);
    });

    // 4. 将 Widget 挂载到 Item 上
    item->setSizeHint(bubbleWidget->sizeHint()); // 初始化高度
    ui->listWidgetChat->setItemWidget(item, bubbleWidget);
    ui->listWidgetChat->scrollToBottom();

    // 如果是 AI 创建的空气泡，把它捏在手里
    if (!isUser && text.isEmpty()) {
        _curAiBubble = bubbleWidget;
    }
}

QString MainWidget::parseAiStreamToHtml(const QString& rawStream) {
    QString linksPart = "";
    QString llmPart = rawStream;

    // 1. 物理切割：保护我们在 Aiclient 里注入的参考文件 HTML 不被转义破坏
    int hrIndex = rawStream.indexOf("<hr style=");
    if (hrIndex != -1) {
        int splitPoint = rawStream.indexOf("><br>", hrIndex);
        if (splitPoint != -1) {
            splitPoint += 5;
            linksPart = rawStream.left(splitPoint);
            llmPart = rawStream.mid(splitPoint);
        }
    } else if (rawStream.startsWith("<b style='color: #2c3e50;'>📚 参考本地文件：</b>")) {
        return rawStream;
    }

    // 2. 全局安全转义
    QString safeLlm = llmPart.toHtmlEscaped();

    // 3. Think黑盒化拦截与动态渲染
    int thinkStartIdx = safeLlm.indexOf("&lt;think&gt;");
    int thinkEndIdx = safeLlm.indexOf("&lt;/think&gt;");

    if (thinkStartIdx != -1) {
        if (thinkEndIdx != -1) {
            // 状态 A：思考已经完成
            // 剥离出思考过程，将其隐藏，替换为一行极简的完成提示
            QString replacement = "<div style='color: #64748b; border-left: 3px solid #10b981; padding-left: 8px; margin: 5px 0; font-size: 12px; background-color: #f1f5f9; border-radius: 4px;'>"
                                  "<i>🟢 深度思考完成，开始输出结论</i></div>";


            // 将从 <think> 到 </think> 的所有几万字废话全部替换为上面这行 HTML
            safeLlm.replace(thinkStartIdx, thinkEndIdx - thinkStartIdx + 14, replacement);
        } else {
            // 状态 B：思考中...
            // 提取出当前已经生成的思考字符数，用于动态跳动展示
            QString currentThought = safeLlm.mid(thinkStartIdx + 13);
            int thoughtLength = currentThought.length(); // 近似字数
            QString replacement = QString("<div style='color: #8b5cf6; border-left: 3px solid #8b5cf6; padding-left: 8px; margin: 5px 0; font-size: 12px; background-color: #f5f3ff; border-radius: 4px;'>"
                                          "<i>🟣 正在进行逻辑推演... (动态演算量: %1)</i></div>").arg(thoughtLength);

            // 将从 <think> 开始到末尾的所有内容截断，替换为这句跳动的提示
            safeLlm.replace(thinkStartIdx, safeLlm.length() - thinkStartIdx, replacement);
        }
    }

    // 4. 将原始换行符替换为 HTML 换行
    safeLlm.replace("\n", "<br>");

    return linksPart + safeLlm;
}

QString MainWidget::DialogBuilder(int sId, int maxRounds)
{
    if (sId == -1) return "无";

    QList<MessageInfo> recentMsgs = Datamanager::getInstance().getMessagesBySession(sId);
    if (recentMsgs.isEmpty()) return "无";

    // 只截取最近的maxRounds * 2条消息
    int startIndex = qMax(0, (int)recentMsgs.size() - (maxRounds * 2));
    QString historyStr;
    historyStr.reserve(1024);

    // 静态正则用于剥离 HTML 标签和可能残留的 Markdown
    static const QRegularExpression htmlRegex("<[^>]*>");
    static const QRegularExpression markdownRegex("(\\*\\*|__|_|\\*)");

    for (int i = startIndex; i < recentMsgs.size(); ++i) {
        const auto& msg = recentMsgs[i];

        // 1. 基础清洗
        QString cleanText = msg.content;
        cleanText.remove(htmlRegex);
        cleanText.remove(markdownRegex);
        cleanText = cleanText.trimmed();

        // 2. 过滤无价值的系统提示或死循环提示
        if (cleanText.isEmpty() || cleanText.contains("📚 参考本地文件：")) continue;

        // 3. 错误状态屏蔽 (防止残缺/报错文本误导下一次的规划)
        if (cleanText.contains("系统异常") || cleanText.contains("生成失败") || cleanText.contains("超时")) {
            if (msg.role == "assistant") {
                historyStr += "AI: [该轮任务执行失败或未完成]\n";
                continue;
            }
        }

        //非对称截断 Asymmetric Truncation
        if (msg.role == "user") {
            // 用户输入通常代表核心意图，保留较长篇幅 (例如 150 字)
            if (cleanText.length() > 150) {
                cleanText = cleanText.left(150) + "...";
            }
            historyStr += "User: " + cleanText + "\n";
        }
        else {
            // AI 输出通常极其冗长，但核心结论(如提取出的人名、判断结果)往往在第一句话，只保留前 80 个字符
            if (cleanText.length() > 80) {
                // 寻找前 80 个字符中最后一个句号或逗号，尽量保证句子不生硬截断
                int lastPunc = cleanText.left(80).lastIndexOf(QRegularExpression("[,。，；\\n]"));
                if (lastPunc > 30) {
                    cleanText = cleanText.left(lastPunc + 1) + " ...[详细内容已省略]";
                } else {
                    cleanText = cleanText.left(80) + " ...[详细内容已省略]";
                }
            }
            historyStr += "AI: " + cleanText + "\n";
        }
    }

    return historyStr.isEmpty() ? "无" : historyStr.trimmed();
}


void MainWidget::on_btn_DeleteFile_clicked()
{
    _uploadedFilePath.clear();
    _uploadedFileText.clear();
    ui->labelStatus->setText("🟢 模型已就绪");
}

