#ifndef WORKFLOW_ENGINE_H
#define WORKFLOW_ENGINE_H

#include "core/global_defs.h"
#include <functional>
#include <QString>

class WorkflowEngine {
public:
    /**
     * @brief 启动 Agent 工作流引擎 (阻塞运行，必须在 ThreadPool 子线程中调用)
     * @param intent 包含附件信息的意图结构体
     * @param userInput 用户的原始提问
     * @param statusCallback UI 状态回调函数，用于实时刷新界面进度
     * @return 最终组装好的 TaskResult
     */
    static TaskResult execute(const ParsedIntent& intent,
                              const QString& userInput,
                              const QString& historyContext, // 🚀 新增记忆传参
                              std::function<void(const QString&)> statusCallback);
};

#endif // WORKFLOW_ENGINE_H
