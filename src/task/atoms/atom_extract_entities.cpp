#include "atom_extract_entities.h"
#include "llm/aiclient.h"          // 引入单一通信模块
#include "llm/prompt_templates.h"  // 引入统一提示词
#include "json/nlohmann_json.hpp"
#include <QDebug>

using json = nlohmann::json;

QStringList AtomExtractEntities::execute(const QString& sourceText, const QString& targetEntity) {
    QStringList resultList;
    if (sourceText.isEmpty() || targetEntity.isEmpty()) return resultList;

    // 1. 组装 Prompt
    QString prompt = PromptForExtraction.arg(targetEntity, sourceText);

    // 2. 直接调用 Aiclient 提供的阻塞接口（要求必须返回 JSON 格式）
    QString responseStr = Aiclient::getInstance().generateBlocking(prompt, true);
    if (responseStr.isEmpty()) return resultList;
    qDebug()<<responseStr;
    // 3. 将返回的 JSON 字符串反序列化为 QStringList
    try {
        // 预清洗：处理小模型可能包裹的 Markdown 格式 (沿用你之前的清洗逻辑)
        QString cleanStr = responseStr.trimmed();
        if (cleanStr.startsWith("```json")) cleanStr.remove(0, 7);
        if (cleanStr.startsWith("```")) cleanStr.remove(0, 3);
        if (cleanStr.endsWith("```")) cleanStr.chop(3);
        cleanStr = cleanStr.trimmed();

        // 尝试寻找真正的起点和终点，防止前面有废话
        int startArr = cleanStr.indexOf('[');
        int startObj = cleanStr.indexOf('{');
        // 找出最先出现的 JSON 符号
        int startIndex = -1;
        if (startArr != -1 && startObj != -1) startIndex = std::min(startArr, startObj);
        else if (startArr != -1) startIndex = startArr;
        else if (startObj != -1) startIndex = startObj;

        int endIndex = cleanStr.lastIndexOf('}') > cleanStr.lastIndexOf(']') ?
                           cleanStr.lastIndexOf('}') : cleanStr.lastIndexOf(']');

        if (startIndex != -1 && endIndex != -1 && endIndex >= startIndex) {
            cleanStr = cleanStr.mid(startIndex, endIndex - startIndex + 1);
        }

        // 开始解析
        json j_root = json::parse(cleanStr.toStdString());

        // 保底：智能适配 Array 和 Object 两种结构
        if (j_root.is_array()) {
            // 情况 A：极其听话的模型，直接返回了 ["狮子座", "双鱼座"]
            for (const auto& item : j_root) {
                if (item.is_string()) {
                    resultList.append(QString::fromStdString(item.get<std::string>()));
                }
            }
        }
        else if (j_root.is_object()) {
            // 情况 B：自作聪明的模型，返回了 {"人名": ["狮子座", "双鱼座"]}
            // 我们直接遍历这个对象的所有 value，只要找到 Array 就提取里面的字符串！
            for (auto it = j_root.begin(); it != j_root.end(); ++it) {
                if (it.value().is_array()) {
                    for (const auto& item : it.value()) {
                        if (item.is_string()) {
                            resultList.append(QString::fromStdString(item.get<std::string>()));
                        }
                    }
                }
            }
        }

    } catch (const std::exception& e) {
        qDebug() << "实体提取 JSON 反序列化失败:" << e.what() << "\n原始返回:" << responseStr;
    }

    qDebug() << "成功提取到" << resultList.size() << "个实体:" << resultList;
    return resultList;
}
