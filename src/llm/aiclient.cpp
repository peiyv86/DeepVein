#include "aiclient.h"
#include "llm_tools/llmtools.h"
#include "core/excepthandler.h"
#include <QFileInfo>

void Aiclient::setAI(const QString& llmName)
{
    llmname = llmName;
}

void Aiclient::setPORT(const QString& p)
{
    port = p;
}

/**
 * @brief 获取本地 Ollama 模型列表
 */
QStringList Aiclient::getLocalModels()
{
    QNetworkRequest request((QUrl(QString("http://127.0.0.1:%1/api/tags").arg(port))));
    QNetworkReply* rpy = networkManager->get(request);
    QEventLoop lop;
    QTimer time;
    time.setSingleShot(true);
    connect(&time, &QTimer::timeout, &lop, &QEventLoop::quit);
    connect(rpy, &QNetworkReply::finished, &lop, &QEventLoop::quit);

    time.start(2000); // 2秒检测超时
    lop.exec();

    QStringList modelNames;
    if (!time.isActive()) {
        ExceptHandler::getInstance().reportError(ErrorCode::NetworkTimeout, "获取模型列表超时，Ollama 可能未启动或端口被占用");
        rpy->abort();
    }
    else if (rpy->error() == QNetworkReply::NoError) {
        try {
            QByteArray data = rpy->readAll();
            json j_res = json::parse(data.constData(), data.constData() + data.size());
            if (j_res.contains("models")) {
                for (const auto& model : j_res["models"]) {
                    modelNames << QString::fromStdString(model["name"].get<std::string>());
                }
            }
        } catch (const std::exception& e) {
            ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed,
                                                     QString("解析 Ollama 模型列表 JSON 失败: ") + e.what());
        }
    }
    else {
        ExceptHandler::getInstance().reportError(ErrorCode::NetworkTimeout,
                                                 "连接 Ollama 失败: " + rpy->errorString());
    }

    rpy->deleteLater();
    return modelNames;
}

/**
 * @brief 异步获取意图路由关键词
 */
void Aiclient::getKeyword(const QString& cmd)
{
    if (llmname.trimmed().isEmpty()) {
        ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed,
                                                 "意图路由失败：当前未选择大模型，请检查设置面板。");
        emit keywordParsed(ParsedIntent{}); // 返回空意图，防止流程卡死
        return;
    }
    QNetworkReply* rpy = getConnect(cmd, "generate", false, true);
    QPointer<Aiclient> p_ai = this;
    QTimer* time = new QTimer(rpy);
    time->setSingleShot(true);
    time->start(15000); //15 秒

    connect(time, &QTimer::timeout, rpy, [rpy]() {
        ExceptHandler::getInstance().reportError(ErrorCode::NetworkTimeout, "意图路由请求超时，正在强制中止...");
        rpy->abort();
    });

    connect(rpy, &QNetworkReply::finished, this, [p_ai, rpy, time]() {
        if (!p_ai) return;
        if (time->isActive()) time->stop();

        if (rpy->error() == QNetworkReply::NoError) {
            QByteArray data = rpy->readAll();

            // JSON 解析放入线程池，防阻
            ThreadPool::getInstance().addTask([p_ai, data = std::move(data)]() {
                ParsedIntent out;
                try {
                    // 解析 Ollama 外层通讯包
                    json llmJ = json::parse(data.toStdString());

                    // 获取真正的输出内容
                    std::string rawOut = "";
                    if (llmJ.contains("response") && llmJ["response"].is_string()) {
                        rawOut = llmJ["response"].get<std::string>();
                    }

                    // 推理模型保护,response为空时从thinking捞数据
                    if (rawOut.empty() && llmJ.contains("thinking") && llmJ["thinking"].is_string()) {
                        rawOut = llmJ["thinking"].get<std::string>();
                        qDebug() << "触发推理模型保护机制，已从 thinking 字段中捞出数据。";
                    }

                    // 脱掉Markdown代码块
                    QString cleanJ = QString::fromStdString(rawOut);
                    cleanJ.replace("```json", "", Qt::CaseInsensitive);
                    cleanJ.replace("```", "");
                    cleanJ = cleanJ.trimmed();

                    qDebug() << "[路由纯净 JSON]：" << cleanJ;

                    // 伪装标准外壳，LlmTools解析
                    json newJ;
                    newJ["response"] = cleanJ.toStdString();
                    out = LlmTools::parsedJson(newJ);

                } catch (const std::exception& e) {
                    ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed,
                                                             QString("意图解析彻底失败: ") + e.what());
                    out.intentType = IntentType::DirectChat; // 解析失败则强制降级为普通对话
                }

                QMetaObject::invokeMethod(p_ai, [p_ai, out]() {
                    if (p_ai) emit p_ai->keywordParsed(out);
                }, Qt::QueuedConnection);
            });
        }
        else {
            QByteArray e = rpy->readAll();
            qDebug() << "Ollama :" << e;
            if (rpy->error() != QNetworkReply::OperationCanceledError) {
                ExceptHandler::getInstance().reportError(ErrorCode::NetworkTimeout,
                                                         "意图路由通信失败: " + rpy->errorString());
            }
            emit p_ai->keywordParsed(ParsedIntent{});
        }
        rpy->deleteLater();
    });
}

/**
 * @brief 流式打印回答并根据结果
 */
void Aiclient::printAnswerStream(const TaskResult& result, const QString& cmd)
{
    // 检索阶段的直接阻断
    if (!result.directUIResponse.isEmpty()) {
        emit answerChunkReceived(result.directUIResponse, 2);
        emit answerFinished();
        return;
    }
    if (llmname.trimmed().isEmpty()) {
        ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed, "生成回答失败：尚未绑定大模型引擎。");
        emit answerFinished();
        return;
    }

    // 生成提示词
    QString prompt = LlmTools::generatePrompt(result, cmd);
    if (prompt.isEmpty()) {
        ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed, "系统内部错误：生成的提示词为空");
        return;
    }

    // UI渲染参考文件列表
    if (!result.slices.empty()) {
        QSet<QString> fileLists;
        QString html = "<b style='color: #2c3e50;'>📚 参考本地文件：</b><br>";

        for (const auto& s : result.slices) {
            if (!fileLists.contains(s.filePath)) {
                fileLists.insert(s.filePath);
                QString fName = s.fileName.trimmed().isEmpty()?QFileInfo(s.filePath).fileName():s.fileName;
                QString fUrl = QUrl::fromLocalFile(s.filePath).toString(QUrl::FullyEncoded);
                html += QString("-> <a href=\"%1\" style=\"color: #0066cc; text-decoration: none;\">%2</a><br>")
                                 .arg(fUrl, fName);
            }
        }
        html += "<br><hr style='border: none; border-top: 1px solid #e0e0e0; margin: 5px 0;'><br>";
        emit answerChunkReceived(html, 0);
    }

    // 发起流式请求
    QNetworkReply* rpy = getConnect(prompt, "generate", true, false);
    QEventLoop lop;
    QTimer time;
    time.setSingleShot(true);

    QPointer<QNetworkReply> srpy = rpy;
    QPointer<Aiclient> p_ai = this;

    connect(&time, &QTimer::timeout, &lop, [p_ai, &lop]() {
        if (p_ai) ExceptHandler::getInstance().reportError(ErrorCode::NetworkTimeout, "模型首字响应超时，请检查显存占用或 Ollama 负载");
        lop.quit();
    });

    time.start(100000);

    auto buffer = std::make_shared<QByteArray>();
    auto state = std::make_shared<StreamState>();

    connect(rpy, &QNetworkReply::readyRead, this, [&]() {//可能
        if (!p_ai || !srpy) return;
        time.start(10000);

        buffer->append(srpy->readAll());
        while (buffer->contains('\n')) {
            int idx = buffer->indexOf('\n');
            QByteArray line = buffer->left(idx);
            buffer->remove(0, idx + 1);

            auto parsedChunks = LlmTools::processStreamLine(line, *state);
            for (const auto& c : parsedChunks) {
                emit p_ai->answerChunkReceived(c.text, static_cast<int>(c.type));
            }
        }
    });

    connect(rpy, &QNetworkReply::finished, &lop, [p_ai, srpy, &lop]() {
        if (p_ai && srpy && srpy->error() != QNetworkReply::NoError) {
            if (srpy->error() != QNetworkReply::OperationCanceledError) {
                ExceptHandler::getInstance().reportError(ErrorCode::NetworkTimeout,
                                                         "生成过程中断: " + srpy->errorString());
            }
        }
        lop.quit();
    });

    lop.exec();

    if (time.isActive()) time.stop();
    emit answerFinished();
    if (rpy) rpy->deleteLater();
}

/**
 * @brief 构造 HTTP 请求辅助函数
 */
QNetworkReply* Aiclient::getConnect(const QString& prompt, const QString& api, bool stream, bool needJson)
{
    QString url = "http://127.0.0.1:%1/api/%2";
    QNetworkRequest request(QUrl(url.arg(port).arg(api)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    qDebug() << "发起模型请求 -> [" << llmname.trimmed() << "] Api:" << api;
    json j;
    j["model"] = llmname.trimmed().toStdString();
    j["prompt"] = prompt.toStdString();
    j["stream"] = stream;

    json op;
    op["num_ctx"] = 4096;

    // 根据任务类型动态调整模型参数
    if (needJson) {
        j["format"] = "json";
        op["temperature"] = 0.0;
        op["top_p"] = 0.1;
    } else {
        op["temperature"] = 0.3;
        op["repeat_penalty"] = 1.1;
    }

    j["op"] = op;
    return networkManager->post(request, QByteArray::fromStdString(j.dump()));
}

/**
 * @brief 线程安全且阻塞式的 LLM 请求 用于 Agent 实体提取任务
 */
QString Aiclient::generateBlocking(const QString& prompt, bool needJson)
{
    if (llmname.trimmed().isEmpty()) {
        ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed, "阻塞任务失败：大模型名称为空。");
        return "";
    }
    // 子线程必须使用局部的网络管理器不可跨线程竞争全局networkManager
    QNetworkAccessManager localManager;
    QString url = QString("http://127.0.0.1:%1/api/generate").arg(port);
    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    json j;
    json op;
    j["model"] = llmname.toStdString();
    j["prompt"] = prompt.toStdString();
    j["stream"] = false;
    j["op"] = op;
    op["num_ctx"] = 4096;

    if (needJson) {
        op["temperature"] = 0.0;
        op["top_p"] = 0.1;
        op["repeat_penalty"] = 1.1;
        j["format"] = "json";
    } else {
        op["temperature"] = 0.3;
        op["repeat_penalty"] = 1.1;
    }
    QNetworkReply* rpy = localManager.post(request, QByteArray::fromStdString(j.dump()));
    QEventLoop lop;
    QTimer time;
    time.setSingleShot(true);
    connect(&time, &QTimer::timeout, &lop, &QEventLoop::quit);
    connect(rpy, &QNetworkReply::finished, &lop, &QEventLoop::quit);

    time.start(45000); //提取45秒
    lop.exec();

    QString outStr = "";
    if (!time.isActive()) {
        ExceptHandler::getInstance().reportError(ErrorCode::NetworkTimeout, "阻塞式提取任务超时");
        rpy->abort();
    }
    else if (rpy->error() == QNetworkReply::NoError) {
        try {
            // 解Ollama外壳
            json llmJ = json::parse(rpy->readAll().toStdString());
            std::string rawOut = "";// 提取内容
            if (llmJ.contains("response") && llmJ["response"].is_string()) {
                rawOut = llmJ["response"].get<std::string>();
            }
            if (rawOut.empty() && llmJ.contains("thinking") && llmJ["thinking"].is_string()) {
                rawOut = llmJ["thinking"].get<std::string>();
            }

            // 清理Markdown
            QString cleanJ = QString::fromStdString(rawOut);
            cleanJ.replace("```json", "", Qt::CaseInsensitive);
            cleanJ.replace("```", "");
            outStr = cleanJ.trimmed();

        } catch (const std::exception& e) {
            ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed,
                                                     QString("阻塞式 JSON 解析失败: ") + e.what());
        }
    }
    else {
        ExceptHandler::getInstance().reportError(ErrorCode::NetworkTimeout,
                                                 "阻塞式任务失败: " + rpy->errorString());
    }

    rpy->deleteLater();
    return outStr;
}
