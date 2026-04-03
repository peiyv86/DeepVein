#include "llmtools.h"
#include "core/excepthandler.h"
#include <QStringView>
#include <QFileInfo>
#include <algorithm>

namespace LlmTools {

/**
 * @brief 解析大模型路由返回的 JSON，提取意图与关键词
 */
ParsedIntent parsedJson(const nlohmann::json& j_res)
{
    ParsedIntent out;
    out.intentType = IntentType::SemanticSearch; // 默认值

    try {
        std::string rawJsonStr;
        // 1. 多字段提取原始字符串 (兼容不同模型的输出习惯)
        if (j_res.contains("response") && j_res["response"].is_string()) {
            rawJsonStr = j_res["response"].get<std::string>();
        }
        else if (j_res.contains("message") && j_res["message"].is_object()) {
            auto msg = j_res["message"];
            if (msg.contains("content") && msg["content"].is_string()) {
                rawJsonStr = msg["content"].get<std::string>();
            }
        }

        if (rawJsonStr.empty()) {
            ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed, "LLM 返回的 JSON 核心字段为空");
            return out;
        }

        QString cleanStr = QString::fromStdString(rawJsonStr).trimmed();

        // 2. 剥离推理思维链 <think>
        int thinkEnd = cleanStr.indexOf("</think>");
        if (thinkEnd != -1) {
            cleanStr = cleanStr.mid(thinkEnd + 8).trimmed();
        }

        // 3. 物理提取 {} 结构，免疫 Markdown 标记
        int jsonStart = cleanStr.indexOf('{');
        int jsonEnd = cleanStr.lastIndexOf('}');
        if (jsonStart != -1 && jsonEnd != -1 && jsonEnd >= jsonStart) {
            cleanStr = cleanStr.mid(jsonStart, jsonEnd - jsonStart + 1);
        } else {
            ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed, "无法在大模型回复中定位到有效 JSON 结构");
            return out;
        }

        // [调试探针] 记录清洗后的数据
        qDebug() << "[探针1]路由JSON已剥离=\n" << cleanStr;

        nlohmann::json j_routing = nlohmann::json::parse(cleanStr.toStdString());

        // 4. 安全提取意图 (Intent)
        QString intentStr = "semantic_search";
        if (j_routing.contains("intent") && j_routing["intent"].is_string()) {
            intentStr = QString::fromStdString(j_routing["intent"].get<std::string>());
        }
        out.intentType = getIntentEnum(intentStr);

        // 5. 参数解析 (Params)
        if (j_routing.contains("params") && j_routing["params"].is_object()) {
            auto params = j_routing["params"];

            // 精确匹配分支
            if (intentStr == "exact_match" && params.contains("filename") && params["filename"].is_string()) {
                out.targetFileName = QString::fromStdString(params["filename"].get<std::string>());
            }
            // 搜索类分支
            else if (intentStr == "semantic_search" || intentStr == "hybrid_compare" || intentStr == "list_cross_search") {
                // 提取关键词数组
                if (params.contains("keywords") && params["keywords"].is_array()) {
                    for (const auto& kw : params["keywords"]) {
                        if (kw.is_string()) out.keywords << QString::fromStdString(kw.get<std::string>());
                    }
                }
                // 兜底提取 query 字段
                else if (params.contains("query") && params["query"].is_string()) {
                    out.keywords << QString::fromStdString(params["query"].get<std::string>());
                }

                if (params.contains("hyde_text") && params["hyde_text"].is_string()) {
                    out.hydeText = QString::fromStdString(params["hyde_text"].get<std::string>());
                }
            }
        }
        // 6. 终极兜底：API 幻觉处理（野蛮提取模式）
        else if (intentStr == "semantic_search") {
            ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed, "警告：路由 JSON 丢失 params 字段，启动强制提取兜底");
            for (auto it = j_routing.begin(); it != j_routing.end(); ++it) {
                if (it.value().is_string()) {
                    QString val = QString::fromStdString(it.value().get<std::string>());
                    if (!val.contains("http") && val.length() < 20) out.keywords << val;
                }
            }
        }

        qDebug() << "[探针2]最终关键词清单=" << out.keywords;

    } catch (const std::exception& e) {
        ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed, QString("ParsedJson 异常: ") + e.what());
    }
    return out;
}

/**
 * @brief 根据任务结果与用户指令，动态生成发送给 LLM 的最终 Prompt
 */
QString generatePrompt(const TaskResult& result, const QString& cmd)
{
    switch (result.aim.intentType)
    {
    case IntentType::DirectChat:
        return promptForChat.arg(cmd);

    case IntentType::ExactMatch:
    {
        if (result.slices.empty()) return "未找到匹配的文件。";
        QString nameList;
        for (const auto& s : result.slices) nameList += "- " + s.fileName + '\n';
        return promptForExact.arg(result.aim.targetFileName, QString::number(result.slices.size()), nameList);
    }

    case IntentType::SemanticSearch:
    case IntentType::ListCrossSearch:
    {
        if (result.slices.empty()) return "未找到相关参考资料。";

        // 1. 降序重排切片，确保 Reranker 高分在前
        auto sortedSlices = result.slices;
        std::sort(sortedSlices.begin(), sortedSlices.end(), [](const auto& a, const auto& b) {
            return a.score > b.score;
        });

        QString slicesList;
        int count = 0;
        for (const auto& s : sortedSlices) {
            if (count >= 3) break; // 仅取 Top 3 喂给模型
            QString text = s.pureText.trimmed();
            if (text.isEmpty()) text = "(该文件已找到，但无法提取纯文本内容)";

            slicesList += QString("[文献: %1]\n内容: %2\n\n")
                              .arg(QFileInfo(s.filePath).fileName())
                              .arg(text);
            count++;
        }
        return promptForSearch.arg(slicesList, cmd);
    }

    case IntentType::DocumentInsight:
    {
        QString prompt = "你是一个专业的数据分析专家和文档助手。用户已经授权你处理以下这份文件，请你作为助手提供信息服务。\n\n";
        prompt += "### 待处理文档内容：\n";
        prompt += "----------------------\n";
        if (!result.slices.empty()) {
            prompt += result.slices[0].pureText;
        } else {
            prompt += "(警告：未提取到有效文本)";
        }
        prompt += "\n----------------------\n\n";
        prompt += "### 用户具体指令：\n" + cmd + "\n\n";
        prompt += "请根据上述文档内容，给出专业、准确且客观的回答：";

        qDebug() << "组装 DocumentInsight 提示词完毕，长度:" << prompt.length();
        return prompt;
    }

    case IntentType::HybridCompare:
    {
        if (result.slices.empty()) return "对比任务未发现有效参考资料。";
        QString slicesList;
        for (const auto& s : result.slices) {
            slicesList += QString("[文献: %1] 来源: %2\n内容: %3\n\n")
                              .arg(QFileInfo(s.filePath).fileName(), s.filePath, s.pureText.trimmed());
        }
        return PromptForCompare.arg(slicesList, cmd);
    }

    case IntentType::Unknown:
        return promptForUnknown.arg(cmd);
    }
    return {};
}

/**
 * @brief 解析流式返回的 JSON 行
 */
QList<StreamChunk> processStreamLine(const QByteArray& line, StreamState& state) {
    QList<StreamChunk> chunks;
    if (line.isEmpty()) return chunks;

    try {
        auto j = nlohmann::json::parse(line.toStdString());
        if (j.contains("response") && !j["response"].is_null()) {
            QString text = QString::fromStdString(j["response"].get<std::string>());
            if (!text.isEmpty()) chunks.append({text, ChunkType::NormalText});
        }
    } catch (...) {
        // 流式解析通常会有断句，解析失败不报告错误
    }
    return chunks;
}

/**
 * @brief 解析 Agent 工作流计划 (内存优化版)
 */
QList<WorkflowStep> parseWorkflowPlan(const QString& rawJsonStr)
{
    QList<WorkflowStep> plan;
    if (rawJsonStr.isEmpty()) return plan;

    try {
        // 1. 零拷贝清洗头尾
        QStringView view(rawJsonStr);
        view = view.trimmed();
        if (view.startsWith(QLatin1String("```json"))) view = view.mid(7);
        else if (view.startsWith(QLatin1String("```"))) view = view.mid(3);
        if (view.endsWith(QLatin1String("```"))) view = view.chopped(3);

        // 寻找边界
        int start = view.indexOf('[');
        int end = view.lastIndexOf(']');
        if (start != -1 && end != -1 && end >= start) {
            view = view.mid(start, end - start + 1);
        }

        // 2. 指针级 JSON 解析
        QByteArray utf8Bytes = view.toUtf8();
        nlohmann::json j_root = nlohmann::json::parse(utf8Bytes.constData(), utf8Bytes.constData() + utf8Bytes.size());

        nlohmann::json j_array;
        if (j_root.is_array()) j_array = std::move(j_root);
        else if (j_root.is_object()) {
            for (auto it = j_root.begin(); it != j_root.end(); ++it) {
                if (it.value().is_array()) { j_array = std::move(it.value()); break; }
            }
        }

        if (!j_array.is_array()) throw std::runtime_error("JSON 结构中不包含工作流数组");

        plan.reserve(j_array.size());
        for (const auto& j_step : j_array) {
            if (!j_step.is_object()) continue;

            WorkflowStep step;
            if (j_step.contains("stepId")) step.stepId = j_step["stepId"].get<int>();
            if (j_step.contains("actionName")) step.actionName = QString::fromStdString(j_step["actionName"].get<std::string>());
            if (j_step.contains("description")) step.description = QString::fromStdString(j_step["description"].get<std::string>());

            if (j_step.contains("params") && j_step["params"].is_object()) {
                for (auto it = j_step["params"].begin(); it != j_step["params"].end(); ++it) {
                    if (it.value().is_string()) {
                        step.params.insert(QString::fromStdString(it.key()), QString::fromStdString(it.value().get<std::string>()));
                    }
                }
            }
            if (!step.actionName.isEmpty()) plan.append(std::move(step));
        }

    } catch (const std::exception& e) {
        ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed, QString("工作流计划解析失败: ") + e.what());
    }

    return plan;
}

} // namespace LlmTools
