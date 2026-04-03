#include "workflow_engine.h"
#include "llm/aiclient.h"
#include "llm/prompt_templates.h"
#include "llm/llm_tools/llmtools.h"
#include "task/atoms/atom_extract_entities.h"
#include "task/atoms/atom_hybrid_recall.h"
#include "task/atoms/atom_fetch_parent.h"
#include "task/atoms/atom_rerank_filter.h"
#include <QDebug>
#include <QMap>

// 定义工具函数的标准签名：传入当前步骤信息，并直接修改上下文黑板
using ToolFunction = std::function<void(const WorkflowStep&, WorkflowContext&)>;

// 轻量级工具注册表：初始化所有可用工具
static QMap<QString, ToolFunction> initToolRegistry() {
    QMap<QString, ToolFunction> registry;

    // 注册提取工具
    registry["tool_extract"] = [](const WorkflowStep& step, WorkflowContext& ctx) {
        if (ctx.uploadedFileText.isEmpty()) throw std::runtime_error("缺失必要的附件内容");
        QString target = step.params.value("target", "关键信息");
        ctx.extractedEntities = AtomExtractEntities::execute(ctx.uploadedFileText, target);
    };

    // 注册搜索工具
    registry["tool_search"] = [](const WorkflowStep& step, WorkflowContext& ctx) {
        QString kwStr = step.params.value("keywords", "");
        QStringList searchKws = kwStr.split(" ", Qt::SkipEmptyParts);

        // 依赖断裂兜底
        if (!ctx.extractedEntities.isEmpty()) searchKws.append(ctx.extractedEntities);
        if (searchKws.isEmpty()) searchKws.append(ctx.originalQuery);

        QList<DocChunk> recalled = AtomHybridRecall::execute(searchKws, "");
        if (!recalled.isEmpty()) {
            QList<DocChunk> fullChunks = AtomFetchParent::execute(recalled, 30);
            QString rankQuery = searchKws.join(" ");
            ctx.recalledChunks = AtomRerankFilter::execute(rankQuery, fullChunks, 3);
        }
    };

    // 注册对比工具 (占位，实际由最后的主渲染接管)
    registry["tool_compare"] = [](const WorkflowStep&, WorkflowContext&) {
        qDebug() << "触发比对汇总，准备结束调度";
    };

    return registry;
}

TaskResult WorkflowEngine::execute(const ParsedIntent& intent,
                                   const QString& userInput,
                                   const QString& historyContext,
                                   std::function<void(const QString&)> statusCallback)
{
    TaskResult result;
    result.aim = intent;
    result.success = true;

    // 1. 初始化黑板
    WorkflowContext context;
    context.originalQuery = userInput;
    context.uploadedFileName = intent.uploadedFileName;
    context.uploadedFilePath = intent.uploadedFilePath;
    context.uploadedFileText = intent.uploadedFileText;

    if (statusCallback) statusCallback("🧠 Agent 正在进行全局任务规划...");

    // 2. 大脑规划阶段
    QString statusStr = context.uploadedFileName.isEmpty() ? "当前无附件" : "当前已挂载附件：[" + context.uploadedFileName + "]";

    // 使用新的 arg(状态, 用户输入, 历史记忆)
    QString plannerPrompt = PromptForPlanner.arg(statusStr, userInput, historyContext);
    if (!context.uploadedFileName.isEmpty()) {
        plannerPrompt = "【当前已挂载附件】: " + context.uploadedFileName + "\n" + plannerPrompt;
    }

    QString planJsonStr;
    try {
        planJsonStr = Aiclient::getInstance().generateBlocking(plannerPrompt, true);
    } catch (const std::exception& e) {
        result.directUIResponse = QString("规划引擎通信异常: %1").arg(e.what());
        return result;
    }

    QList<WorkflowStep> plan = LlmTools::parseWorkflowPlan(planJsonStr);
    if (plan.isEmpty()) {
        result.directUIResponse = "未能解析出有效的工作流步骤，请尝试换种方式提问。";
        return result;
    }

    // 获取工具注册表 (只在首次调用时初始化一次)
    static const QMap<QString, ToolFunction> toolRegistry = initToolRegistry();

    // 3. 步进执行循环 (核心改造：查表法替代 if-else)
    for (const auto& step : plan) {
        if (statusCallback) {
            statusCallback(QString("⚙️ [步骤 %1/%2] %3").arg(step.stepId).arg(plan.size()).arg(step.description));
        }

        // 查找工具是否存在
        if (!toolRegistry.contains(step.actionName)) {
            qDebug() << "警告：模型产生了未知工具幻觉:" << step.actionName << "，已跳过。";
            continue;
        }

        // 执行工具并捕获异常 (防止某一个工具炸毁整个工作流)
        try {
            toolRegistry[step.actionName](step, context);

            // 如果遇到 compare 工具，提前结束流程
            if (step.actionName == "tool_compare") break;

        } catch (const std::exception& e) {
            qDebug() << "工具" << step.actionName << "执行异常:" << e.what();
            if (statusCallback) {
                statusCallback(QString("%1 步骤出现异常，尝试跳过...").arg(step.actionName));
            }
            // 非致命错误，选择 continue 继续后续步骤，尽力而为
            continue;
        } catch (...) {
            qDebug() << "工具" << step.actionName << "发生未知崩溃!";
        }
    }

    // 4. 收尾组装阶段 (伪装为 HybridCompare)
    result.aim.intentType = IntentType::HybridCompare;
    for (auto& chunk : context.recalledChunks) {
        result.slices.push_back(std::move(chunk));
    }
    if (!context.uploadedFileText.isEmpty()) {
        DocChunk attachmentChunk;
        attachmentChunk.chunkId = -1;
        attachmentChunk.fileName = "[当前上传附件] " + context.uploadedFileName;
        attachmentChunk.filePath = context.uploadedFilePath;
        attachmentChunk.pureText = context.uploadedFileText;
        attachmentChunk.score = 1.0f;
        result.slices.push_back(std::move(attachmentChunk));
    }

    return result;
}
