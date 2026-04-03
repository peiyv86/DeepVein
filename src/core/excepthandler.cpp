#include "excepthandler.h"
#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QCoreApplication>
#include <QTextStream>
#include <QMutexLocker>

ExceptHandler::ExceptHandler(QObject *parent) : QObject(parent)
{
    // 初始化日志目录：在可执行文件同级目录下创建 logs 文件夹
    QString appDir = QCoreApplication::applicationDirPath();
    QDir logDir(appDir + "/logs");
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }

    // 设置日志文件路径
    m_logFilePath = logDir.filePath("pdan_error.log");
}

ExceptHandler::~ExceptHandler()
{
}

// 错误码映射
QString ExceptHandler::errorCodeToString(ErrorCode code)
{
    switch (code) {
    case ErrorCode::Success:             return "Success";
    case ErrorCode::NetworkTimeout:      return "NetworkTimeout";
    case ErrorCode::LlmParseFailed:      return "LlmParseFailed";
    case ErrorCode::DbTransactionError:  return "DbTransactionError";
    case ErrorCode::VectorIndexError:    return "VectorIndexError";
    case ErrorCode::FileNotFound:        return "FileNotFound";
    case ErrorCode::DatabaseInitFailed:  return "DatabaseInitFailed";
    case ErrorCode::DatabaseQueryFailed: return "DatabaseQueryFailed";
    default:                             return "UnknownError";
    }
}

void ExceptHandler::reportError(ErrorCode code, const QString& message)
{
    // 组装标准化的日志文本
    QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString codeStr = errorCodeToString(code);
    QString logMessage = QString("[%1] [ERROR: %2] %3").arg(timeStr, codeStr, message);

    // 终端/控制台输出
    qWarning().noquote() << "[WORNING]" << logMessage;

    // 线程安全地写入本地日志文件
    {
        // 加锁即使 ThreadPool 中有 10 个线程同时崩溃，日志也能按顺序安全写入
        QMutexLocker locker(&m_mutex);
        QFile file(m_logFilePath);

        // 以追加和纯文本模式打开文件
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&file);
            out << logMessage << "\n";
            file.close();
        } else {
            qWarning() << "无法打开日志文件写入异常信息:" << m_logFilePath;
        }
    }

    // 将原始信号广播出去，通知UI进行弹窗提示或状态拦截
    emit errorOccurred(code, message);
}
