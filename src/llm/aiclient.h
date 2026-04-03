#ifndef AICLIENT_H
#define AICLIENT_H

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QPointer>
#include <QTimer>
#include <QDebug>
#include <QStringList>
#include <memory>
#include <QApplication> // 如果需要用到 QApplication::processEvents
#include <QUrl>
#include <QFileInfo>

#include"core/excepthandler.h"
#include "llm/prompt_templates.h"
#include "json/nlohmann_json.hpp"
#include"threadpool/threadpool.h"
#include "core/excepthandler.h"
using json = nlohmann::json;

class Aiclient : public QObject
{
    Q_OBJECT
signals:
    //加上 int type = 0
    void answerChunkReceived(const QString& chunk, int type = 0);
    void answerFinished();
    void errorOccurred(const QString&);
    //触发网络请求后立即结束，等网络返回后再发出一个信号
    void keywordParsed(ParsedIntent intent);

public:
    static Aiclient& getInstance() {
        static Aiclient instance;
        return instance;
    }
    //解析用户要求
    void getKeyword(const QString& cmd);
    //打印输出
    void printAnswerStream(const TaskResult& result, const QString& cmd);
    //获取本地大模型
    QStringList getLocalModels();
    //设置ai
    void setAI(const QString& llmName);
    void setPORT(const QString& port);
    // 禁用拷贝
    Aiclient(const Aiclient&) = delete;
    Aiclient& operator=(const Aiclient&) = delete;

    // 专供后台原子操作调用的阻塞式请求接口线程安全
    QString generateBlocking(const QString& prompt, bool requireJson = false);

private:
    Aiclient(QObject* parent = nullptr) : QObject(parent)
    {
        networkManager = new QNetworkAccessManager(this); // 指定父对象，自动释放
    }
    ~Aiclient() override = default; // networkManager 由 Qt 父对象机制自动删除

    //工具函数
    //取得与ollama的连接，并取得回复
    QNetworkReply* getConnect(const QString& prompt,const QString& api,bool stream,bool requireJson = false);
    //私有成员变量
    QNetworkAccessManager* networkManager;
    QString llmname="qwen2.5:1.5b";
    QString port="11434";
};

#endif // AICLIENT_H
