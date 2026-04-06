#include "task/actions/action_list_search.h"
#include "task/atoms/atom_extract_entities.h"
#include "task/atoms/atom_hybrid_recall.h"
#include "task/atoms/atom_fetch_parent.h"
#include "task/atoms/atom_rerank_filter.h"

TaskResult ActionListCrossSearch::execute(const ParsedIntent& intent)
{
    TaskResult result;
    result.aim = intent;
    result.success = true;

    if (intent.uploadedFileText.isEmpty()) {
        result.directUIResponse = "未能读取到名单附件内容，请确保已上传包含名单的文件。";
        return result;
    }

    // 动态提取实体
    QStringList extractedNames = AtomExtractEntities::execute(intent.uploadedFileText, intent.extractTarget);

    if (extractedNames.isEmpty()) {
        result.directUIResponse = QString("我无法从您上传的文件中提取出有效的【%1】。").arg(intent.extractTarget);
        return result;
    }

    qDebug() << "开始按实体拆分召回，提取目标:" << intent.extractTarget << " 列表:" << extractedNames;

    // 拆分循环召回，统一汇总去重
    QList<DocChunk> allRecalledChunks;
    QSet<int> recalledChunkIds;

    for (const QString& name : extractedNames) {
        // 为每个实体单独组装精准搜索词
        QStringList singleTargetKw = intent.keywords;
        singleTargetKw.prepend(name);

        QList<DocChunk> chunksForName = AtomHybridRecall::execute(singleTargetKw, "");

        for (const auto& chunk : chunksForName) {
            if (!recalledChunkIds.contains(chunk.chunkId)) {
                recalledChunkIds.insert(chunk.chunkId);
                allRecalledChunks.append(chunk);
            }
        }
    }

    if (allRecalledChunks.isEmpty()) {
        result.directUIResponse = "根据提取出的实体，在本地库中未找到相关文档。";
        return result;
    }

    // 联表查询补全父文本
    QList<DocChunk> fullChunks = AtomFetchParent::execute(allRecalledChunks, 30);

    // 交叉精排去重 (精排 Query 应该强调核心意图)
    QString rankQuery = intent.extractTarget + "的" + intent.keywords.join(" ");
    QList<DocChunk> finalSlices = AtomRerankFilter::execute(rankQuery, fullChunks, 8);

    if (finalSlices.isEmpty()) {
        result.directUIResponse = "检索到了名单相关的内容，但匹配度过低已被过滤。";
        return result;
    }

    // 封装返回
    for(auto& slice : finalSlices) {
        result.slices.push_back(std::move(slice));
    }

    return result;
}
