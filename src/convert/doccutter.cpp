#include "doccutter.h"

Doccutter::Doccutter() {}

// 原版实现：值传递 适合父切片
QList<DocChunk> Doccutter::splitText(const FileTxt& fileData, int chunkSize, int overlap)
{
    QList<DocChunk> chunks;

    QStringView view(fileData.Text);
    view = view.trimmed();

    int totalLen = view.length();
    if (totalLen == 0) return chunks;

    if (chunkSize <= 0) chunkSize = 300;
    if (overlap < 0 || overlap >= chunkSize) overlap = 50;

    int estimatedChunks = totalLen / (chunkSize - overlap) + 2;
    chunks.reserve(estimatedChunks);

    int start = 0;
    const QChar newlineChar('\n');
    const QChar periodChar(0x3002);

    while (start < totalLen) {
        int old_start = start;
        int end = std::min(start + chunkSize, totalLen);

        if (end < totalLen) {
            int lastNewline = view.lastIndexOf(newlineChar, end);
            if (lastNewline > start + (chunkSize / 2)) {
                end = lastNewline + 1;
            } else {
                int lastPeriod = view.lastIndexOf(periodChar, end);
                if (lastPeriod > start + (chunkSize / 2)) {
                    end = lastPeriod + 1;
                }
            }
        }

        QStringView chunkView = view.mid(start, end - start).trimmed();

        if (!chunkView.isEmpty()) {
            DocChunk chunk;
            chunk.fileName = fileData.fileName;
            chunk.filePath = fileData.filePath;
            chunk.pureText = chunkView.toString();
            chunks.append(chunk);
        }

        if (end >= totalLen) {
            break;
        }

        start = end - overlap;
        if (start <= old_start) {
            start = old_start + 1;
        }
    }

    return chunks;
}

// 高性能内存池版：指针传递 适合海量子切片
QList<DocChunk*> Doccutter::splitText(const FileTxt& fileData, int chunkSize, int overlap, MemoryPool<DocChunk>& pool)
{
    QList<DocChunk*> chunks;

    QStringView view(fileData.Text);
    view = view.trimmed();

    int totalLen = view.length();
    if (totalLen == 0) return chunks;

    if (chunkSize <= 0) chunkSize = 300;
    if (overlap < 0 || overlap >= chunkSize) overlap = 50;

    int estimatedChunks = totalLen / (chunkSize - overlap) + 2;
    chunks.reserve(estimatedChunks);

    int start = 0;
    const QChar newlineChar('\n');
    const QChar periodChar(0x3002);

    while (start < totalLen) {
        int old_start = start;
        int end = std::min(start + chunkSize, totalLen);

        if (end < totalLen) {
            int lastNewline = view.lastIndexOf(newlineChar, end);
            if (lastNewline > start + (chunkSize / 2)) {
                end = lastNewline + 1;
            } else {
                int lastPeriod = view.lastIndexOf(periodChar, end);
                if (lastPeriod > start + (chunkSize / 2)) {
                    end = lastPeriod + 1;
                }
            }
        }

        QStringView chunkView = view.mid(start, end - start).trimmed();

        if (!chunkView.isEmpty()) {
            // 不再在栈上创建，而是向内存池索要指针
            DocChunk* chunk = pool.allocate();

            chunk->fileName = fileData.fileName;
            chunk->filePath = fileData.filePath;
            chunk->pureText = chunkView.toString();

            // 装填指针进入 List
            chunks.append(chunk);
        }

        if (end >= totalLen) {
            break;
        }

        start = end - overlap;
        if (start <= old_start) {
            start = old_start + 1;
        }
    }

    return chunks;
}
