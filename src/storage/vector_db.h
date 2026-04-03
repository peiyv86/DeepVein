#ifndef VECTOR_DB_H
#define VECTOR_DB_H

#include <QList>
#include <vector>
#include <QFileInfo>
#include <QDir>

#include "core/excepthandler.h"
#include "core/global_defs.h"
#include "hnswlib/hnswlib.h"

class VectorDB
{
public:
    // 强制单例模式
    static VectorDB& getInstance() {
        static VectorDB instance;
        return instance;
    }

    // 初始化向量库
    bool init(const QString& indexPath, int dim = 1024, int maxElements = 100000);

    // 将内存中的图索引保存到硬盘
    bool saveIndex();

    // 往图里插入一个向量，绑定它在 SQLite 里的 chunkId
    bool addVector(int chunkId, const std::vector<float>& vector);

    // 核心检索：输入目标向量，找出最相似的 Top-K 个切片
    QList<DocChunk> search(const std::vector<float>& queryVector, int topK = 5);

private:
    VectorDB();
    ~VectorDB();
    Q_DISABLE_COPY(VectorDB)

    QString m_indexPath;
    int m_dim;

    void* m_space;
    void* m_alg_hnsw;
};

#endif // VECTOR_DB_H
