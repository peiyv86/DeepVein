#ifndef HANDLER_PDF_H
#define HANDLER_PDF_H

#include "file_io/IFilerouter.h" // 注意大小写与你的项目一致
#include <QVector>
extern "C" {
#include "fpdfview.h"
#include "fpdf_text.h"
}

struct PdfChar {
    double x;
    double y;
    double right;
    QString ch;
};

class HandlerPdf : public IFileRouter
{
public:
    HandlerPdf() noexcept;
    ~HandlerPdf() noexcept override = default;

    // 核心提取接口
    FileTxt extractText(const QString& filePath) override;

    // 标识当前处理器负责的文件类型
    FileType getSupportedType() const override;
private:
    // ✨ 高级解析：利用 PDFium 进行坐标排序提取
    QString readPdfWithPdfium(const QString& filePath);

    // 💡 视觉解析：保留你原有的 OCR 渲染逻辑作为兜底
    QString readPageWithOcr(class QPdfDocument& pdfDoc, int pageIndex);

};

#endif // HANDLER_PDF_H
