#ifndef FILESEARCH_H
#define FILESEARCH_H

#include "file_io/file_factory.h"
#include <QString>

// 遵循 C++ 命名规范，首字母大写
class FileSearch
{
public:
    FileSearch();

    // 使用 const 引用传递路径，安全且高效
    FileTxt Searcher(const QString& filePath);
};

#endif // FILESEARCH_H
