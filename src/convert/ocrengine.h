#ifndef OCRENGINE_H
#define OCRENGINE_H

#include <QImage>
#include <QString>
#include <QDebug>
#include <QStringList>

class OcrEngine
{
public:
    static OcrEngine& getInstance() {
        static OcrEngine instance;
        return instance;
    }

    // 初始化模型路径
    bool init(const QString& modelsDir);

    // 核心接口：看图识字
    QString recognizeText(const QImage& image);

private:
    OcrEngine();
    ~OcrEngine();
    Q_DISABLE_COPY(OcrEngine)

    // 不透明指针：隐藏底层的 RapidOCR 对象
    void* m_ocrObj;
};

#endif // OCRENGINE_H
