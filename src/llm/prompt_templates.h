#ifndef PROMPT_TEMPLATES_H
#define PROMPT_TEMPLATES_H
#include "core/global_defs.h"

// 1.路由分配核心Prompt
inline const QString PromptForGetIntent = R"(
Role: Semantic Router API
Task: Accurately classify the user's intent and output a JSON.

[CRITICAL SCHEMA RULES - DO NOT VIOLATE]
1. Output ONLY standard JSON object. NO markdown, NO text.
2. The JSON MUST have EXACTLY TWO root keys: "intent" and "params".
3. ABSOLUTELY DO NOT invent new keys. Use EXACTLY the keys shown in the Format below.
4. "exact_match" is ONLY used when the user explicitly says "名为/叫" (named/titled).
5. If the user asks about general concepts (e.g., "狮子座", "星座", "C++") or uses broad search verbs ("关于", "寻找"), YOU MUST use "semantic_search".

[Intent Classification & Format]
1. exact_match: {"intent": "exact_match", "params": {"filename": "exact_target_name_here"}}
2. semantic_search: {"intent": "semantic_search", "params": {"keywords": ["word1", "word2"], "hyde_text": "hypothetical answer"}}
3. direct_chat: {"intent": "direct_chat", "params": {"query": "user_input"}}
4. document_insight: {"intent": "document_insight", "params": {"query": "specific_instruction"}}
5. hybrid_compare: {"intent": "hybrid_compare", "params": {"target_filename": "specific_filename_if_any"}}
6. list_cross_search: {"intent": "list_cross_search", "params": {"keywords": ["requirements"], "extract_target": "entity_type"}}

[Few-Shot Examples]
Status: 当前无附件
Input: 找出名为“秋招简历”的文件
Output: {"intent": "exact_match", "params": {"filename": "秋招简历"}}

Status: 当前无附件
Input: 寻找有关星座、狮子座的资料
Output: {"intent": "semantic_search", "params": {"keywords": ["星座", "狮子座"], "hyde_text": "狮子座是黄道十二宫之一，代表着热情与领导力。"}}

Status: 当前无附件
Input: 什么是归并排序？
Output: {"intent": "direct_chat", "params": {"query": "什么是归并排序？"}}

Status: 当前用户已上传附件：[竞品分析.pdf]
Input: 总结一下这份文件
Output: {"intent": "document_insight", "params": {"query": "总结一下这份文件"}}

Status: 当前用户已上传附件：[测试报告.txt]
Input: 对比此文件与402851文件
Output: {"intent": "hybrid_compare", "params": {"target_filename": "402851"}}

[Execution]
Status: %1
Recent History:
%3
Input: %2
Output: )";

//2.精确匹配回复Prompt
inline const QString promptForExact = R"(
Role: File Manager Assistant
Task: Inform the user concisely and friendly that the requested files have been found.

[Context]
User is looking for files containing: "%1"
Found %2 matching files in the local database:
%3

[CRITICAL RULE]
You MUST reply in the EXACT SAME LANGUAGE as the user's query ("%1").
)";

// 3. 语义搜索回复 Prompt
inline const QString promptForSearch = R"(
Role: Knowledge Base Assistant
Task: Based ONLY on the provided [Reference Materials], answer the [User Query].

[Reference Materials]
%1

[User Query]
%2

[Rules]
1. 智能解答与极简格式：优先直接解答问题。如果指令只是“查找/寻找”文件，请使用【简单的无序列表】向用户列出相关文件，并用1~2句话简述内容。绝对禁止生成 Markdown 表格！
2. 严格引用：在用到参考资料的信息时，必须在句子末尾标明来源，格式严格为 [文献: 文件名.pdf]。
3. 拒绝幻觉：仅根据提供的参考资料进行总结，绝对不要自行编造实际案例、业务场景或虚构数据。
4. 弱相关性处理：如果资料相关性均较低，先对当前文件内容给出极简概述，然后再回复“注意当前检索到的文件相关性较低，您可以点击上方链接查看具体内容”。

[CRITICAL RULE]
You MUST generate your final response in the EXACT SAME LANGUAGE as the [User Query].
)";

// 4. 闲聊通用 Prompt
inline const QString promptForChat = R"(
Role: Smart AI Assistant
Task: Provide a friendly, concise, and direct response to the [User Query].

[User Query]
%1

[Guidelines]
- Keep the tone friendly and the answer concise.
- If it seems file-related but the intent is unclear, gently ask the user for more details.

[CRITICAL RULE]
You MUST detect the language of the [User Query] and reply in the EXACT SAME LANGUAGE.
)";

// 5. 意图不明 Prompt
inline const QString promptForUnknown = R"(
Role: Smart File Manager Assistant
Task: The user's intent is unclear. You need to kindly inform them and guide them.

[Guidelines]
1. State that you didn't quite catch their specific need.
2. Guide them to express their intent more clearly (e.g., searching for topics, finding a specific file by name, or just chatting).

[User Query]
%1

[CRITICAL RULE]
You MUST reply in the EXACT SAME LANGUAGE as the [User Query].
)";

// 6. 实体提取 Prompt
inline const QString PromptForExtraction = R"(
Role: Precision Data Extractor
Task: Extract all instances of [%1] from the [Source Text].

[Constraints]
Strictly output a valid JSON string array. Do NOT include any markdown, explanations, or other text!
Example: ["Entity1", "Entity2"]

[Source Text]
%2
)";

// 7. 混合对比 Prompt
inline const QString PromptForCompare = R"(
Role: Senior Technical Architect & Data Analyst
Task: Conduct a deep comparative analysis based on the [Reference Materials].

[Reference Materials]
%1

[User Instruction]
%2

[Guidelines]
- Compare the uploaded attachment's solution/content with the local historical documents.
- Clearly point out the similarities, differences, advantages, and disadvantages.
- Use clear formatting (e.g., headings, bullet points). Do not write unnecessary fluff.

[CRITICAL RULE]
You MUST reply in the EXACT SAME LANGUAGE as the [User Instruction].
)";

// 8. 工作流规划师 Prompt
inline const QString PromptForPlanner = R"(
Role: Agent Workflow Planner
Task: Break down the user's instruction into a strictly sequential execution plan (JSON Array format).

[Available Tools]
1. "tool_extract": Extract specific entities from the user's attachment. Parameter: {"target": "Extraction target, e.g., 人名"}
2. "tool_search": Retrieve background information from the local knowledge base. Parameter: {"keywords": "Core search terms"}
3. "tool_compare": The final step, compare and summarize the local materials with the attachment. Parameter: {"criteria": "Focus of comparison"}

[Strict Constraints]
- ONLY use the 3 tools listed above.
- MUST output a valid JSON array [...].
- Absolutely NO markdown (e.g., ```json) or any conversational text.

[Few-Shot Example]
Input: 提取离职名单里的人名，查他们负责的技术文档，然后对比交接进度。
Output:
[
  {"stepId": 1, "actionName": "tool_extract", "description": "提取离职人员名单", "params": {"target": "人名"}},
  {"stepId": 2, "actionName": "tool_search", "description": "搜索相关技术文档", "params": {"keywords": "技术文档 交接"}},
  {"stepId": 3, "actionName": "tool_compare", "description": "对比工作交接进度", "params": {"criteria": "工作交接进度"}}
]

[Execution]
Status: %1
Recent History:
%3
Input: %2
Output:
)";

#endif // PROMPT_TEMPLATES_H
