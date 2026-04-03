#include "action_chat.h"

// 日常聊天不需要查数据库，所以 result.slices 保持为空的 std::vector 即可。
// 外层的 LLM 交互模块拿到这个 result 后，发现 aim.intentType 是 DirectChat，
// 就会直接把用户的原始问题发给大模型进行闲聊，而不去强行拼接切片上下文。

TaskResult ActionChat::execute(const ParsedIntent& intent)
{
    TaskResult result;
    result.aim = intent;    // 保存意图溯源
    result.success = true;  // 默认执行成功

    qDebug() << "进入日常聊天模式 (DirectChat)，准备跳过本地检索。";
    return result;
}
