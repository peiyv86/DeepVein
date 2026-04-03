#ifndef LLMTOOLS_H
#define LLMTOOLS_H

#include <QFileInfo>
#include <QList>
#include <QString>
#include <QByteArray>
#include <algorithm>
#include "core/global_defs.h"
#include "llm/prompt_templates.h"
#include "json/nlohmann_json.hpp"

namespace LlmTools
{
// 将解析的 JSON 转化为意图结构体
ParsedIntent parsedJson(const nlohmann::json& j_res);

// 组装最终的 Prompt
QString generatePrompt(const TaskResult& result, const QString& cmd);



// 纯函数：处理从网络接收到的单行 JSON 数据
QList<StreamChunk> processStreamLine(const QByteArray& line, StreamState& state);
QList<WorkflowStep> parseWorkflowPlan(const QString& rawJsonStr);
}

#endif // LLMTOOLS_H
