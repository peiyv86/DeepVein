#include "build_index_task.h"

BuildIndexTask::BuildIndexTask(const QString& dirPath) : m_dirPath(dirPath) {
    setAutoDelete(true); // 线程池执行完毕后自动销毁该任务对象
}

thread_local MemoryPool<DocChunk> tl_pool(1000);

void BuildIndexTask::run() {
    try {
        qDebug() << "开始后台构建知识库流水线 (父子切片模式)...";
        qDebug() << "扫描目录:" << m_dirPath;

        FileScanner scanner;
        QStringList allFiles = scanner.scanDirectory(m_dirPath);

        if (allFiles.isEmpty()) {
            qDebug() << "目录下没有找到支持的文件。";
            return;
        }

        FileSearch searcher;
        Datamanager& dm = Datamanager::getInstance();
        VectorDB& vdb = VectorDB::getInstance();

        // 预估 pendingTasks 可能的最大数量，用于 reserve（粗略估算）
        size_t estimatedTotalChildren = 0;
        for (const QString& filePath : allFiles) {
            // 假设每个文件平均产生约 40 个子切片，可根据实际文件大小调整
            estimatedTotalChildren += 40;
        }

        std::vector<PendingTask> pendingTasks;
        pendingTasks.reserve(estimatedTotalChildren);

        const size_t BATCH_SIZE = 50;   // 批处理阈值
        dm.beginTransaction();          // 开启初始事务

        for (int i = 0; i < allFiles.size(); ++i) {
            const QString& filePath = allFiles[i];
            FileTxt fileData = searcher.Searcher(filePath);

            if (fileData.isOpen && !fileData.Text.isEmpty()) {
                QList<DocChunk> parentChunks = Doccutter::getInstance().splitText(fileData, 1000, 100);

                for (DocChunk& parent : parentChunks) {
                    int parentId = dm.insertParentChunk(parent.filePath, parent.pureText);
                    if (parentId <= 0) continue;

                    FileTxt dummyFile;
                    dummyFile.filePath = parent.filePath;
                    dummyFile.Text = parent.pureText;
                    // 这里虽然是 fileName，也最好给它补齐以防万一
                    dummyFile.fileName = QFileInfo(parent.filePath).fileName();

                    QList<DocChunk*> childChunks = Doccutter::getInstance().splitText(dummyFile, 250, 50, tl_pool);

                    for (DocChunk* child : childChunks) {
                        auto future = ThreadPool::getInstance().addTask([text = child->pureText]() {
                            return SemanticExtract::getInstance().getEmbedding(text);
                        });

                        // 修复：移除 std::move(*child)
                        // 强制进行深拷贝，让 PendingTask 拥有自己独立的数据备份。
                        // 这样即使底层内存池的指针被瞬间回收复用，也绝对不会污染已经排队的任务
                        DocChunk safeCopyChunk = *child;

                        // 加上 std::move()，把这份安全的拷贝所有权转移进队列
                        pendingTasks.emplace_back(std::move(safeCopyChunk), parentId, std::move(future));
                        // 数据已经安全拷贝，现在可以放心地交还给内存池了
                        tl_pool.deallocate(child);

                        // ... 下面的 BATCH_SIZE 满 50 写入逻辑保持不变 ...
                        if (pendingTasks.size() >= BATCH_SIZE) {
                            for (auto& pTask : pendingTasks) {
                                try {
                                    std::vector<float> embedding = pTask.embeddingFuture.get();
                                    if (embedding.empty()) continue;

                                    int childId = dm.insertChunk(pTask.parentId, pTask.chunk);
                                    if (childId > 0) {
                                        vdb.addVector(childId, embedding);
                                    }
                                } catch (const std::exception& e) {
                                    qWarning() << "处理单个切片发生异常:" << e.what();
                                }
                            }
                            dm.commitTransaction();
                            pendingTasks.clear();
                            dm.beginTransaction();
                        }
                    }
                }
            }
        }

        // 收尾工作把最后不足 BATCH_SIZE 的剩余任务处理掉
        if (!pendingTasks.empty()) {
            for (auto& pTask : pendingTasks) {
                try {
                    std::vector<float> embedding = pTask.embeddingFuture.get();
                    if (!embedding.empty()) {
                        int childId = dm.insertChunk(pTask.parentId, pTask.chunk);
                        if (childId > 0) vdb.addVector(childId, embedding);
                    }
                } catch (...) {}
            }
            pendingTasks.clear();
        }

        // 提交最后一波事务无悬挂事务
        dm.commitTransaction();

        vdb.saveIndex();
        qDebug() << "知识库构建任务完成";

    } catch (const std::exception& e) {
        qCritical() << "任务流水线发生致命错误:" << e.what();
    } catch (...) {
        qCritical() << "任务流水线发生未知异常";
    }
}
