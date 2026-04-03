#ifndef FILESCANNER_H
#define FILESCANNER_H

#include "storage/datamanager.h"
#include "core/global_defs.h"
#include <QStringList>
#include <QString>

class FileScanner
{
public:
    FileScanner() = default;

    // 1. 获取所有的存档数据 (返回纯数据结构，绝不包含 UI)
    QList<SessionInfo> loadAllChatSessions();

    // 2. 扫描指定目录及其子目录，返回所有支持的文件的绝对路径列表
    QStringList scanDirectory(const QString& dirPath);
};

#endif // FILESCANNER_H
