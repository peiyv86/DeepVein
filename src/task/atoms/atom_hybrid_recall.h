#ifndef ATOM_HYBRID_RECALL_H
#define ATOM_HYBRID_RECALL_H
#include"task/ITaskrouter.h"

class AtomHybridRecall {
public:
    // 输入关键词和 HyDE文本，输出 RRF 融合后的候选切片列表（仅含 ID 和 RRF分数）
    static QList<DocChunk> execute(const QStringList& keywords, const QString& hydeText);
};

#endif // ATOM_HYBRID_RECALL_H
