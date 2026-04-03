#include "handler_img.h"
#include "core/excepthandler.h"
#include "convert/ocrengine.h" // 引入我们在前面设计的全局 OCR 引擎

#include <QFileInfo>
#include <QImage>
#include <QDebug>

// 接口实现：返回该 Handler 支持的基础类型
FileType HandlerImg::getSupportedType() const {
    return FileType::Image;
}

// 接口：读取图片并执行 OCR
FileTxt HandlerImg::extractText(const QString& filePath)
{
    FileTxt result;
    QFileInfo fileInfo(filePath);

    // 1. 初始化基础信息
    result.filePath = filePath;
    result.fileName = fileInfo.fileName();
    result.typeName = FileType::Image;
    result.isOpen = false;
    result.Text = "";

    // 2. 检查文件物理存在性
    if (!fileInfo.exists()) {
        ExceptHandler::getInstance().reportError(ErrorCode::FileNotFound, "图片文件不存在: " + filePath);
        return result;
    }

    // 3. 将图片加载到内存中 (QImage 能自动识别 jpg, png, bmp 等格式)
    QImage img(filePath);
    if (img.isNull()) {
        ExceptHandler::getInstance().reportError(ErrorCode::FileNotFound, "无法加载或解析该图片格式: " + filePath);
        return result;
    }

    qDebug() << "开始提取图片文字:" << filePath;

    // 4. 调用 OCR 引擎执行视觉识别
    QString extractedText = OcrEngine::getInstance().recognizeText(img);

    // 5. 判断识别结果并组装返回数据
    if (!extractedText.isEmpty()) {
        result.Text = extractedText.trimmed();
        result.isOpen = true; // 提取成功，放行标记！
        qDebug() << "图片 OCR 提取成功，共获取" << result.Text.length() << "个字符。";
    } else {
        // 如果引擎返回空，说明可能是一张纯风景图或没有清晰文字的图
        ExceptHandler::getInstance().reportError(ErrorCode::Success, "图片中未识别到可用文字: " + filePath);
    }

    return result;
}
