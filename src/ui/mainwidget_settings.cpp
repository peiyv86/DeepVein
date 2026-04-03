#include "mainwidget.h"
#include "ui_mainwidget.h"
#include "core/excepthandler.h"

/**
 * @brief 切换设置页面与对话页面
 */
void MainWidget::on_btnSettings_clicked()
{
    if (settingChanger) {
        ui->stackedWidget->setCurrentIndex(0);
        settingChanger = false;
        return;
    }
    ui->stackedWidget->setCurrentIndex(1);
    settingChanger = true;
    qDebug() << "进入设置页面";
}

// 设置界面与后台建库任
/**
 * @brief 触发后台知识库重建流水线
 */
void MainWidget::on_btnRebuildDB_clicked()
{
    QString targetPath = ui->editScanPath->text();
    if (targetPath.isEmpty()) {
        ui->labelStatus->setText(tr("🔴 请先设置扫描路径！"));
        return;
    }

    ui->labelStatus->setText(tr("正在后台重建知识库..."));
    ui->labelStatus->setStyleSheet("color: #409EFF;");
    ui->btnRebuildDB->setEnabled(false);

    QPointer<MainWidget> safeThis = this;

    //ThreadPool 进行异步处理
    ThreadPool::getInstance().addTask([safeThis, targetPath]() {
        try {
            // 在后台线程运行耗时的建库流水线
            BuildIndexTask* task = new BuildIndexTask(targetPath);
            task->run(); // run 内部已对接 ExceptHandler 记录 VectorIndexError
            delete task;

            // 完成后切回主线程更新 UI
            QMetaObject::invokeMethod(safeThis, [safeThis]() {
                if (!safeThis) return;
                safeThis->ui->labelStatus->setText(tr("知识库重建完成！"));
                safeThis->ui->labelStatus->setStyleSheet("");
                safeThis->ui->btnRebuildDB->setEnabled(true);
            }, Qt::QueuedConnection);

        } catch (const std::exception& e) {
            // 捕获任务创建阶段的异常
            ExceptHandler::getInstance().reportError(ErrorCode::DbTransactionError,
                                                     tr("后台建库启动失败: %1").arg(e.what()));
        }
    });
}

//路径选择按钮组
void MainWidget::on_btnBrowseScanPath_clicked() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("选择知识库扫描目录"));
    if (!dir.isEmpty()) { ui->editScanPath->setText(dir); SCANPATH = dir; }
}

void MainWidget::on_btnBrowseVectorPath_clicked() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("选择向量库存储目录"));
    if (!dir.isEmpty()) { ui->editVectorPath->setText(dir); VECTORPATH = dir; }
}

void MainWidget::on_btnBrowseHistoryPath_clicked() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("选择数据库存放目录"));
    if (!dir.isEmpty()) { ui->editHistoryPath->setText(dir); DIALOGPATH = dir; }
}

/**
 * @brief 设置 Ollama API 端口
 */
void MainWidget::on_btnSetPort_clicked() {
    QString port = ui->editPort->text();
    bool ok;
    int portNum = port.toInt(&ok);
    if (ok && portNum > 0) {
        Aiclient::getInstance().setPORT(port);
        ui->labelStatus->setText(tr("🟢 端口已设置为: ") + port);
    } else {
        ui->labelStatus->setText(tr("🔴 端口号无效"));
        // 这种输入校验错误通常不需要记录到“黑匣子”日志，仅 UI 提示即可
    }
}

void MainWidget::on_btnRefreshModels_clicked()
{
    ui->labelStatus->setText(tr("🔄 正在刷新模型列表..."));
    refreshModels();
}

/**
 * @brief 刷新本地模型列表并处理 Ollama 未启动的情况
 */
void MainWidget::refreshModels()
{
    ui->btnRefreshModels->setEnabled(false);
    QStringList localModels = Aiclient::getInstance().getLocalModels();
    ui->comboModel->clear();

    if (localModels.isEmpty()) {
        ui->comboModel->addItem(tr("未检测到模型 (请启动 Ollama)"));
        ui->comboModel->setEnabled(false);
        ui->labelStatus->setText(tr("🔴 等待 Ollama 启动..."));

        // 记录到异常处理器，方便排查连接问题
        ExceptHandler::getInstance().reportError(ErrorCode::NetworkTimeout, "无法连接到 Ollama 服务，请检查后端状态。");

        if (!ollamaCheckTimer->isActive()) ollamaCheckTimer->start(5000);
    } else {
        ui->comboModel->addItems(localModels);
        ui->comboModel->setEnabled(true);
        ui->labelStatus->setText(tr("🟢 模型已就绪"));
        if (ollamaCheckTimer->isActive()) ollamaCheckTimer->stop();
    }
    ui->btnRefreshModels->setEnabled(true);
}

/**
 * @brief 切换系统语言并加载 .qm 翻译文件
 */
void MainWidget::on_comboLanguage_currentIndexChanged(int index)
{
    QString langCode = ui->comboLanguage->itemData(index).toString();
    QString qmFilePath = QString(":/i18n/lang_%1.qm").arg(langCode);

    if (m_translator.load(qmFilePath)) {
        qApp->installTranslator(&m_translator);
        qDebug() << "语言包加载成功:" << qmFilePath;
    } else {
        ExceptHandler::getInstance().reportError(ErrorCode::FileNotFound,
                                                 tr("无法加载语言包文件: %1").arg(qmFilePath));
    }
}

/**
 * @brief 核心机制：在切换语言时保护并恢复 UI 动态数据
 */
void MainWidget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {

        // 保护：暂存真实的模型列表和当前选中的模型
        QString currentModel = ui->comboModel->currentText();
        QStringList realModels;
        for (int i = 0; i < ui->comboModel->count(); ++i) {
            realModels << ui->comboModel->itemText(i);
        }
        int currentLangIndex = ui->comboLanguage->currentIndex();

        // 隔离阶段：切断信号，防止数据刷写时触发逻辑重入
        ui->comboModel->blockSignals(true);
        ui->comboLanguage->blockSignals(true);

        // 刷新 Qt Designer 静态文本
        ui->retranslateUi(this);

        // 恢复：重填被 retranslateUi 冲掉的真实数据
        ui->comboLanguage->clear();
        ui->comboLanguage->addItem(tr("简体中文"), "zh_CN");
        ui->comboLanguage->addItem(tr("English"), "en_US");
        if (currentLangIndex >= 0 && currentLangIndex < ui->comboLanguage->count()) {
            ui->comboLanguage->setCurrentIndex(currentLangIndex);
        }

        ui->comboModel->clear();
        ui->comboModel->addItems(realModels);
        int modelIdx = ui->comboModel->findText(currentModel);
        if (modelIdx != -1) ui->comboModel->setCurrentIndex(modelIdx);

        // 就绪：数据恢复，重新接通信号
        ui->comboModel->blockSignals(false);
        ui->comboLanguage->blockSignals(false);

        retranslateDynamicUi();
    }
    QWidget::changeEvent(event);
}

/**
 * @brief 刷新代码中手写的动态 UI 文本
 */
void MainWidget::retranslateDynamicUi()
{
    if (m_uploadedFilePath.isEmpty()) {
        ui->labelStatus->setText(tr("🟢 模型已就绪"));
    } else {
        ui->labelStatus->setText(tr("已挂载附件: ") + QFileInfo(m_uploadedFilePath).fileName());
    }

    ui->btnToggleSidebar->setText(m_isSidebarExpanded ? "<<<" : ">");
}
