#ifndef GLOBAL_DEFS_H
#define GLOBAL_DEFS_H

#include <QString>
#include <QStringList>
#include <QMap>
#include <QVariant>
#include <vector>
#include <QDebug>
#include <future>

//enums------------------------------------------
enum class IntentType {
    SemanticSearch,
    ExactMatch,
    DirectChat,
    DocumentInsight,
    ListCrossSearch, // 名单交叉检索
    HybridCompare,
    Unknown
};

enum class FileType {
    Text,       // .txt, .md, .csv 等纯文本
    PDF,        // .pdf
    Word,       // .doc, .docx
    Image,      // .jpg, .png ( OCR 提取语义)
    Markdown,
    HTML,
    Unsupported // 不支持的文件，直接跳过并记录日志
};

enum class ErrorCode {
    Success = 0,
    NetworkTimeout,     // AIClient的API 请求超时
    LlmParseFailed,     // 大模型返回的 JSON 格式错误
    DbTransactionError, // SQLite 写入失败
    VectorIndexError,   // HNSWlib 加载或检索失败
    FileNotFound,        // 用户想精准匹配的文件在本地不存在
    DatabaseInitFailed, // 数据库初始化失败
    DatabaseQueryFailed // 数据库查询失败
};

//Structs------------------------------------------

// 1. 文档数据（文件处理模块 -> 内容转换模块）
struct FileTxt {
    QString fileName;          // 8 bytes
    QString filePath;          // 8 bytes
    QString Text;              // 8 bytes
    FileType typeName;         // 4 bytes
    bool isOpen;               // 1 byte
};

// 2. 文档切片数据（内容转换模块 -> 数据存取模块）
struct DocChunk {
    double score = 0.0;        // 8 bytes
    QString fileName;          // 8 bytes
    QString filePath;          // 8 bytes
    QString pureText;          // 8 bytes
    QString parentText;        // 8 bytes
    int chunkId = -1;          // 4 bytes (凑在一起)
    int parentId = -1;         // 4 bytes (凑在一起，4+4刚好填满一个 8 字节对齐)
};

// 3. 扩充 ParsedIntent 结构体
struct ParsedIntent {
    QStringList keywords;      // 8 bytes
    QString hydeText;          // 8 bytes
    QString targetFileName;    // 8 bytes
    QString extractTarget;     // 8 bytes
    QString uploadedFileName;  // 8 bytes
    QString uploadedFilePath;  // 8 bytes
    QString uploadedFileText;  // 8 bytes
    IntentType intentType;     // 4 bytes (移到最后)
};

// 4. 任务执行结果（任务处理模块 -> LLM交互模块/UI）
struct TaskResult {
    ParsedIntent aim;          // 64 bytes
    QString directUIResponse;  // 8 bytes
    QString errorMsg;          // 8 bytes
    std::vector<DocChunk> slices;// 24 bytes
    bool success = true;       // 1 byte
};

struct SessionInfo {
    QString title;             // 8 bytes
    QString createTime;        // 8 bytes
    int sessionId;             // 4 bytes (移至末尾，减少Padding)
};

struct MessageInfo {
    QString role;              // 8 bytes
    QString content;           // 8 bytes
    int msgId;                 // 4 bytes (移至末尾，减少Padding)
};

static IntentType getIntentEnum(const QString& intent)
{
    QString normalizedIntent = intent.trimmed().toLower();
    if (normalizedIntent == "semantic_search") return IntentType::SemanticSearch;
    else if (normalizedIntent == "exact_match") return IntentType::ExactMatch;
    else if (normalizedIntent == "direct_chat") return IntentType::DirectChat;
    else if (normalizedIntent == "document_insight") return IntentType::DocumentInsight;
    else if (normalizedIntent == "list_cross_search") return IntentType::ListCrossSearch;
    else if (normalizedIntent == "hybrid_compare") return IntentType::HybridCompare;
    else {
        qDebug() << "未知意图类型，已回退为 DirectChat:" << intent;
        return IntentType::Unknown;
    }
}

// 流式解析器专用数据结构
enum class ChunkType {
    HtmlBlock = 0,
    ThinkingText = 1,
    NormalText = 2
};

struct StreamChunk {
    QString text;              // 8 bytes
    ChunkType type;            // 4 bytes
};

struct StreamState {
    QString tagBuffer;         // 8 bytes (移至前面)
    bool isThinking = false;   // 1 byte
};

// 为 PendingTask 添加移动构造函数，支持直接构造
struct PendingTask {
    DocChunk chunk;
    std::future<std::vector<float>> embeddingFuture; // 移至前面 (大小视STL实现，通常8或16)
    int parentId;                                    // 移至末尾

    PendingTask(DocChunk&& c, int id, std::future<std::vector<float>>&& f)
        : chunk(std::move(c)), embeddingFuture(std::move(f)), parentId(id) {}
};

// Workflow Agentic Blackboard
// 描述大模型规划出的单步执行任务
struct WorkflowStep {
    QString actionName;                   // 8 bytes - 工具名称，如 "tool_extract"
    QString description;                  // 8 bytes - 大模型生成的该步解释
    QMap<QString, QString> params;        // 8 bytes - 动态参数
    int stepId = 0;                       // 4 bytes - 执行顺序 ID (移至末尾)
};

// Agent 工作流的全局黑板 Blackboard
struct WorkflowContext {
    //静态输入区
    QString originalQuery;                // 8 bytes
    QString uploadedFileText;             // 8 bytes
    QString uploadedFileName;             // 8 bytes
    QString uploadedFilePath;
    //强类型流转区常用原子的明确输出位置
    QStringList extractedEntities;        // 8 bytes
    QList<DocChunk> recalledChunks; // 24 bytes
    QString intermediateAnswer;           // 8 bytes

    //动态插槽
    QVariantMap dynamicBlackboard;        // 8 bytes

    //执行状态
    QString errorMsg;                     // 8 bytes (移到 hasError 前面，消除 7 字节 Padding！)
    bool hasError = false;                // 1 byte
};

#endif // GLOBAL_DEFS_H
