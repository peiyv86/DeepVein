#ifndef ACTION_SEMANTIC_H
#define ACTION_SEMANTIC_H

#include"task/ITaskrouter.h"
#include"threadpool/threadpool.h"
class ActionSemantic: public ITaskRouter
{
public:
    ActionSemantic() = default;
    ~ActionSemantic() override = default;
    TaskResult execute(const ParsedIntent& intent) override;
};

#endif // ActionSemantic_H
