#ifndef FILE_FACTORY_H
#define FILE_FACTORY_H

#include <memory>
#include <QFileInfo>

#include "file_io/IFilerouter.h" // 严格匹配头文件大小写
#include "handlers/handler_doc.h"
#include "handlers/handler_img.h"
#include "handlers/handler_other.h"
#include "handlers/handler_pdf.h"
#include "core/global_defs.h"

class FileFactory {
public:
    /**
     * 工具方法：根据文件后缀名自动推断 FileType
     */
    static FileType detectFileType(const QString& filePath);

    /**
     * 工厂方法：根据 FileType 创建对应的解析器
     */
    static std::unique_ptr<IFileRouter> createRouter(FileType fileType);
};

#endif // FILE_FACTORY_H
