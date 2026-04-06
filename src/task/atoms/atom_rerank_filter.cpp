#include "atom_rerank_filter.h"

QList<DocChunk> AtomRerankFilter::execute(const QString& originalQuery,
                               QList<DocChunk>& candidateChunks,
                               int finalTopK)
{
    // 1. 如果输入为空，直接返回空列表
    if (candidateChunks.isEmpty() || originalQuery.isEmpty()) {
        return {};
    }

    qDebug() << "开始进行 Reranker 精排，候选切片数：" << candidateChunks.size();

    // 2. 多线程并行计算每个切片的 Reranker 分数
    std::vector<std::future<float>> scoreFutures;
    scoreFutures.reserve(candidateChunks.size());

    for (const auto& chunk : candidateChunks) {
        // 按值捕获查询文本和切片内容，避免悬空引用
        scoreFutures.push_back(ThreadPool::getInstance().addTask(
            [query = originalQuery, text = chunk.pureText]() {
                return RerankerEngine::getInstance().computeScore(query, text);
            }
            ));
    }

    // 3. 收集分数，并处理异常
    for (size_t i = 0; i < candidateChunks.size(); ++i) {
        try {
            candidateChunks[i].score = scoreFutures[i].get();
            qDebug() << "精排得分:" << candidateChunks[i].score
                     << "->" << candidateChunks[i].fileName;
        } catch (const std::exception& e) {
            candidateChunks[i].score = -999.0f;
            qWarning() << "切片精排异常:" << e.what();
        }
    }

    // 4. 按分数降序排序
    std::sort(candidateChunks.begin(), candidateChunks.end(),
              [](const DocChunk& a, const DocChunk& b) {
                  return a.score > b.score;
              });

    // 5. 剔除低分切片（低于 0.25）
    candidateChunks.erase(
        std::remove_if(candidateChunks.begin(), candidateChunks.end(),
                       [](const DocChunk& chunk) {
                           return chunk.score < 0.25f;
                       }),
        candidateChunks.end()
        );

    // 6. 如果无有效结果，返回空列表（调用方可自行处理提示）
    if (candidateChunks.isEmpty()) {
        qDebug() << "无有效切片（得分低于阈值）";
        return {};
    }

    // 7. 父节点去重 + 替换为完整父文本
    QSet<int> seenParents;
    QList<DocChunk> finalSlices;
    finalSlices.reserve(finalTopK);

    // 在 atom_rerank_filter.cpp 最后的循环中：
    for (auto& chunk : candidateChunks) {
        if (finalSlices.size() >= finalTopK) break;

        if (seenParents.contains(chunk.parentId)) continue;
        seenParents.insert(chunk.parentId);

        // 修复：只有当 parentText 确有内容时才进行替换
        // 否则保留原本命中高分的子切片内容 (pureText)
        if (!chunk.parentText.trimmed().isEmpty()) {
            chunk.pureText = std::move(chunk.parentText);
        }

        // 再次移动：把整个 chunk 的内存所有权直接转移进 finalSlices
        finalSlices.append(std::move(chunk));
    }

    qDebug() << "精排结束，最终返回切片数:" << finalSlices.size();
    return finalSlices;
}
