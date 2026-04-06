#include "filesearch.h"

FileSearch::FileSearch() {}

FileTxt FileSearch::Searcher(const QString& filePath)
{
    // 识别类型
    FileType type = FileFactory::detectFileType(filePath);

    // 制造对应的解析器
    auto parser = FileFactory::createRouter(type);

    // 执行解析
    if (parser) {
        return parser->extractText(filePath);
    }

    return FileTxt(); // 兜底空返回
}
