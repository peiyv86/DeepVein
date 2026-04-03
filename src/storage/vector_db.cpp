#include "vector_db.h"

VectorDB::VectorDB() : m_space(nullptr), m_alg_hnsw(nullptr), m_dim(1024) {}

/**
 * @brief 析构函数：负责释放 HNSW 算法实例和空间实例占用的内存。
 * 在销毁前会自动调用 saveIndex() 将内存中的向量索引持久化到硬盘。
 */
VectorDB::~VectorDB()
{
    if (m_alg_hnsw) {
        saveIndex(); // 析构时自动保存
        delete static_cast<hnswlib::HierarchicalNSW<float>*>(m_alg_hnsw);
    }
    if (m_space) {
        delete static_cast<hnswlib::L2Space*>(m_space);
    }
}

/**
 * @brief 初始化向量数据库：设置维度并尝试加载或新建索引。
 * 如果指定的索引文件存在，则从硬盘加载；如果不存在，则在内存中创建一个全新的 HNSW 结构。
 * @param indexPath 索引文件保存的路径
 * @param dim 向量维度（需与 Embedding 模型保持一致）
 * @param maxElements 数据库允许存储的最大切片数量
 */
bool VectorDB::init(const QString& indexPath, int dim, int maxElements)
{
    m_indexPath = indexPath;
    m_dim = dim;

    try {
        // 使用 L2 距离空间
        m_space = new hnswlib::L2Space(dim);

        QFileInfo fileInfo(indexPath);
        if (fileInfo.exists())
        {
            m_alg_hnsw = new hnswlib::HierarchicalNSW<float>(
                static_cast<hnswlib::L2Space*>(m_space),
                indexPath.toLocal8Bit().constData(),
                false,
                maxElements
                );
        }
        else
        {
            // 创建全新图索引 (ef_construction=200, M=16 是通用推荐值)
            QDir dir = fileInfo.dir();
            if (!dir.exists()) dir.mkpath(".");

            m_alg_hnsw = new hnswlib::HierarchicalNSW<float>(
                static_cast<hnswlib::L2Space*>(m_space),
                maxElements, 16, 200
                );
        }
    } catch (const std::exception& e){
        ExceptHandler::getInstance().reportError(ErrorCode::VectorIndexError,
                                                 QString("HNSWlib 初始化异常: ") + e.what());
        return false;
    }

    return true;
}

/**
 * @brief 保存索引：将当前内存中的 HNSW 图结构序列化并保存到磁盘上的 .bin 文件。
 * 确保程序下次启动时可以恢复已索引的数据，无需重新解析文件。
 */
bool VectorDB::saveIndex()
{
    if (!m_alg_hnsw) return false;
    try
    {
        auto alg = static_cast<hnswlib::HierarchicalNSW<float>*>(m_alg_hnsw);
        alg->saveIndex(m_indexPath.toLocal8Bit().constData());
        return true;
    } catch (const std::exception& e) {
        ExceptHandler::getInstance().reportError(ErrorCode::VectorIndexError,
                                                 QString("HNSWlib 保存索引失败: ") + e.what());
        return false;
    }
}

/**
 * @brief 插入向量：将一个计算好的特征向量添加到 HNSW 索引中。
 * @param chunkId 对应 SQLite 数据库中的主键 ID，用于检索时反查原文。
 * @param vector 具体的向量数据，其长度必须等于初始化的 m_dim。
 */

bool VectorDB::addVector(int chunkId, const std::vector<float>& vector)
{
    if (!m_alg_hnsw || vector.size() != static_cast<size_t>(m_dim))
    {
        ExceptHandler::getInstance().reportError(ErrorCode::VectorIndexError, "插入向量维度不匹配或未初始化");
        return false;
    }

    try {
        auto alg = static_cast<hnswlib::HierarchicalNSW<float>*>(m_alg_hnsw);

        //工业级动态扩容机制
        if (alg->cur_element_count >= alg->max_elements_ * 0.95) {
            size_t newMax = alg->max_elements_ * 2;
            alg->resizeIndex(newMax);
        }
        alg->addPoint(vector.data(), static_cast<hnswlib::labeltype>(chunkId));
        return true;

    } catch (const std::exception& e) {
        ExceptHandler::getInstance().reportError(ErrorCode::VectorIndexError,
                                                 QString("插入向量失败 (ChunkID: %1): ").arg(chunkId) + e.what());
        return false;
    }
}

/**
 * @brief 向量搜索：执行 K-最近邻 (KNN) 搜索，找出与查询向量最相似的记录。
 * @param queryVector 用户问题的特征向量。
 * @param topK 需要返回的最相关的结果数量。
 * @return QList<DocChunk> 包含 chunkId 和 score 的切片对象列表。
 */
QList<DocChunk> VectorDB::search(const std::vector<float>& queryVector, int topK)
{
    QList<DocChunk> results;
    if (!m_alg_hnsw || queryVector.size() != static_cast<size_t>(m_dim)) return results;

    try {
        auto alg = static_cast<hnswlib::HierarchicalNSW<float>*>(m_alg_hnsw);
        auto pq = alg->searchKnn(queryVector.data(), topK);

        while (!pq.empty()) {
            DocChunk chunk;
            chunk.score = pq.top().first;        // 距离得分
            chunk.chunkId = pq.top().second;     // 绑定的 SQLite 主键
            results.prepend(chunk);              // 前插法，让最相似的排在前面
            pq.pop();
        }
    } catch (const std::exception& e) {
        ExceptHandler::getInstance().reportError(ErrorCode::VectorIndexError,
                                                 QString("向量检索失败: ") + e.what());
    }

    return results;
}
