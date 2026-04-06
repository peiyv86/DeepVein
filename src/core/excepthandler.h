#ifndef EXCEPTHANDLER_H
#define EXCEPTHANDLER_H

#include <QObject>
#include <QString>
#include <QMutex>
#include <QFile>
#include "core/global_defs.h"

class ExceptHandler : public QObject
{
    Q_OBJECT
public:
    // 强制单例模式：全程序只能有一个异常广播中心
    static ExceptHandler& getInstance() {
        static ExceptHandler instance;
        return instance;
    }

    // 提供给你的模块（底层）调用的抛错接口
    void reportError(ErrorCode code, const QString& message);

signals:
    // 广播信号，UI 层可以连接此信号进行弹窗或状态栏提示
    void errorOccurred(ErrorCode code, const QString& message);

private:
    explicit ExceptHandler(QObject *parent = nullptr);
    ~ExceptHandler();

    // 禁用拷贝
    Q_DISABLE_COPY(ExceptHandler)

    // 辅助工具：将 ErrorCode 转换为直观的字符串名
    QString errorCodeToString(ErrorCode code);

    QMutex _mutex;        // 线程安全锁，防止多线程同时写入日志崩溃
    QString _logFilePath; // 错误日志文件的持久化路径
};

#endif // EXCEPTHANDLER_H
