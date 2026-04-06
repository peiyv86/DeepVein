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

//文件解析器抽象接口
//抹平不同文件格式的差异，对外统一输出提取出的纯文本长字符串
class IFileRouter {
public:
    virtual ~IFileRouter() = default;
    virtual FileTxt extractText(const QString& filePath) = 0;//提取纯文本
    virtual FileType getSupportedType() const = 0;//获取该Handler负责的类型
};

#endif // IFILEROUTER_H
