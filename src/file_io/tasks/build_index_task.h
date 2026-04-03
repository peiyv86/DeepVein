#ifndef BUILD_INDEX_TASK_H
#define BUILD_INDEX_TASK_H

#include <QRunnable>
#include <QString>
#include <QDebug>
#include <QFileInfo>

#include"core/global_defs.h"
#include "file_io/filescanner.h"
#include "file_io/filesearch.h"
#include "storage/datamanager.h"
#include "storage/vector_db.h"
#include "convert/doccutter.h"
#include "convert/semanticextract.h"
#include "threadpool/threadpool.h"

// 遵循 C++ 命名规范，首字母大写
class BuildIndexTask : public QRunnable
{
public:
    // 构造时传入需要扫描和建库的文件夹绝对路径
    explicit BuildIndexTask(const QString& dirPath);
    ~BuildIndexTask() override = default;

    // QRunnable 必须重写的核心函数，线程池会在后台静默执行它
    void run() override;

private:
    QString m_dirPath;
};

#endif // BUILD_INDEX_TASK_H
