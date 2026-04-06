#ifndef ACTION_LIST_SEARCH_H   // 🚀 1. 改为专属的宏
#define ACTION_LIST_SEARCH_H   // 🚀 2. 改为专属的宏

#include"task/ITaskrouter.h"

class ActionListCrossSearch: public ITaskRouter
{
public:
    ActionListCrossSearch() = default;
    ~ActionListCrossSearch() override = default;
    TaskResult execute(const ParsedIntent& intent) override;
};

#endif // ACTION_LIST_SEARCH_H //
