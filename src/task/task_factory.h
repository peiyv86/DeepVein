#ifndef TASK_FACTORY_H
#define TASK_FACTORY_H

#include <memory>
#include "itaskrouter.h"
#include "core/global_defs.h"


class TaskFactory {
public:
    // 静态工厂方法，返回独占智能指针，自动管理内存生命周期
    static std::unique_ptr<ITaskRouter> createRouter(IntentType intentType);
};

#endif // TASK_FACTORY_H
