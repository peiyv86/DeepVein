#include "atom_fetch_parent.h"

QList<DocChunk> AtomFetchParent::execute(const QList<DocChunk>& inputChunks, int limit = 30) {
    QList<DocChunk> candidateChunks;
    candidateChunks.reserve(limit);
    QSqlDatabase db = Datamanager::getInstance().getThreadLocalConnection();
    if (!db.isOpen()) return candidateChunks;

    QSqlQuery query(db);
    query.prepare("SELECT c.file_path, c.pure_text AS child_text, p.pure_text AS parent_text, p.id AS parent_id "
                  "FROM chunks c JOIN parent_chunks p ON c.parent_id = p.id WHERE c.id = :id");

    int count = 0;
    for (const auto& inputChunk : inputChunks) {
        if (count >= limit) break;

        query.bindValue(":id", inputChunk.chunkId);
        if (query.exec() && query.next()) {
            // 一次拷贝（如果需要修改原值，这是必须的）
            DocChunk chunk = inputChunk;

            chunk.filePath   = query.value("file_path").toString();
            chunk.fileName   = QFileInfo(chunk.filePath).fileName();
            chunk.pureText   = query.value("child_text").toString();
            chunk.parentText = query.value("parent_text").toString();
            chunk.parentId   = query.value("parent_id").toInt();

            // 移动进入列表（零拷贝）
            candidateChunks.append(std::move(chunk));
            ++count;
        }
    }


    return candidateChunks;
}
