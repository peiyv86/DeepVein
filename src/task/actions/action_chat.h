#ifndef ActionChat_H
#define ActionChat_H

#include"task/ITaskrouter.h"

class ActionChat: public ITaskRouter
{
public:
    ActionChat() = default;
    ~ActionChat() override = default;
    TaskResult execute(const ParsedIntent& intent) override;
};

#endif // ActionChat_H
