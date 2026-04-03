#ifndef HANDLER_OTHER_H
#define HANDLER_OTHER_H

#include "file_io/IFilerouter.h"

class HandlerOther : public IFileRouter
{
public:
    HandlerOther() = default;
    ~HandlerOther() override = default;

    FileTxt extractText(const QString& filePath) override;
    FileType getSupportedType() const override;
};

#endif // HANDLER_OTHER_H
