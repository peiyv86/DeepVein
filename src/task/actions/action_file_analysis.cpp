#include "action_file_analysis.h"

TaskResult ActionFileAnalysis::execute(const ParsedIntent& intent)
{
    TaskResult result;
    result.aim = intent;
    result.success = true;

    // 1. 异常拦截：如果因为某种不可预期的 Bug 导致没拿到文件文本
    if (intent.uploadedFileText.isEmpty()) {
        result.directUIResponse = "未能读取到附件内容，可能是文件为空或解析失败。";
        return result;
    }

    qDebug() << "执行单文件洞察 (DocumentInsight)，目标附件：" << intent.uploadedFileName;

    // 2. 伪造一个完美的满分命中切片
    DocChunk attachmentChunk;
    attachmentChunk.chunkId = -1; // -1 是一个特殊标记，代表这是临时文件，不在 SQLite 库中
    attachmentChunk.fileName = intent.uploadedFileName;
    attachmentChunk.filePath = intent.uploadedFilePath;

    // 这里因为 intent 传进来的是 const 引用，所以只能 copy。
    // 如果后续想要极致优化，可以修改 ITaskRouter 接口把 intent 改为传值并内部 move。
    attachmentChunk.pureText = intent.uploadedFileText;
    attachmentChunk.score = 1.0f; // 满分置顶

    // 3. 移入切片集合
    result.slices.emplace_back(std::move(attachmentChunk));

    return result;
}
