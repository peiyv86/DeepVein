#include "atom_hybrid_recall.h"

QList<DocChunk> AtomHybridRecall::execute(const QStringList& keywords, const QString& hydeText) {
    QList<DocChunk> kwChunks = Datamanager::getInstance().searchByKeyWord(keywords);

    QString queryText = hydeText.isEmpty() ? keywords.join(" ") : hydeText;
    std::vector<float> queryEmb = SemanticExtract::getInstance().getEmbedding(queryText);
    QList<DocChunk> vecChunks;
    if (!queryEmb.empty()) vecChunks = VectorDB::getInstance().search(queryEmb, 30);

    QHash<int, double> rrfScores;
    const double k = 60.0;

    int rank = 1;
    for (const auto& chunk : kwChunks) {
        rrfScores[chunk.chunkId] += 1.0 / (k + rank);
        rank++;
    }

    rank = 1;
    for (const auto& chunk : vecChunks) {
        rrfScores[chunk.chunkId] += 1.0 / (k + rank);
        rank++;
    }

    QList<QPair<int, double>> sortedRrf;
    for (auto it = rrfScores.begin(); it != rrfScores.end(); ++it) {
        sortedRrf.append({it.key(), it.value()});
    }
    std::sort(sortedRrf.begin(), sortedRrf.end(), [](const QPair<int, double>& a, const QPair<int, double>& b) {
        return a.second > b.second;
    });


    QList<DocChunk> recalledChunks;
    for (const auto& pair : sortedRrf) { // 假设 sortedRrf 是 RRF 算出的 QPair<int, double>
        DocChunk chunk;
        chunk.chunkId = pair.first;
        chunk.score = pair.second; // 暂存 RRF 分数
        recalledChunks.append(chunk);
    }
    return recalledChunks;
}
