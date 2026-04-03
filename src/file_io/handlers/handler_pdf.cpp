#include "handler_pdf.h"
#include "core/excepthandler.h"
#include "convert/ocrengine.h"

#include <QFileInfo>
#include <QImage>
#include <QDebug>
#include <QPdfDocument>
#include <QPdfSelection>
#include <QPainter>
#include <algorithm> // 用于 std::sort
#include <QMutex>
#include <QMutexLocker>
static QMutex g_pdfiumMutex;

// 全局静态标志，确保 PDFium 库只初始化一次
static bool g_pdfiumInited = false;

HandlerPdf::HandlerPdf() noexcept {
    if (!g_pdfiumInited) {
        FPDF_InitLibrary();
        g_pdfiumInited = true;
    }
}


FileType HandlerPdf::getSupportedType() const {
    return FileType::PDF;
}

FileTxt HandlerPdf::extractText(const QString& filePath)
{
    FileTxt result;
    QFileInfo fileInfo(filePath);

    result.filePath = filePath;
    result.fileName = fileInfo.fileName();
    result.typeName = FileType::PDF;
    result.isOpen = false;

    if (!fileInfo.exists()) {
        ExceptHandler::getInstance().reportError(ErrorCode::FileNotFound, "PDF文件不存在: " + filePath);
        return result;
    }

    qDebug() << "📄 [PDFium] 正在进行坐标深度解析..." << filePath;
    QString nativeText = readPdfWithPdfium(filePath);

    // 智能判定：如果原生字数够多，直接返回
    if (nativeText.trimmed().length() > 50) {
        qDebug() << "[PDFium] 坐标排序提取成功";
        result.Text = nativeText.trimmed();
        result.isOpen = true;
        return result;
    }

    qDebug() << "[OCR] 原生文本极少，启动视觉通道...";
    QPdfDocument pdfDoc;
    pdfDoc.load(filePath);

    QString ocrText = "";
    for (int i = 0; i < pdfDoc.pageCount(); ++i) {
        ocrText += readPageWithOcr(pdfDoc, i) + "\n";
    }

    if (!ocrText.trimmed().isEmpty()) {
        result.Text = ocrText.trimmed();
        result.isOpen = true;
        qDebug() << "[OCR] 视觉识别完成。";
    }

    return result;
}


QString HandlerPdf::readPdfWithPdfium(const QString& filePath) {
    // 1. 线程安全初始化
    {
        QMutexLocker locker(&g_pdfiumMutex);
        static bool g_pdfiumInited = false;
        if (!g_pdfiumInited) {
            FPDF_InitLibrary();
            g_pdfiumInited = true;
        }
    }
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return "";
    QByteArray fileData = file.readAll();
    file.close();

    FPDF_DOCUMENT doc = nullptr;
    int pageCount = 0;
    {
        QMutexLocker locker(&g_pdfiumMutex);
        doc = FPDF_LoadMemDocument(fileData.constData(), fileData.size(), nullptr);
        if (!doc) return "";
        pageCount = FPDF_GetPageCount(doc);
    }

    QString fullContent = "";

    for (int i = 0; i < pageCount; ++i) {
        QVector<PdfChar> charList;

        {
            QMutexLocker locker(&g_pdfiumMutex);
            FPDF_PAGE page = FPDF_LoadPage(doc, i);
            if (page) {
                FPDF_TEXTPAGE textPage = FPDFText_LoadPage(page);
                if (textPage) {
                    int charCount = FPDFText_CountChars(textPage);
                    for (int j = 0; j < charCount; ++j) {
                        double l, b, r, t;
                        if (FPDFText_GetCharBox(textPage, j, &l, &b, &r, &t)) {
                            unsigned int unicode = FPDFText_GetUnicode(textPage, j);
                            if (unicode != 0) {
                                charList.append({l, t, r, QString::fromUcs4(&unicode, 1)});
                            }
                        }
                    }
                    FPDFText_ClosePage(textPage);
                }
                FPDF_ClosePage(page);
            }
        }

        if (!charList.isEmpty()) {
            std::stable_sort(charList.begin(), charList.end(), [](const PdfChar& a, const PdfChar& b) {
                if (qAbs(a.y - b.y) > 4.5) return a.y > b.y;
                return a.x < b.x;
            });

            QString pageText = "";
            QList<PdfChar> currentRow;
            double currentRowY = charList[0].y;

            auto processLine = [&](const QList<PdfChar>& row) {
                QString lineStr = "";
                double lastRight = -1;
                QList<PdfChar> sortedRow = row;
                std::stable_sort(sortedRow.begin(), sortedRow.end(), [](const PdfChar& a, const PdfChar& b){
                    return a.x < b.x;
                });

                for (const auto& c : sortedRow) {
                    if (lastRight != -1) {
                        double gap = c.x - lastRight;
                        if (gap > 1.2) lineStr += " ";
                        if (gap > 15.0) lineStr += "    ";
                    }
                    lineStr += c.ch;
                    lastRight = c.right;
                }
                return lineStr;
            };

            for (const auto& pc : charList) {
                if (qAbs(pc.y - currentRowY) < 4.0) {
                    currentRow.append(pc);
                } else {
                    pageText += processLine(currentRow) + "\n";
                    currentRow.clear();
                    currentRow.append(pc);
                    currentRowY = pc.y;
                }
            }
            if (!currentRow.isEmpty()) {
                pageText += processLine(currentRow) + "\n";
            }

            fullContent += pageText + "\n\n";
        }
    }


    {
        QMutexLocker locker(&g_pdfiumMutex);
        FPDF_CloseDocument(doc);
    }

    return fullContent;
}


QString HandlerPdf::readPageWithOcr(QPdfDocument& pdfDoc, int pageIndex) {
    QSizeF pageSize = pdfDoc.pagePointSize(pageIndex);
    QSize renderSize = (pageSize * 3.0).toSize(); // 300DPI 效果

    QImage pageImage = pdfDoc.render(pageIndex, renderSize);
    if (pageImage.isNull()) return "";

    QImage solidImage(pageImage.size(), QImage::Format_RGB888);
    solidImage.fill(Qt::white);

    QPainter painter(&solidImage);
    painter.drawImage(0, 0, pageImage);
    painter.end();

    return OcrEngine::getInstance().recognizeText(solidImage);
}
