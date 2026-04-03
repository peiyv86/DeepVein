#ifndef RERANKER_ENGINE_H
#define RERANKER_ENGINE_H

#include <QString>
#include <vector>
#include <QDebug>
#include <QFile>
#include <cmath>

class RerankerEngine
{
public:
    static RerankerEngine& getInstance() {
        static RerankerEngine instance;
        return instance;
    }

    bool init(const QString& modelPath, const QString& tokenizerPath);

    // 输入用户问题和文档切片，输出一个绝对相关性得分
    float computeScore(const QString& query, const QString& text);

private:
    RerankerEngine();
    ~RerankerEngine();
    Q_DISABLE_COPY(RerankerEngine)

    void* m_env;
    void* m_sessionOptions;
    void* m_session;
    void* m_memoryInfo;
    void* m_tokenizer;
};

#endif // RERANKER_ENGINE_H
