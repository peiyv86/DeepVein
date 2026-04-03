#ifndef ATOM_FETCH_PARENT_H
#define ATOM_FETCH_PARENT_H
#include"task/ITaskrouter.h"
class AtomFetchParent {
public:
    // 输入带 ID 的切片列表，输出包含子文本和父文本的完整切片
    static QList<DocChunk> execute(const QList<DocChunk>& inputChunks, int limit) ;
};

#endif // ATOM_FETCH_PARENT_H
