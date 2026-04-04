#define RAPID_OCR_DLL_IMPORT
#include "ocrengine.h"
#include "core/excepthandler.h"
#include <opencv2/opencv.hpp>
#include "OcrLite.h"

OcrEngine::OcrEngine() : m_ocrObj(nullptr) {}

OcrEngine::~OcrEngine() {
    if (m_ocrObj) {
        delete static_cast<OcrLite*>(m_ocrObj);
        m_ocrObj = nullptr;
    }
}

bool OcrEngine::init(const QString& modelsDir) {
    try {
        OcrLite* ocr = new OcrLite();

        // 拼接三个模型和一个字典的绝对路径
        std::string detPath = modelsDir.toStdString() + "/ch_PP-OCRv4_det_infer.onnx";
        std::string clsPath = modelsDir.toStdString() + "/ch_PP-OCRv4_cls_infer.onnx";
        std::string recPath = modelsDir.toStdString() + "/ch_PP-OCRv4_rec_infer.onnx";
        std::string keysPath = modelsDir.toStdString() + "/ppocr_keys_v1.txt";

        // 设置线程数等参数并初始化
        ocr->setNumThread(4);
        bool success = ocr->initModels(detPath, clsPath, recPath, keysPath);

        if (!success) {
            ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed, "RapidOCR 模型加载失败，请检查路径。");
            delete ocr;
            return false;
        }

        m_ocrObj = ocr;
        qDebug() << "[RapidOCR] 引擎初始化";
        return true;

    } catch (const std::exception& e) {
        ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed,
                                                 QString("OCR 引擎崩溃: ") + e.what());
        return false;
    }
}

QString OcrEngine::recognizeText(const QImage& image) {
    if (image.isNull() || !m_ocrObj) return "";

    QString extractedText = "";

    try {
        QImage formattedImg = image.convertToFormat(QImage::Format_RGB888);
        cv::Mat mat(formattedImg.height(), formattedImg.width(),
                    CV_8UC3,
                    (void*)formattedImg.bits(),
                    formattedImg.bytesPerLine());

        cv::Mat bgrMat;
        cv::cvtColor(mat, bgrMat, cv::COLOR_RGB2BGR);

        // 动态调整最长边
        int maxSide = std::max(bgrMat.cols, bgrMat.rows);
        if (maxSide < 1024) maxSide = 1024; // 保证下限，不设上限

        qDebug() << "[OCR引擎]接收图片尺寸:" << bgrMat.cols << "*" << bgrMat.rows << " 长边已设为:" << maxSide;

        auto* ocr = static_cast<OcrLite*>(m_ocrObj);
        OcrResult result = ocr->detect(
            bgrMat,
            50,         // padding
            maxSide,
            0.5f,       // boxScoreThresh
            0.3f,       // boxThresh
            1.6f,       // unClipRatio
            true,       // doAngle
            true        // mostAngle
            );

        // 结果提取
        QStringList textBlocks;
        qDebug() << " [OCR引擎]模型抓取" << result.textBlocks.size() << "个文本块";

        for (const auto& block : result.textBlocks) {
            // 放宽分数限制，防止误杀正确文字
            if (block.boxScore > 0.1f) {
                textBlocks.append(QString::fromStdString(block.text));
            }
        }

        extractedText = textBlocks.join("\n");

    } catch (const std::exception& e) {
        ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed,
                                                 QString("图片推理报错: ") + e.what());
    }

    return extractedText.trimmed();
}
