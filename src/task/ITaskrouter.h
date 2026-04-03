#ifndef ITASKROUTER_H
#define ITASKROUTER_H

#include<QDebug>
#include <QFileInfo>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <algorithm>
#include <QMap>
#include <QSet> // 用于父节点去重

//引入数据存取模块
#include "core/global_defs.h"
#include "convert/semanticextract.h"
#include "convert/reranker_engine.h" // 确保引入了精排引擎
#include "storage/vector_db.h"
#include "storage/datamanager.h"

/**
 * 任务策略抽象基类
 * 所有的task都必须继承此接口并实现 execute 方法
 */
class ITaskRouter {
public:
    /**
     * 核心执行函数
     * intent 包含从 LLM 解析出的意图类型、关键词等参数
     * TaskResult 包含最终要发给 LLM 的切片数据或 UI 提示词
     */
    virtual TaskResult execute(const ParsedIntent& intent) = 0;
    virtual ~ITaskRouter() = default;
};

#endif // ITASKROUTER_H
