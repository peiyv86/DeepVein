#ifndef ACTION_LIST_SEARCH_H   // 🚀 1. 改为专属的宏
#define ACTION_LIST_SEARCH_H   // 🚀 2. 改为专属的宏

#include"task/ITaskrouter.h"

class ActionListCrossSearch: public ITaskRouter
{
public:
    ActionListCrossSearch() = default;
    ~ActionListCrossSearch() override = default;
    // 使用 override 关键字明确表示这是实现接口中的虚函数
    TaskResult execute(const ParsedIntent& intent) override;
};

#endif // ACTION_LIST_SEARCH_H // 🚀 3. 对应修改
