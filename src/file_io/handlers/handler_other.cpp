#include "handler_other.h"
#include "core/excepthandler.h"
#include <QFileInfo>

FileTxt HandlerOther::extractText(const QString& filePath)
{
    FileTxt result;
    QFileInfo fileInfo(filePath);
    result.filePath = filePath;
    result.fileName = fileInfo.fileName();
    result.typeName = FileType::Unsupported;
    result.isOpen = false;
    result.Text = "";

    ExceptHandler::getInstance().reportError(ErrorCode::Success, "跳过不支持的文件格式: " + filePath);
    return result;
}

FileType HandlerOther::getSupportedType() const {
    return FileType::Unsupported;
}
