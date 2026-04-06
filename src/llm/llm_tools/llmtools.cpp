#include "llmtools.h"
#include "core/excepthandler.h"
#include <QStringView>
#include <QFileInfo>
#include <algorithm>

namespace LlmTools {

/**
 * @brief 解析大模型路由返回的 JSON，提取意图与关键词
 */
ParsedIntent parsedJson(const nlohmann::json& j)
{
    ParsedIntent out;
    out.intentType = IntentType::SemanticSearch; //都找不到设为默认

    try {
        std::string contentJ;
        //两个字段都可能有内容
        if (j.contains("response") && j["response"].is_string()) {
            contentJ = j["response"].get<std::string>();
        }
        else if (j.contains("message") && j["message"].is_object()) {
            auto msg = j["message"];
            if (msg.contains("content") && msg["content"].is_string()) {
                contentJ = msg["content"].get<std::string>();
            }
        }

        if (contentJ.empty()) {
            ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed, "LLM 返回的 JSON 核心字段为空");
            return out;
        }

        QString cleanJ = QString::fromStdString(contentJ).trimmed();

        // 剥离think
        int thinkEnd = cleanJ.indexOf("</think>");
        if (thinkEnd != -1) {
            cleanJ = cleanJ.mid(thinkEnd + 8).trimmed();
        }

        // 提取{}结构，免疫 Markdown 标记
        int jStart = cleanJ.indexOf('{');
        int jEnd = cleanJ.lastIndexOf('}');

        // 找到了开头但没找到结尾（截断情况）
        if (jStart != -1 && jEnd == -1) {
            cleanJ = cleanJ.mid(jStart); // 从开头截取到最后
            ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed, "警告：检测到 JSON 可能被截断，尝试暴力修补");
        }
        else if (jStart != -1 && jEnd != -1 && jEnd >= jStart) {
            cleanJ = cleanJ.mid(jStart, jEnd - jStart + 1);
        } else {
            ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed, "无法在大模型回复中定位到有效 JSON 结构");
            return out;
        }

        //记录清洗后的数据
        qDebug() << "[1]路由JSON待解析=\n" << cleanJ;

        nlohmann::json txtJ;
        try {
            txtJ = nlohmann::json::parse(cleanJ.toStdString());
        } catch (const nlohmann::json::parse_error& e) {
            qWarning() << "标准 JSON 解析失败，启动暴力括号补全机制...";

            //补齐丢失的右括号
            int openBraces = cleanJ.count('{');
            int closeBraces = cleanJ.count('}');
            int openBrackets = cleanJ.count('[');
            int closeBrackets = cleanJ.count(']');
            int quotes = cleanJ.count('"');

            //如果引号是单数说明字符串没闭合-补一个引号
            if (quotes % 2 != 0) cleanJ += "\"";

            // 补齐数组和对象括号 先闭合数组再闭合对象
            while (closeBrackets < openBrackets) { cleanJ += "]"; closeBrackets++; }
            while (closeBraces < openBraces) { cleanJ += "}"; closeBraces++; }

            try {
                txtJ = nlohmann::json::parse(cleanJ.toStdString());
                qDebug() << "补全成功-修复后的JSON:\n" << cleanJ;
            } catch (...) {
                ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed, "JSON无法修补");
                return out;
            }
        }

        //提取意图
        QString intentStr = "semantic_search";
        if (txtJ.contains("intent") && txtJ["intent"].is_string()) {
            intentStr = QString::fromStdString(txtJ["intent"].get<std::string>());
        }
        out.intentType = getIntentEnum(intentStr);

        //参数解析
        if (txtJ.contains("params") && txtJ["params"].is_object()) {
            auto params = txtJ["params"];

            // 修复只要有 filename 或 target_filename，就优先提取为 targetFileName
            if (params.contains("filename") && params["filename"].is_string()) {
                out.targetFileName = QString::fromStdString(params["filename"].get<std::string>());
            }
            if (params.contains("target_filename") && params["target_filename"].is_string()) {
                out.targetFileName = QString::fromStdString(params["target_filename"].get<std::string>());
            }

            // 搜索类分支 (正常提取 keywords)
            if (intentStr == "semantic_search" || intentStr == "hybrid_compare" || intentStr == "list_cross_search") {
                if (params.contains("keywords") && params["keywords"].is_array()) {
                    for (const auto& kw : params["keywords"]) {
                        if (kw.is_string()) out.keywords << QString::fromStdString(kw.get<std::string>());
                    }
                }
                else if (params.contains("query") && params["query"].is_string()) {
                    out.keywords << QString::fromStdString(params["query"].get<std::string>());
                }

                if (params.contains("hyde_text") && params["hyde_text"].is_string()) {
                    out.hydeText = QString::fromStdString(params["hyde_text"].get<std::string>());
                }
            }
        }

        //API幻觉处理
        else if (intentStr == "semantic_search") {
            ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed, "警告：路由 JSON 丢失 params 字段，启动强制提取兜底");
            for (auto it = txtJ.begin(); it != txtJ.end(); ++it) {
                if (it.value().is_string()) {
                    QString val = QString::fromStdString(it.value().get<std::string>());
                    if (!val.contains("http") && val.length() < 20) out.keywords << val;
                }
            }
        }

        qDebug() << "[2]最终关键词清单=" << out.keywords;

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
        if (!result.slices.empty()) {
            prompt += result.slices[0].pureText;
        } else {
            prompt += "(警告：未提取到有效文本)";
        }
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
 * @brief 解析流式返回的 JSON 行（引入状态机，精准处理推理模型）
 */
QList<StreamChunk> processStreamLine(const QByteArray& line, StreamState& state) {
    QList<StreamChunk> chunks;
    if (line.isEmpty()) return chunks;

    try {
        auto j = nlohmann::json::parse(line.toStdString());

        QString responseText;
        if (j.contains("response") && !j["response"].is_null()) {
            responseText = QString::fromStdString(j["response"].get<std::string>());
        }

        QString thinkingText;
        if (j.contains("thinking") && !j["thinking"].is_null()) {
            thinkingText = QString::fromStdString(j["thinking"].get<std::string>());
        }

        // 状态机切分
        if (!thinkingText.isEmpty()) {
            if (!state.isThinking) {
                // 第一次进入思考状态，补上唯一的开标签
                state.isThinking = true;
                chunks.append({"<think>" + thinkingText, ChunkType::NormalText});
            } else {
                // 持续思考中-直接追加纯文本，不包标签
                chunks.append({thinkingText, ChunkType::NormalText});
            }
        } else if (!responseText.isEmpty()) {
            if (state.isThinking) {
                // 状态-思考结束，开始输出正文，补上唯一的闭合标签,换行
                state.isThinking = false;
                chunks.append({"</think>\n\n" + responseText, ChunkType::NormalText});
            } else {
                // 持续输出正文-直接追加
                chunks.append({responseText, ChunkType::NormalText});
            }
        }
    } catch (...) {
        //忽略
    }
    return chunks;
}

/**
 * @brief 解析 Agent 工作流计划
 */
QList<WorkflowStep> parseWorkflowPlan(const QString& contentJ)
{
    QList<WorkflowStep> plan;
    if (contentJ.isEmpty()) return plan;

    try {
        //零拷贝清洗头尾
        QStringView view(contentJ);
        view = view.trimmed();
        if (view.startsWith(QLatin1String("```json"))) view = view.mid(7);
        else if (view.startsWith(QLatin1String("```"))) view = view.mid(3);
        if (view.endsWith(QLatin1String("```"))) view = view.chopped(3);

        //寻找边界
        int start = view.indexOf('[');
        int end = view.lastIndexOf(']');
        if (start != -1 && end != -1 && end >= start) {
            view = view.mid(start, end - start + 1);
        }

        //JSON指针解析
        QByteArray utf = view.toUtf8();
        nlohmann::json rootJ = nlohmann::json::parse(utf.constData(), utf.constData() + utf.size());

        nlohmann::json arrayJ;
        if (rootJ.is_array()) arrayJ = std::move(rootJ);
        else if (rootJ.is_object()) {
            for (auto it = rootJ.begin(); it != rootJ.end(); ++it) {
                if (it.value().is_array()) { arrayJ = std::move(it.value()); break; }
            }
        }

        if (!arrayJ.is_array()) throw std::runtime_error("JSON 结构中不包含工作流数组");

        plan.reserve(arrayJ.size());
        for (const auto& stepJ : arrayJ) {
            if (!stepJ.is_object()) continue;
            WorkflowStep step;
            if (stepJ.contains("stepId")) step.stepId = stepJ["stepId"].get<int>();
            if (stepJ.contains("actionName")) step.actionName = QString::fromStdString(stepJ["actionName"].get<std::string>());
            if (stepJ.contains("description")) step.description = QString::fromStdString(stepJ["description"].get<std::string>());

            if (stepJ.contains("params") && stepJ["params"].is_object()) {
                for (auto it = stepJ["params"].begin(); it != stepJ["params"].end(); ++it) {
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
