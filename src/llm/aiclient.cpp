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
    QString urlStr = QString("http://localhost:%1/api/tags").arg(port);
    QNetworkRequest request((QUrl(urlStr)));

    QNetworkReply* reply = networkManager->get(request);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    timer.start(2000); // 2秒检测
    loop.exec();

    QStringList modelNames;
    if (!timer.isActive()) {
        ExceptHandler::getInstance().reportError(ErrorCode::NetworkTimeout, "获取模型列表超时，Ollama 可能未启动或端口被占用");
        reply->abort();
    }
    else if (reply->error() == QNetworkReply::NoError) {
        try {
            QByteArray data = reply->readAll();
            //零拷贝解析
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
                                                 "连接 Ollama 失败: " + reply->errorString());
    }

    reply->deleteLater();
    return modelNames;
}

/**
 * @brief 异步获取意图路由关键词
 */
void Aiclient::getKeyword(const QString& cmd)
{
    QNetworkReply* reply = getConnect(cmd, "generate", false, true);

    QTimer* timer = new QTimer(reply);
    timer->setSingleShot(true);
    timer->start(15000); // 路由判别限时 15 秒

    connect(timer, &QTimer::timeout, reply, [reply]() {
        ExceptHandler::getInstance().reportError(ErrorCode::NetworkTimeout, "意图路由请求超时，正在强制中止...");
        reply->abort();
    });

    QPointer<Aiclient> self = this;
    connect(reply, &QNetworkReply::finished, this, [self, reply, timer]() {
        if (!self) return;
        if (timer->isActive()) timer->stop();

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();

            //防止阻塞 UI 渲染主线程
            ThreadPool::getInstance().addTask([self, data = std::move(data)]() {
                ParsedIntent out;
                try {
                    json j_res = json::parse(data.toStdString());
                    out = LlmTools::parsedJson(j_res);
                } catch (const std::exception& e) {
                    ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed,
                                                             QString("路由回复 JSON 解析崩溃: ") + e.what());
                }

                QMetaObject::invokeMethod(self, [self, out]() {
                    if (self) emit self->keywordParsed(out);
                }, Qt::QueuedConnection);
            });
        }
        else {
            if (reply->error() != QNetworkReply::OperationCanceledError) {
                ExceptHandler::getInstance().reportError(ErrorCode::NetworkTimeout,
                                                         "意图路由通信失败: " + reply->errorString());
            }
            emit self->keywordParsed(ParsedIntent{});
        }
        reply->deleteLater();
    });
}

/**
 * @brief 流式打印回答并根据结果注入参考文件链接
 */
void Aiclient::printAnswerStream(const TaskResult& result, const QString& cmd)
{
    // 保底 如果检索阶段已经给出了直接回应 (如“未找到”)，则不再请求大模型
    if (!result.directUIResponse.isEmpty()) {
        emit answerChunkReceived(result.directUIResponse, 2);
        emit answerFinished();
        return;
    }

    // 生成最终提示词
    QString prompt = LlmTools::generatePrompt(result, cmd);
    if (prompt.isEmpty()) {
        ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed, "系统内部错误：生成的提示词为空");
        return;
    }

    // UI提前渲染参考文件列表
    if (!result.slices.empty()) {
        QSet<QString> uniqueFiles;
        QString linksHtml = "<b style='color: #2c3e50;'>📚 参考本地文件：</b><br>";

        for (const auto& slice : result.slices) {
            if (!uniqueFiles.contains(slice.filePath)) {
                uniqueFiles.insert(slice.filePath);
                QString fileName = slice.fileName.trimmed().isEmpty() ?
                                       QFileInfo(slice.filePath).fileName() : slice.fileName;
                QString fileUrl = QUrl::fromLocalFile(slice.filePath).toString(QUrl::FullyEncoded);
                linksHtml += QString("-> <a href=\"%1\" style=\"color: #0066cc; text-decoration: none;\">%2</a><br>")
                                 .arg(fileUrl, fileName);
            }
        }
        linksHtml += "<br><hr style='border: none; border-top: 1px solid #e0e0e0; margin: 5px 0;'><br>";
        emit answerChunkReceived(linksHtml, 0); // type 0 = HtmlBlock
    }

    //发起流式请求
    QNetworkReply* reply = getConnect(prompt, "generate", true, false);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QPointer<QNetworkReply> safeReply = reply;
    QPointer<Aiclient> self = this;

    connect(&timer, &QTimer::timeout, &loop, [self, &loop]() {
        if (self) ExceptHandler::getInstance().reportError(ErrorCode::NetworkTimeout, "模型首字响应超时，请检查显存占用或 Ollama 负载");
        loop.quit();
    });

    timer.start(100000); // 针对 RAG 长上下文给予 100s 宽容度

    auto buffer = std::make_shared<QByteArray>();
    auto state = std::make_shared<StreamState>();

    connect(reply, &QNetworkReply::readyRead, this, [self, safeReply, buffer, state, &timer]() {
        if (!self || !safeReply) return;
        timer.start(10000); // 只要有数据流，重置间隔超时为 10s

        buffer->append(safeReply->readAll());
        while (buffer->contains('\n')) {
            int idx = buffer->indexOf('\n');
            QByteArray line = buffer->left(idx);
            buffer->remove(0, idx + 1);

            auto parsedChunks = LlmTools::processStreamLine(line, *state);
            for (const auto& c : parsedChunks) {
                emit self->answerChunkReceived(c.text, static_cast<int>(c.type));
            }
        }
    });

    connect(reply, &QNetworkReply::finished, &loop, [self, safeReply, &loop]() {
        if (self && safeReply && safeReply->error() != QNetworkReply::NoError) {
            if (safeReply->error() != QNetworkReply::OperationCanceledError) {
                ExceptHandler::getInstance().reportError(ErrorCode::NetworkTimeout,
                                                         "生成过程中断: " + safeReply->errorString());
            }
        }
        loop.quit();
    });

    loop.exec();

    if (timer.isActive()) timer.stop();
    emit answerFinished();
    if (reply) reply->deleteLater();
}

/**
 * @brief 构造 HTTP 请求辅助函数
 */
QNetworkReply* Aiclient::getConnect(const QString& prompt, const QString& api, bool stream, bool requireJson)
{
    QString url = "http://localhost:%1/api/%2";
    QNetworkRequest request(QUrl(url.arg(port).arg(api)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    json j_payload;
    j_payload["model"] = llmname.toStdString();
    j_payload["prompt"] = prompt.toStdString();
    j_payload["stream"] = stream;

    // 防止长文本 RAG 导致默认 2048 上下文截断
    json options;
    options["num_ctx"] = 4096;
    j_payload["options"] = options;

    if (requireJson) j_payload["format"] = "json";

    return networkManager->post(request, QByteArray::fromStdString(j_payload.dump()));
}

/**
 * @brief 线程安全且阻塞式的 LLM 请求 (常用于 Agent 实体提取任务)
 */
QString Aiclient::generateBlocking(const QString& prompt, bool requireJson)
{
    // 子线程必须使用局部的网络管理器，不可跨线程竞争 this->networkManager
    QNetworkAccessManager localManager;

    QString urlStr = QString("http://localhost:%1/api/generate").arg(port);
    QNetworkRequest request((QUrl(urlStr)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    json j_payload;
    j_payload["model"] = llmname.toStdString();
    j_payload["prompt"] = prompt.toStdString();
    j_payload["stream"] = false;

    json options;
    options["num_ctx"] = 4096;
    j_payload["options"] = options;

    if (requireJson) j_payload["format"] = "json";

    QNetworkReply* reply = localManager.post(request, QByteArray::fromStdString(j_payload.dump()));

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    timer.start(45000); // 提取操作宽延至 45 秒
    loop.exec();

    QString resultStr = "";
    if (!timer.isActive()) {
        ExceptHandler::getInstance().reportError(ErrorCode::NetworkTimeout, "阻塞式提取任务超时");
        reply->abort();
    }
    else if (reply->error() == QNetworkReply::NoError) {
        try {
            json j_res = json::parse(reply->readAll().toStdString());
            if (j_res.contains("response")) {
                resultStr = QString::fromStdString(j_res["response"].get<std::string>());
            }
        } catch (const std::exception& e) {
            ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed,
                                                     QString("阻塞式 JSON 解析失败: ") + e.what());
        }
    }
    else {
        ExceptHandler::getInstance().reportError(ErrorCode::NetworkTimeout,
                                                 "阻塞式任务失败: " + reply->errorString());
    }

    reply->deleteLater();
    return resultStr;
}
