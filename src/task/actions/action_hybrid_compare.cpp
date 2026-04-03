#include "action_hybrid_compare.h"
#include "task/atoms/atom_hybrid_recall.h"
#include "task/atoms/atom_fetch_parent.h"
#include "task/atoms/atom_rerank_filter.h"
#include "task/atoms/atom_exact_search.h" // 🚀 别忘了引入精准匹配原子

TaskResult ActionHybridCompare::execute(const ParsedIntent& intent)
{
    TaskResult result;
    result.aim = intent;
    result.success = true;

    if (intent.uploadedFileText.isEmpty()) {
        result.directUIResponse = "未能读取到附件内容，无法进行跨文件对比。";
        return result;
    }

    QList<DocChunk> finalLocalSlices;

    // 🌟 核心双轨制路由
    if (!intent.targetFileName.isEmpty()) {
        // 🚀 轨道 A：用户明确指出了要和哪个文件对比！直接走精准查找，绕开智障的向量打分！
        qDebug() << "进入跨文件精准对比，目标文件：" << intent.targetFileName;
        finalLocalSlices = AtomExactSearch::execute(intent.targetFileName);

        if (finalLocalSlices.isEmpty()) {
            result.directUIResponse = "在本地库中未找到名为“" + intent.targetFileName + "”的文件，无法对比。";
            return result;
        }

        // 防止目标文件太大撑爆 LLM，最多只取前 5 个切片
        if (finalLocalSlices.size() > 5) {
            finalLocalSlices = finalLocalSlices.mid(0, 5);
        }
    }
    else {
        // 🚀 轨道 B：用户对比的是某个概念，走原有的“超级 HyDE 盲搜 + 精排”
        qDebug() << "进入跨文件语义对比，对比概念：" << intent.keywords;
        QString superHydeText = intent.hydeText + "\n" + intent.uploadedFileText;
        QList<DocChunk> recalledChunks = AtomHybridRecall::execute(intent.keywords, superHydeText);

        if (recalledChunks.isEmpty()) {
            result.directUIResponse = "在本地知识库中未找到与您指定的对比目标（" + intent.keywords.join(" ") + "）相关的内容。";
            return result;
        }

        QList<DocChunk> fullChunks = AtomFetchParent::execute(recalledChunks, 30);
        QString rankQuery = "关于 " + intent.keywords.join(" ") + " 的内容";
        finalLocalSlices = AtomRerankFilter::execute(rankQuery, fullChunks, 5);

        if (finalLocalSlices.isEmpty()) {
            result.directUIResponse = "找到了部分本地资料，但在精排比对时发现关联度过低，无法进行有效对比。";
            return result;
        }
    }

    // 🌟 组装最终结果
    for (auto& slice : finalLocalSlices) {
        result.slices.push_back(std::move(slice));
    }

    DocChunk attachmentChunk;
    attachmentChunk.chunkId = -1;
    attachmentChunk.fileName = "[当前上传附件] " + intent.uploadedFileName;
    attachmentChunk.filePath = intent.uploadedFilePath;
    attachmentChunk.pureText = intent.uploadedFileText;
    attachmentChunk.score = 1.0f;

    result.slices.push_back(std::move(attachmentChunk));

    return result;
}
