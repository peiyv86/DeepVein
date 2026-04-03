#ifndef ATOM_EXACT_SEARCH_H
#define ATOM_EXACT_SEARCH_H

#include "task/ITaskrouter.h"

class AtomExactSearch {
public:
    // 输入目标文件名，直接返回匹配的切片集合
    static QList<DocChunk> execute(const QString& targetFileName);
};

#endif // ATOM_EXACT_SEARCH_H
