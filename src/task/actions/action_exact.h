#ifndef ACTION_EXACT_H
#define ACTION_EXACT_H

#include"task/ITaskrouter.h"

class ActionExact: public ITaskRouter
{
public:
    ActionExact() = default;
    ~ActionExact() override = default;
    TaskResult execute(const ParsedIntent& intent) override;
};

#endif // ActionExact_H
