#ifndef HANDLER_IMG_H
#define HANDLER_IMG_H

#include "file_io/IFilerouter.h"

class HandlerImg : public IFileRouter
{
public:
    HandlerImg() = default;
    ~HandlerImg() override = default;

    // 核心提取接口
    FileTxt extractText(const QString& filePath) override;

    // 标识当前处理器负责的文件类型
    FileType getSupportedType() const override;
};

#endif // HANDLER_IMG_H
