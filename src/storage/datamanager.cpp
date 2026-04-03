#include "datamanager.h"
#include "core/excepthandler.h" // 假设异常处理在这里
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QThread>
#include <QDebug>
#include "convert/doccutter.h"
#include "convert/semanticextract.h"
#include "vector_db.h"

Datamanager::Datamanager() {}

Datamanager::~Datamanager()
{

}

// 动态获取线程专属连接
QSqlDatabase Datamanager::getThreadLocalConnection()
{
    // 1. 如果当前来请求的线程，就是最初始化数据库的主线程
    // 我们直接把主连接给它，完全不用调用危险的 database() 去试探！
    if (QThread::currentThread() == m_mainThread) {
        return QSqlDatabase::database("pdan_sql_connection");
    }

    // 2. 如果是后台子线程，生成它的专属名字
    QString threadIdStr = QString::number(reinterpret_cast<quint64>(QThread::currentThreadId()));
    QString localConnName = "thread_db_" + threadIdStr;

    if (QSqlDatabase::contains(localConnName)) {
        return QSqlDatabase::database(localConnName);
    }
    else {
        // 子线程第一次请求，克隆连接
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", localConnName);
        db.setDatabaseName(m_dbPath);
        db.setConnectOptions("QSQLITE_BUSY_TIMEOUT=5000");

        if (db.open()) {
            QSqlQuery q(db);
            q.exec("PRAGMA journal_mode=WAL;");
            q.exec("PRAGMA synchronous=NORMAL;");
            q.exec("PRAGMA foreign_keys=ON;");
        } else {
            qCritical() << "子线程创建数据库连接失败:" << db.lastError().text();
        }
        return db;
    }
}

// 事务管理接口 (供 BuildIndexTask 等后台任务使用)
void Datamanager::beginTransaction() {
    QSqlDatabase db = getThreadLocalConnection();
    if (db.isOpen()) db.transaction();
}

void Datamanager::commitTransaction() {
    QSqlDatabase db = getThreadLocalConnection();
    if (db.isOpen()) db.commit();
}

void Datamanager::rollbackTransaction() {
    QSqlDatabase db = getThreadLocalConnection();
    if (db.isOpen()) db.rollback();
}

// 初始化与建表
bool Datamanager::init(const QString& dbPath)
{
    m_mainThread = QThread::currentThread();//及主线程
    m_dbPath = dbPath; // 缓存路径供子线程使用

    QSqlDatabase mainDb;
    if (QSqlDatabase::contains("pdan_sql_connection")) {
        mainDb = QSqlDatabase::database("pdan_sql_connection");
    } else {
        mainDb = QSqlDatabase::addDatabase("QSQLITE", "pdan_sql_connection");
    }
    mainDb.setDatabaseName(dbPath);
    mainDb.setConnectOptions("QSQLITE_BUSY_TIMEOUT=5000");

    if (!mainDb.open()) {
        ExceptHandler::getInstance().reportError(ErrorCode::DatabaseInitFailed,
                                                 "无法打开本地数据库: " + mainDb.lastError().text());
        return false;
    }

    QSqlQuery query(mainDb);

    // 性能加速与并发
    query.exec("PRAGMA journal_mode=WAL;");     // 允许多线程同时一写多读
    query.exec("PRAGMA synchronous=NORMAL;");   // 配合 WAL 的极速落盘模式
    query.exec("PRAGMA foreign_keys=ON;");      // 强制开启外键约束，用于级联删除

    // 文档切片表 & FTS5全文检索表
    query.exec("CREATE TABLE IF NOT EXISTS parent_chunks (id INTEGER PRIMARY KEY AUTOINCREMENT, file_path TEXT, pure_text TEXT)");
    query.exec("CREATE TABLE IF NOT EXISTS chunks (id INTEGER PRIMARY KEY AUTOINCREMENT, parent_id INTEGER, file_path TEXT, pure_text TEXT)");
    query.exec("CREATE VIRTUAL TABLE IF NOT EXISTS chunks_fts USING fts5(file_path, pure_text, content='chunks', content_rowid='id', tokenize='unicode61')");
    query.exec("CREATE TRIGGER IF NOT EXISTS chunks_ai AFTER INSERT ON chunks BEGIN INSERT INTO chunks_fts(rowid, file_path, pure_text) VALUES (new.id, new.file_path, new.pure_text); END;");
    query.exec("CREATE TRIGGER IF NOT EXISTS chunks_ad AFTER DELETE ON chunks BEGIN INSERT INTO chunks_fts(chunks_fts, rowid, file_path, pure_text) VALUES('delete', old.id, old.file_path, old.pure_text); END;");

    // 聊天历史表 (修改为带 ON DELETE CASCADE 的外键表)
    query.exec("CREATE TABLE IF NOT EXISTS sessions (session_id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT, create_time TEXT)");
    query.exec("CREATE TABLE IF NOT EXISTS messages ("
               "msg_id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "session_id INTEGER, role TEXT, content TEXT, "
               "FOREIGN KEY(session_id) REFERENCES sessions(session_id) ON DELETE CASCADE)");

    return true;
}


// 知识库写入模块 支持在任意子线程调用
int Datamanager::insertParentChunk(const QString& filePath, const QString& pureText)
{
    QSqlDatabase db = getThreadLocalConnection();
    if (!db.isOpen()) return -1;
    QSqlQuery query(db);
    query.prepare("INSERT INTO parent_chunks (file_path, pure_text) VALUES (:fp, :pt)");
    query.bindValue(":fp", filePath);
    query.bindValue(":pt", pureText);
    if (query.exec()) return query.lastInsertId().toInt();
    return -1;
}

int Datamanager::insertChunk(int parentId, const DocChunk& chunk)
{
    QSqlDatabase db = getThreadLocalConnection();
    if (!db.isOpen()) return -1;
    QSqlQuery query(db);
    query.prepare("INSERT INTO chunks (parent_id, file_path, pure_text) VALUES (:pid, :fp, :pt)");
    query.bindValue(":pid", parentId);
    query.bindValue(":fp", chunk.filePath);
    query.bindValue(":pt", chunk.pureText);
    if (query.exec()) return query.lastInsertId().toInt();
    return -1;
}

//知识库检索模块 支持在任意子线程调用
QList<DocChunk> Datamanager::searchByFileName(const QString& Name)
{
    QList<DocChunk> out;
    QSqlDatabase db = getThreadLocalConnection();
    if (!db.isOpen() || Name.isEmpty()) return out;

    QSqlQuery query(db);
    query.prepare("SELECT id, file_path, pure_text FROM chunks WHERE file_path LIKE :name");
    query.bindValue(":name", "%" + Name + "%");
    if (query.exec()) {
        while (query.next()) {
            DocChunk chunk;
            chunk.chunkId = query.value(0).toInt();
            chunk.filePath = query.value(1).toString();
            chunk.pureText = query.value(2).toString();
            out.append(chunk);
        }
    }
    return out;
}

QList<DocChunk> Datamanager::searchByKeyWord(const QStringList& KeyWords)
{
    QList<DocChunk> out;
    QSqlDatabase db = getThreadLocalConnection();
    if (!db.isOpen() || KeyWords.isEmpty()) return out;

    QSqlQuery query(db);
    QString kwString = KeyWords.join(" OR ");

    query.prepare("SELECT rowid, file_path, pure_text, rank FROM chunks_fts WHERE chunks_fts MATCH :kw ORDER BY rank LIMIT 10");
    query.bindValue(":kw", kwString);
    if (query.exec()) {
        while (query.next()) {
            DocChunk chunk;
            chunk.chunkId = query.value(0).toInt();
            chunk.filePath = query.value(1).toString();
            chunk.pureText = query.value(2).toString();
            chunk.score = -query.value(3).toDouble();
            out.append(chunk);
        }
    } else {
        ExceptHandler::getInstance().reportError(ErrorCode::DatabaseQueryFailed, "检索失败: " + query.lastError().text());
    }
    return out;
}

// 聊天历史存取模块
int Datamanager::createSession(const QString& title)
{
    QSqlDatabase db = getThreadLocalConnection();
    if (!db.isOpen()) return -1;
    QSqlQuery query(db);
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");

    query.prepare("INSERT INTO sessions (title, create_time) VALUES (:t, :ct)");
    query.bindValue(":t", title);
    query.bindValue(":ct", currentTime);

    if (query.exec()) return query.lastInsertId().toInt();
    return -1;
}

void Datamanager::saveMessage(int sessionId, const QString& role, const QString& content)
{
    QSqlDatabase db = getThreadLocalConnection();
    if (!db.isOpen()) return;
    QSqlQuery query(db);
    query.prepare("INSERT INTO messages (session_id, role, content) VALUES (:sid, :r, :c)");
    query.bindValue(":sid", sessionId);
    query.bindValue(":r", role);
    query.bindValue(":c", content);
    query.exec();
}

QList<SessionInfo> Datamanager::getAllSessions()
{
    QList<SessionInfo> out;
    QSqlDatabase db = getThreadLocalConnection();
    if (!db.isOpen()) return out;

    QSqlQuery query(db);
    query.exec("SELECT session_id, title, create_time FROM sessions ORDER BY session_id DESC");
    while (query.next()) {
        SessionInfo info;
        info.sessionId = query.value(0).toInt();
        info.title = query.value(1).toString();
        info.createTime = query.value(2).toString();
        out.append(info);
    }
    return out;
}

QList<MessageInfo> Datamanager::getMessagesBySession(int sessionId)
{
    QList<MessageInfo> out;
    QSqlDatabase db = getThreadLocalConnection();
    if (!db.isOpen()) return out;

    QSqlQuery query(db);
    query.prepare("SELECT msg_id, role, content FROM messages WHERE session_id = :sid ORDER BY msg_id ASC");
    query.bindValue(":sid", sessionId);
    if (query.exec()) {
        while (query.next()) {
            MessageInfo info;
            info.msgId = query.value(0).toInt();
            info.role = query.value(1).toString();
            info.content = query.value(2).toString();
            out.append(info);
        }
    }
    return out;
}

bool Datamanager::deleteSession(int sessionId)
{
    QSqlDatabase db = getThreadLocalConnection();
    if (!db.isOpen()) return false;
    QSqlQuery query(db);

    // 利用建表时的 ON DELETE CASCADE 机制，直接删除主表记录即可，
    // SQLite 底层引擎会瞬间自动清空对应的所有 messages，防二次查询
    query.prepare("DELETE FROM sessions WHERE session_id = :sid");
    query.bindValue(":sid", sessionId);
    return query.exec();
}

//重命名
bool Datamanager::renameSession(int sessionId, const QString& newTitle)
{
    QSqlDatabase db = getThreadLocalConnection();
    if (!db.isOpen()) {
        qWarning() << "renameSession: 数据库连接未打开";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("UPDATE sessions SET title = :title WHERE session_id = :sid");
    query.bindValue(":title", newTitle);
    query.bindValue(":sid", sessionId);

    if (!query.exec()) {
        qWarning() << "renameSession 失败:" << query.lastError().text();
        return false;
    }

    // 可选：检查是否确实更新了记录
    if (query.numRowsAffected() == 0) {
        qWarning() << "renameSession: 未找到 session_id =" << sessionId;
        // 根据业务需求决定返回 false 还是 true（操作无错误但无更新）
    }
    return true;
}

/**
 * @brief 接口1：直接传一整个 FileTxt，内部自动完成切片并转交匹配
 */
QList<DocChunk> Datamanager::searchByFile(const FileTxt& fileTxt)
{
    if (!fileTxt.isOpen || fileTxt.Text.isEmpty()) {
        return QList<DocChunk>();
    }

    // 1. 调用切割器，把大文件切成小块（比如 500字一段，50字重叠）
    QList<DocChunk> chunks = Doccutter::getInstance().splitText(fileTxt, 500, 50);

    // 2. 直接委托给下面的重载接口去匹配
    return searchByFile(chunks);
}

/**
 * @brief 接口2：接收切好的 chunks，提取特征并与知识库进行相似度匹配
 */
QList<DocChunk> Datamanager::searchByFile(const QList<DocChunk>& chunks)
{
    QList<DocChunk> finalResults;
    if (chunks.isEmpty()) return finalResults;

    QMap<int, double> rrfScores;
    const double k = 60.0;

    int sampleLimit = std::min(5, static_cast<int>(chunks.size()));
    int step = std::max(1, static_cast<int>(chunks.size()) / sampleLimit);

    for (int i = 0; i < chunks.size() && sampleLimit > 0; i += step) {
        const QString& text = chunks[i].pureText;

        // 1. 把切片文本转成向量
        std::vector<float> emb = SemanticExtract::getInstance().getEmbedding(text);
        if (emb.empty()) continue;

        // 2. 去 VectorDB 搜索相似内容
        QList<DocChunk> hits = VectorDB::getInstance().search(emb, 10);

        // 3. RRF 计分累加 (被多段原文同时命中的切片，分数会狂飙)
        int rank = 1;
        for (const auto& hit : hits) {
            rrfScores[hit.chunkId] += 1.0 / (k + rank);
            rank++;
        }
        sampleLimit--;
    }

    // 4. 将累加好的得分排序
    QList<QPair<int, double>> sortedRrf;
    for (auto it = rrfScores.begin(); it != rrfScores.end(); ++it) {
        sortedRrf.append({it.key(), it.value()});
    }
    std::sort(sortedRrf.begin(), sortedRrf.end(), [](const QPair<int, double>& a, const QPair<int, double>& b) {
        return a.second > b.second;
    });

    // 5. 拿着得分最高的 ID，去 SQLite 里查出真实的父段落文本 (复用你 Action 里的优秀联表查询)
    QSqlDatabase db = getThreadLocalConnection();
    if (!db.isOpen()) return finalResults;
    QSqlQuery query(db);
    query.prepare(
        "SELECT c.file_path, p.pure_text AS parent_text, p.id AS parent_id "
        "FROM chunks c "
        "JOIN parent_chunks p ON c.parent_id = p.id "
        "WHERE c.id = :id"
        );

    int recallCount = 0;
    QSet<int> seenParents; // 父节点去重

    for (const auto& pair : sortedRrf) {
        if (recallCount >= 5) break; // 只要黄金 Top 5

        query.bindValue(":id", pair.first);
        if (query.exec() && query.next()) {
            int parentId = query.value("parent_id").toInt();

            // 去重判断
            if (!seenParents.contains(parentId)) {
                seenParents.insert(parentId);

                DocChunk chunk;
                chunk.chunkId = pair.first;
                chunk.filePath = query.value("file_path").toString();
                chunk.fileName = QFileInfo(chunk.filePath).fileName();
                chunk.pureText = query.value("parent_text").toString(); // 直接装载大段落
                chunk.parentId = parentId;
                chunk.score = pair.second;

                finalResults.append(chunk);
                recallCount++;
            }
        }
    }

    return finalResults;
}
