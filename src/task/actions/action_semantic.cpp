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

    // 探针3：看看 FTS5 稀疏和 ONNX 稠密一共捞出了多少条？
    qDebug() << "[探针3] 混合召回的初始切片数量=" << recalledChunks.size();

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
