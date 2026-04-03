#include "file_factory.h"

FileType FileFactory::detectFileType(const QString& filePath) {
    QString suffix = QFileInfo(filePath).suffix().toLower();

    if (suffix == "txt" || suffix == "csv")
        return FileType::Text;
    if (suffix == "md" || suffix == "markdown")
        return FileType::Markdown;
    if (suffix == "html" || suffix == "htm")
        return FileType::HTML;
    if (suffix == "pdf")
        return FileType::PDF;
    if (suffix == "doc" || suffix == "docx")
        return FileType::Word;
    if (suffix == "png" || suffix == "jpg" || suffix == "jpeg" || suffix == "bmp" || suffix == "gif")
        return FileType::Image;

    return FileType::Unsupported;
}

std::unique_ptr<IFileRouter> FileFactory::createRouter(FileType fileType) {
    switch (fileType) {
    case FileType::Text:
    case FileType::Word:
    case FileType::Markdown:
    case FileType::HTML:
        // Word、Markdown 等纯文本类文件交给 Pandoc 引擎
        return std::make_unique<HandlerDoc>();

    case FileType::Image:
        // 单张图片交给 OCR 引擎
        return std::make_unique<HandlerImg>();

        //PDF 专属通道打通
    case FileType::PDF:
        return std::make_unique<HandlerPdf>();

    default:
        return std::make_unique<HandlerOther>();
    }
}
