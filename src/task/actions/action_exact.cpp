#include "action_exact.h"

TaskResult ActionExact::execute(const ParsedIntent& intent)
{
    TaskResult result;
    result.aim = intent;
    result.success = true;

    if (intent.targetFileName.isEmpty()) {
        result.success = false;
        result.errorMsg = "解析为精准匹配，但未能提取到目标文件名。";
        return result;
    }
    qDebug() << "执行精准匹配检索，目标文件：" << intent.targetFileName;
    QList<DocChunk> matchedChunks = Datamanager::getInstance().searchByFileName(intent.targetFileName);

    if (matchedChunks.isEmpty()) {
        result.directUIResponse = QString("抱歉，在本地知识库中未找到名为“%1”的文件，请检查文件是否已导入。")
                                      .arg(intent.targetFileName);
        return result;
    }
    result.slices.reserve(matchedChunks.size());
    for (const auto& chunk : matchedChunks) {
        result.slices.emplace_back(chunk);
    }

    qDebug() << "精准匹配成功，共提取该文件的切片数量：" << result.slices.size();

    return result;
}
