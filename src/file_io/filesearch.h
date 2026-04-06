#ifndef FILESEARCH_H
#define FILESEARCH_H

#include "file_io/file_factory.h"
#include <QString>

class FileSearch
{
public:
    FileSearch();
    FileTxt Searcher(const QString& filePath);
};

#endif // FILESEARCH_H
