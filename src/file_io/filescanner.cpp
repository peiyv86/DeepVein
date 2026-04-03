#include "filescanner.h"
#include <QDirIterator>

QList<SessionInfo> FileScanner::loadAllChatSessions()
{
    return Datamanager::getInstance().getAllSessions();
}

QStringList FileScanner::scanDirectory(const QString& dirPath)
{
    QStringList filePaths;

    // 递归扫描，自动跳过隐藏文件和系统软链接
    QDirIterator it(dirPath,
                    QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
        filePaths.append(it.next());
    }

    return filePaths;
}
