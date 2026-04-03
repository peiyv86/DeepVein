#ifndef ACTION_SEMANTIC_H
#define ACTION_SEMANTIC_H

#include"task/ITaskrouter.h"
#include"threadpool/threadpool.h"
class ActionSemantic: public ITaskRouter
{
public:
    ActionSemantic() = default;
    ~ActionSemantic() override = default;
    // 使用 override 关键字明确表示这是实现接口中的虚函数
    TaskResult execute(const ParsedIntent& intent) override;
};

#endif // ActionSemantic_H
