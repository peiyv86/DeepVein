#ifndef IFILEROUTER_H
#define IFILEROUTER_H

#include"core/global_defs.h"
#include<QFileInfo>
#include <fstream>
#include <iostream>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QUuid>
#include <QDir>
#include <QDebug>
#include <QCoreApplication>
#include <QString>

/**
 * 文件解析器抽象接口
 * 负责抹平不同文件格式的差异，对外统一输出提取出的纯文本长字符串
 */
class IFileRouter {
public:
    virtual ~IFileRouter() = default;

    /**
     * 核心接口：提取纯文本
     */
    virtual FileTxt extractText(const QString& filePath) = 0;

    /**
     * 辅助接口：获取该 Handler 负责的类型
     */
    virtual FileType getSupportedType() const = 0;
};

#endif // IFILEROUTER_H
