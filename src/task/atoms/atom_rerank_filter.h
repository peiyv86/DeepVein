#ifndef ATOM_RERANK_FILTER_H
#define ATOM_RERANK_FILTER_H

#include "task/ITaskrouter.h"
#include"threadpool/threadpool.h"

class AtomRerankFilter {
public:
    static QList<DocChunk> execute(const QString& originalQuery,
                                   QList<DocChunk>& candidateChunks,
                                   int finalTopK);
};

#endif // ATOM_RERANK_FILTER_H
