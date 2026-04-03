// #include "action_semantic.h"

// TaskResult ActionSemantic::execute(const ParsedIntent& intent)
// {
//     TaskResult result;
//     result.aim = intent;
//     result.success = true;

//     if (intent.keywords.isEmpty()) {
//         result.errorMsg = "大模型未能提取出有效关键词，无法进行检索。";
//         result.success = false;
//         return result;
//     }

//     qDebug() << "双路混合语义检索，关键词：" << intent.keywords;

//     //关键字召回 (SQLite FTS5 稀疏检索)
//     QList<DocChunk> kwChunks = Datamanager::getInstance().searchByKeyWord(intent.keywords);

//     // 向量语义召回 (ONNX BGE 稠密检索)
//     QString queryText = intent.hydeText.isEmpty() ? intent.keywords.join(" ") : intent.hydeText;
//     std::vector<float> queryEmb = SemanticExtract::getInstance().getEmbedding(queryText);

//     QList<DocChunk> vecChunks;
//     if (!queryEmb.empty()) {
//         vecChunks = VectorDB::getInstance().search(queryEmb, 30);
//     }

//     if (kwChunks.isEmpty() && vecChunks.isEmpty()) {
//         result.directUIResponse = "抱歉，我在本地知识库中没有查找到与这些关键词相关的内容。";
//         return result;
//     }

//     // RRF (Reciprocal Rank Fusion) 混合排序
//     QMap<int, double> rrfScores;
//     const double k = 60.0;

//     int rank = 1;
//     for (const auto& chunk : kwChunks) {
//         rrfScores[chunk.chunkId] += 1.0 / (k + rank);
//         rank++;
//     }

//     rank = 1;
//     for (const auto& chunk : vecChunks) {
//         rrfScores[chunk.chunkId] += 1.0 / (k + rank);
//         rank++;
//     }

//     QList<QPair<int, double>> sortedRrf;
//     for (auto it = rrfScores.begin(); it != rrfScores.end(); ++it) {
//         sortedRrf.append({it.key(), it.value()});
//     }
//     std::sort(sortedRrf.begin(), sortedRrf.end(), [](const QPair<int, double>& a, const QPair<int, double>& b) {
//         return a.second > b.second;
//     });

//     // 提取 Top-30 子切片文本，并 JOIN 查出它的父切片大段落
//     QList<DocChunk> candidateChunks;
//     //QSqlDatabase db = QSqlDatabase::database("pdan_sql_connection");
//     QSqlDatabase db = Datamanager::getInstance().getThreadLocalConnection();
//     if (db.isOpen()) {
//         QSqlQuery query(db);
//         // 核心 SQL 修改：联表查询！拿着子切片的 id，查出子切片的文本和父切片的文本
//         query.prepare(
//             "SELECT c.file_path, c.pure_text AS child_text, p.pure_text AS parent_text, p.id AS parent_id "
//             "FROM chunks c "
//             "JOIN parent_chunks p ON c.parent_id = p.id "
//             "WHERE c.id = :id"
//             );

//         int recallCount = 0;
//         for (const auto& pair : sortedRrf) {
//             if (recallCount >= 30) break;

//             query.bindValue(":id", pair.first);
//             if (query.exec() && query.next()) {
//                 DocChunk chunk;
//                 chunk.chunkId = pair.first;
//                 chunk.filePath = query.value("file_path").toString();
//                 chunk.fileName = QFileInfo(chunk.filePath).fileName();

//                 // 子切片文本：用来给 Reranker 打分 (短文本打分快且准)
//                 chunk.pureText = query.value("child_text").toString();
//                 // 父切片文本：这才是最后我们要喂给 LLM 的长篇大论
//                 chunk.parentText = query.value("parent_text").toString();
//                 // 记录 parent_id 用于后期的去重
//                 chunk.parentId = query.value("parent_id").toInt();

//                 candidateChunks.append(chunk);
//                 recallCount++;
//             }
//         }
//     } else {
//         qDebug() << "数据库连接失败，无法补全切片文本！";
//     }

//     if (candidateChunks.isEmpty()) {
//         result.directUIResponse = "找到了一些相关索引，但无法读取文本内容。";
//         return result;
//     }
        //BERT 类

//     // 用【子切片短文本】进行交叉验证
//     qDebug() << "开始进行 Reranker 精排...";
//     QString originalQuery = intent.keywords.join(" ");

//     // 创建一个容器，用于存放所有计算任务的期权future
//     std::vector<std::future<float>> scoreFutures;
//     scoreFutures.reserve(candidateChunks.size());

//     // 将所有打分任务同时派发给线程池
//     for (const auto& chunk : candidateChunks) {
//         // 这里必须按值捕获 originalQuery 和 chunk.pureText，或者创建局部拷贝
//         // 因为如果按引用捕获，循环过快可能导致指针失效或竞争
//         scoreFutures.push_back(ThreadPool::getInstance().addTask([query = originalQuery, text = chunk.pureText]() {
//             // 调用单例的 Reranker 引擎进行推理
//             return RerankerEngine::getInstance().computeScore(query, text);
//         }));
//     }

//     // 3. 阻塞等待所有线程计算完毕，并收集得分
//     for (int i = 0; i < candidateChunks.size(); ++i) {
//         try {
//             // .get() 会等待对应的那个线程算完并返回 float 结果
//             candidateChunks[i].score = scoreFutures[i].get();
//             qDebug() << "精排得分: " << candidateChunks[i].score << " -> " << candidateChunks[i].fileName;
//         } catch (const std::exception& e) {
//             candidateChunks[i].score = -999.0f; // 异常兜底
//             qWarning() << "某个切片精排发生异常:" << e.what();
//         }
//     }

//     // 4. 根据精排最新得分重新排序
//     std::sort(candidateChunks.begin(), candidateChunks.end(), [](const DocChunk& a, const DocChunk& b) {
//         return a.score > b.score;
//     });

//     candidateChunks.erase(
//         std::remove_if(candidateChunks.begin(), candidateChunks.end(),
//                        [](const DocChunk& chunk) {
//                            // 分数异常（-999.0f）或低于阈值则剔除
//                            return chunk.score < 0.25f;
//                        }),
//         candidateChunks.end()
//         );
//     // 在剔除逻辑之后，立刻加上这一段
//     if (candidateChunks.isEmpty()) {
//         result.success = true; // 流程是成功的
//         // 阻止后续调用大模型，直接让 UI 弹出提示
//         result.directUIResponse = "抱歉，我在本地知识库中没有查找到任何与您的提问（" + intent.keywords.join(" ") + "）相关的内容。";
//         return result;
//     }

//     // 提取黄金 Top-5 并【父节点去重】
//     QSet<int> seenParents; // 用于记录已经添加过的父节点
//     const int FinalTopK = 8;

//     for (auto& chunk : candidateChunks) {
//         // 如果已经收满了 5 个切片，直接结束
//         if (result.slices.size() >= FinalTopK) break;

//         // 核心去重：如果这个父亲已经被前面的高分子切片拉取过了，直接跳过！
//         if (seenParents.contains(chunk.parentId)) {
//             qDebug() << "  父节点去重，跳过重复段落 -> 父ID:" << chunk.parentId;
//             continue;
//         }

//         seenParents.insert(chunk.parentId);

//         // 偷天换日：把送给大模型的文本，从短促的子切片替换为完整的父切片！
//         chunk.pureText = chunk.parentText;

//         // 统一使用 append 进行容器插入
//         result.slices.emplace_back(chunk);
//         qDebug() << "最终命中加入 -> 父ID:" << chunk.parentId << " 文件:" << chunk.fileName;
//     }

//     if (result.slices.empty()) {
//         result.directUIResponse = "找到了一些相关内容，但匹配度较低，无法提供准确参考。";
//     }

//     return result;
// }


#include "action_semantic.h"
#include "task/atoms/atom_hybrid_recall.h"
#include "task/atoms/atom_fetch_parent.h"
#include "task/atoms/atom_rerank_filter.h"

TaskResult ActionSemantic::execute(const ParsedIntent& intent)
{
    TaskResult result;
    result.aim = intent;
    result.success = true;

    if (intent.keywords.isEmpty()) {
        result.errorMsg = "大模型未能提取出有效关键词，无法进行检索。";
        result.success = false;
        return result;
    }

    qDebug() << "双路混合语义检索，关键词：" << intent.keywords;

    // 混合召回 (FTS5 + ONNX + RRF)
    QList<DocChunk> recalledChunks = AtomHybridRecall::execute(intent.keywords, intent.hydeText);

    // 🚀 探针3：看看 FTS5 稀疏和 ONNX 稠密一共捞出了多少条？
    qDebug() << "==== [探针3] 混合召回的初始切片数量 ====" << recalledChunks.size();

    if (recalledChunks.isEmpty()) {
        result.directUIResponse = "抱歉，我在本地知识库中没有查找到与这些关键词相关的内容。";
        return result;
    }

    // 数据库联表获取完整父子文本
    QList<DocChunk> fullChunks = AtomFetchParent::execute(recalledChunks, 30);
    if (fullChunks.isEmpty()) {
        result.directUIResponse = "找到了一些相关索引，但无法读取文本内容。";
        return result;
    }

    // 多线程精排打分、过滤与去重
    QString originalQuery = intent.keywords.join(" ");
    QList<DocChunk> finalSlices = AtomRerankFilter::execute(originalQuery, fullChunks, 8);

    if (finalSlices.isEmpty()) {
        // 说明虽然召回了，但在精排阶段因为 score < 0.25 全被斩杀了
        result.directUIResponse = "抱歉，我在本地知识库中没有查找到任何与您的提问（" + originalQuery + "）高度相关的内容。";
        return result;
    }

    // 将 std::vector 兼容现有的 TaskResult
    for(const auto& slice : finalSlices) {
        result.slices.push_back(slice);
    }

    return result;
}
