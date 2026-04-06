#include "handler_doc.h"
#include "core/excepthandler.h"
#include "convert/ocrengine.h"
#include "pugixml/pugixml.hpp"

#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QPdfDocument>
#include <QPdfSelection>
#include <QImage>
#include <QDebug>
#include <QPainter>

FileType HandlerDoc::getSupportedType() const {
    return FileType::Word;
}


namespace {
void collectTextRecursively(pugi::xml_node node, QString& outText) {
    for (pugi::xml_node child : node.children()) {
        QString name = QString::fromUtf8(child.name());
        if (name.endsWith(":t") || name == "t") {
            outText += QString::fromUtf8(child.child_value());
        }
        collectTextRecursively(child, outText);
    }
}

void findTablesRecursively(pugi::xml_node node, QString& out, int& tableCount) {
    for (pugi::xml_node child : node.children()) {
        QString name = QString::fromUtf8(child.name()).toLower();

        if (name.endsWith(":tbl") || name == "tbl") {
            tableCount++;
            out += QString("\n| --- [表格 %1] --- |\n").arg(tableCount);

            for (pugi::xml_node row : child.children()) {
                if (QString::fromUtf8(row.name()).toLower().endsWith(":tr")) {
                    out += "| ";
                    for (pugi::xml_node cell : row.children()) {
                        if (QString::fromUtf8(cell.name()).toLower().endsWith(":tc")) {
                            QString cellText;
                            collectTextRecursively(cell, cellText);
                            out += cellText.trimmed() + " | ";
                        }
                    }
                    out += "\n";
                }
            }
        } else {
            findTablesRecursively(child, out, tableCount);
        }
    }
}

bool unzipDocx(const QString& docxPath, const QString& targetDir) {
    QProcess process;
    QString program = "tar";
    QStringList arguments;

    arguments << "-xf" << QDir::toNativeSeparators(docxPath)
              << "-C" << QDir::toNativeSeparators(targetDir);

    qDebug() << "正在执行 tar 解压：" << program << arguments.join(" ");

    process.start(program, arguments);
    if (!process.waitForFinished(10000)) return false;

    QDir checkDir(targetDir);
    if (!checkDir.exists("word")) {
        qDebug() << "解压失败：目标目录中没有找到 word 文件夹";
        return false;
    }
    return true;
}
}

QString HandlerDoc::parseWordXml(const QString& xmlFilePath) {
    pugi::xml_document doc;
    auto result = doc.load_file(xmlFilePath.toStdWString().c_str());
    if (!result) return "";

    QString out = "";
    int tableCount = 0;

    findTablesRecursively(doc, out, tableCount);

    return out;
}


//Pei修改2
FileTxt HandlerDoc::extractText(const QString& filePath) {
    FileTxt result;
    QFileInfo fileInfo(filePath);
    result.filePath = filePath;
    result.fileName = fileInfo.fileName();
    result.isOpen = false;
    result.Text = "";

    if (!fileInfo.exists()) return result;

    QString suffix = fileInfo.suffix().toLower();

    // 1. 处理 .docx (结构化解析：文本 + 表格)
    if (suffix == "docx") {
        result.typeName = FileType::Word;

        // A. 先用 Pandoc 提取基础文本
        QString plainText = readViaPandoc(filePath);

        // B. 启动结构化表格提取
        // 创建一个临时文件夹，用完即焚
        QString tempDir = QDir::tempPath() + "/pdan_unzip_" + QString::number(QCoreApplication::applicationPid());
        QDir().mkpath(tempDir);

        if (unzipDocx(filePath, tempDir)) {
            QString xmlPath = tempDir + "/word/document.xml";
            // 调用我们刚才写好的 pugixml 核心函数
            QString tableData = parseWordXml(xmlPath);

            if (!tableData.isEmpty()) {
                result.Text = plainText + "\n\n--- [解析到文档表格数据] ---\n" + tableData;
            } else {
                result.Text = plainText; // 没表格就只用纯文本
            }
        } else {
            result.Text = plainText; // 解压失败降级只用 Pandoc
        }

        if (unzipDocx(filePath, tempDir)) {
            qDebug() << "自动解压成功，临时目录：" << tempDir;
            // 检查文件是否真的存在
            if (!QFile::exists(tempDir + "/word/document.xml")) {
                qDebug() << "错误：解压目录中找不到 word/document.xml";
            }
        } else {
            qDebug() << "错误：PowerShell 解压失败";
        }

        // C. 销毁临时痕迹
        QDir(tempDir).removeRecursively();
        if (!result.Text.isEmpty()) result.isOpen = true;
    }
    // 2. 处理 .pdf
    else if (suffix == "pdf") {
        result.typeName = FileType::PDF;
        result.Text = readPdf(filePath);
        if (!result.Text.isEmpty()) result.isOpen = true;
    }
    // 3. 处理 .txt / .md / .doc
    else if (suffix == "txt" || suffix == "md" || suffix == "doc") {
        result.typeName = (suffix == "md") ? FileType::Markdown :
                              (suffix == "doc") ? FileType::Word : FileType::Text;
        result.Text = (suffix == "doc") ? readViaPandoc(filePath) : readPlainText(filePath);
        if (!result.Text.isEmpty()) result.isOpen = true;
    }

    return result;
}

QString HandlerDoc::readPdf(const QString& filePath) {
    QPdfDocument pdfDoc;

    // 尝试加载 PDF
    if (pdfDoc.load(filePath) != QPdfDocument::Error::None) {
        ExceptHandler::getInstance().reportError(ErrorCode::FileNotFound, "无法打开 PDF 文件: " + filePath);
        return "";
    }

    QString fullText = "";
    int pageCount = pdfDoc.pageCount();
    qDebug() << "成功打开 PDF，共计" << pageCount << "页，准备提取...";

    // 逐页遍历 PDF
    for (int i = 0; i < pageCount; ++i) {
        QPdfSelection selection = pdfDoc.getAllText(i);
        QString pageText = selection.text().trimmed();

        if (pageText.length() < 15) {
            qDebug() << "第" << i+1 << "页文字极少，判定为扫描件，启动图像渲染与 OCR 兜底...";

            QSize originalSize = pdfDoc.pagePointSize(i).toSize();
            QSize hiResSize = originalSize * 3;
            QImage pageImage = pdfDoc.render(i, hiResSize);

            QImage solidImage(pageImage.size(), QImage::Format_RGB888);
            solidImage.fill(Qt::white);
            QPainter painter(&solidImage);
            painter.drawImage(0, 0, pageImage);
            painter.end();

            // OCR 引擎进行强行提取
            pageText = OcrEngine::getInstance().recognizeText(pageImage);
        }

        // 把这一页的内容拼接到总文本里，页与页之间空两行
        fullText += pageText + "\n\n";
    }

    pdfDoc.close();
    return fullText.trimmed();
}

QString HandlerDoc::readPlainText(const QString& filePath) {
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly)) {
        ExceptHandler::getInstance().reportError(ErrorCode::FileNotFound, "无法读取文本文件: " + filePath);
        return "";
    }

    QString content;
    qint64 fileSize = file.size();

    uchar* mappedMemory = file.map(0, fileSize);

    if (mappedMemory) {
        content = QString::fromUtf8(reinterpret_cast<const char*>(mappedMemory), fileSize);

        file.unmap(mappedMemory);
    } else {
        qDebug() << "内存映射失败，降级为常规 I/O 读取: " << filePath;
        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8);
        content = in.readAll();
    }

    file.close();

    content.replace("\r\n", "\n");

    return content;
}

QString HandlerDoc::readViaPandoc(const QString& filePath) {
    QProcess process;
    QString program = PANDOC_PATH;
    QStringList arguments;
    arguments << filePath << "-t" << "plain";

    process.start(program, arguments);

    if (!process.waitForFinished(30000)) {
        ExceptHandler::getInstance().reportError(ErrorCode::NetworkTimeout, "Pandoc 解析超时: " + filePath);
        process.kill();
        return "";
    }

    if (process.exitCode() != 0) {
        QString errorStr = process.readAllStandardError();
        ExceptHandler::getInstance().reportError(ErrorCode::LlmParseFailed, "Pandoc 转换失败: " + errorStr);
        return "";
    }

    QByteArray output = process.readAllStandardOutput();
    return QString::fromUtf8(output).trimmed();
}


