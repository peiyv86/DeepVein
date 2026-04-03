#ifndef ACTION_HYBRID_COMPARE_H
#define ACTION_HYBRID_COMPARE_H

#include "task/ITaskrouter.h"

class ActionHybridCompare : public ITaskRouter
{
public:
    ActionHybridCompare() = default;
    ~ActionHybridCompare() override = default;

    TaskResult execute(const ParsedIntent& intent) override;
};

#endif // ACTION_HYBRID_COMPARE_H
