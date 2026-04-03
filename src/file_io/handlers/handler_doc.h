#ifndef HANDLER_DOC_H
#define HANDLER_DOC_H

#include "file_io/IFilerouter.h"

class HandlerDoc : public IFileRouter
{
public:
    static HandlerDoc& getInstance() {
        static HandlerDoc instance;
        return instance;
    }
    HandlerDoc() = default;
    ~HandlerDoc() override = default;

    HandlerDoc(const HandlerDoc&) = delete;
    HandlerDoc& operator=(const HandlerDoc&) = delete;

    FileTxt extractText(const QString& filePath) override;
    FileType getSupportedType() const override;
    QString parseWordXml(const QString& xmlFilePath);

private:
    QString readPlainText(const QString& filePath);
    QString readViaPandoc(const QString& filePath);
    QString readPdf(const QString& filePath);
};

#endif // HANDLER_DOC_H
