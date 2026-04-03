#ifndef SEMANTICEXTRACT_H
#define SEMANTICEXTRACT_H

#include <QString>
#include <vector>
#include <QDebug>
#include <QFile>
#include <cmath>

class SemanticExtract
{
public:
    static SemanticExtract& getInstance() {
        static SemanticExtract instance;
        return instance;
    }
    // 初始化模型和分词器
    bool init(const QString& modelPath, const QString& tokenizerPath);
    // 核心提征接口：输入一段文字，输出高维特征向量 (比如 768 或 1024 维)
    std::vector<float> getEmbedding(const QString& text);
    // 传入一组文本，传回一组对应顺序的向量
    std::vector<std::vector<float>> getEmbeddingBatch(const QList<QString>& texts);

private:
    SemanticExtract();
    ~SemanticExtract();
    Q_DISABLE_COPY(SemanticExtract)

    // L2 归一化：向量检索（Cosine/L2距离）的必要步骤
    void normalizeVector(std::vector<float>& vec);

    // 不透明指针：隐藏 ONNX 和 Tokenizer 的复杂头文件
    void* m_env;
    void* m_sessionOptions;
    void* m_session;
    void* m_memoryInfo;
    void* m_tokenizer; // 指向 tokenizers-cpp 的实例
};

#endif // SEMANTICEXTRACT_H
