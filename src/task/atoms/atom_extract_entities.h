#ifndef ATOM_EXTRACT_ENTITIES_H
#define ATOM_EXTRACT_ENTITIES_H

#include <QString>
#include <QStringList>

class AtomExtractEntities {
public:
    /**
     * @brief 从文本中提取指定类型的实体列表
     * @param sourceText 源文本 (如上传的文件内容)
     * @param targetEntity 提取目标 (如 "人名", "文件名", "技术栈")
     * @return 提取出的实体字符串列表
     */
    static QStringList execute(const QString& sourceText, const QString& targetEntity);
};

#endif // ATOM_EXTRACT_ENTITIES_H
