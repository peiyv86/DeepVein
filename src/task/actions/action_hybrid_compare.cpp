#include "action_hybrid_compare.h"
#include "task/atoms/atom_hybrid_recall.h"
#include "task/atoms/atom_fetch_parent.h"
#include "task/atoms/atom_rerank_filter.h"
#include "task/atoms/atom_exact_search.h"

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

    // 防漏网兜底：如果大模型犯傻，把后缀名 .txt 扔进了 keywords 里
    QString realTargetFile = intent.targetFileName;
    if (realTargetFile.isEmpty() && !intent.keywords.isEmpty()) {
        for (const QString& kw : intent.keywords) {
            if (kw.contains(".") && kw.length() > 3) { // 简单判定是否像文件名
                realTargetFile = kw;
                break;
            }
        }
    }

    // 核心双轨制路由
    if (!realTargetFile.isEmpty()) {
        // 轨道 A：用户明确指出了目标文件！
        qDebug() << "进入跨文件精准对比，目标文件：" << realTargetFile;
        QList<DocChunk> fileChunks = AtomExactSearch::execute(realTargetFile);

        if (fileChunks.isEmpty()) {
            result.directUIResponse = "在本地库中未找到名为“" + realTargetFile + "”的文件，无法对比。";
            return result;
        }

        // 优化点：不要粗暴地切取前5个，而是用精排算法挑选目标文件中最相关的部分
        if (fileChunks.size() > 5) {
            QString rankQuery = intent.uploadedFileName + " " + intent.keywords.join(" ");
            finalLocalSlices = AtomRerankFilter::execute(rankQuery, fileChunks, 5);

            // 如果精排被全部过滤掉了（说明目标文件内容与当前附件差异极大），兜底取前几个片段
            if (finalLocalSlices.isEmpty()) {
                finalLocalSlices = fileChunks.mid(0, 5);
            }
        } else {
            finalLocalSlices = fileChunks;
        }
    }
    else {
        // 轨道 B：用户未指明文件，走概念盲搜
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

    // 组装最终结果
    for (auto& slice : finalLocalSlices) {
        result.slices.push_back(std::move(slice)); // 完美保留了正确的来源文件名称
    }

    // 放入当前上传附件作为对比基准
    DocChunk attachmentChunk;
    attachmentChunk.chunkId = -1;
    attachmentChunk.fileName = "[当前上传附件] " + intent.uploadedFileName;
    attachmentChunk.filePath = intent.uploadedFilePath;
    attachmentChunk.pureText = intent.uploadedFileText;
    attachmentChunk.score = 1.0f;

    result.slices.push_back(std::move(attachmentChunk));

    return result;
}
