#ifndef ActionChat_H
#define ActionChat_H

#include"task/ITaskrouter.h"

class ActionChat: public ITaskRouter
{
public:
    ActionChat() = default;
    ~ActionChat() override = default;
    // 使用 override 关键字明确表示这是实现接口中的虚函数
    TaskResult execute(const ParsedIntent& intent) override;
};

#endif // ActionChat_H
