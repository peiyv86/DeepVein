#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QList>
#include <QString>
#include <QStringList>
#include "core/global_defs.h"

class Datamanager
{
public:
    static Datamanager& getInstance() {
        static Datamanager instance;
        return instance;
    }

    bool init(const QString& dbPath);

    // 新增：多线程并发与事务控制核心接口
    QSqlDatabase getThreadLocalConnection(); // 获取当前线程的专属连接 (公开给 ActionSemantic 使用)
    void beginTransaction();                 // 开启事务 (公开给 BuildIndexTask 使用)
    void commitTransaction();                // 提交事务
    void rollbackTransaction();              // 回滚事务

    // --- 知识库模块 ---
    int insertParentChunk(const QString& filePath, const QString& pureText);
    int insertChunk(int parentId, const DocChunk& chunk);
    QList<DocChunk> searchByFileName(const QString& Name);
    QList<DocChunk> searchByKeyWord(const QStringList& KeyWords);

    // --- 历史会话模块 ---
    int createSession(const QString& title);
    void saveMessage(int sessionId, const QString& role, const QString& content);
    QList<SessionInfo> getAllSessions();
    QList<MessageInfo> getMessagesBySession(int sessionId);
    bool deleteSession(int sessionId);
    bool renameSession(int sessionId, const QString& newTitle);

    // 接口保留
    QList<DocChunk> searchByFile(const FileTxt& fileTxt);
    QList<DocChunk> searchByFile(const QList<DocChunk>& chunks);
private:
    Datamanager();
    ~Datamanager();
    Datamanager(const Datamanager&) = delete;
    Datamanager& operator=(const Datamanager&) = delete;

    QSqlDatabase sqlDb;   // 主线程连接
    QString m_dbPath;     // 缓存数据库路径，供子线程动态克隆连接时使用

    QThread* m_mainThread = nullptr; // 新增：记录初始化数据库的主线程
};

#endif // DATAMANAGER_H
