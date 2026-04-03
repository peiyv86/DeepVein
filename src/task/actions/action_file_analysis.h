#ifndef ACTION_FILE_ANALYSIS_H
#define ACTION_FILE_ANALYSIS_H

#include "task/ITaskrouter.h"

class ActionFileAnalysis : public ITaskRouter
{
public:
    ActionFileAnalysis() = default;
    ~ActionFileAnalysis() override = default;

    // 实现接口，处理单文件分析意图
    TaskResult execute(const ParsedIntent& intent) override;
};

#endif // ACTION_FILE_ANALYSIS_H
