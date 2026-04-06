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
    QString fileName;
    QString filePath;
    QString Text;
    FileType typeName;
    bool isOpen;
};

// 2. 文档切片数据（内容转换模块 -> 数据存取模块）
struct DocChunk {
    double score = 0.0;
    QString fileName;
    QString filePath;
    QString pureText;
    QString parentText;
    int chunkId = -1;
    int parentId = -1;
};

// 3. 扩充 ParsedIntent 结构体
struct ParsedIntent {
    QStringList keywords;
    QString hydeText;
    QString targetFileName;
    QString extractTarget;
    QString uploadedFileName;
    QString uploadedFilePath;
    QString uploadedFileText;
    IntentType intentType;
};

// 4. 任务执行结果（任务处理模块 -> LLM交互模块/UI）
struct TaskResult {
    ParsedIntent aim;
    QString directUIResponse;
    QString errorMsg;
    std::vector<DocChunk> slices;
    bool success = true;
};

struct SessionInfo {
    QString title;
    QString createTime;
    int sessionId;
};

struct MessageInfo {
    QString role;
    QString content;
    int msgId;
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
    QString text;
    ChunkType type;            // 4 bytes
};

struct StreamState {
    QString tagBuffer;
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
    QString actionName; //- 工具名称
    QString description;   //- 大模型生成的该步解释
    QMap<QString, QString> params;  //- 动态参数
    int stepId = 0;   // 4 bytes - 执行顺序 ID (移至末尾)
};

// Agent 工作流的全局黑板 Blackboard
struct WorkflowContext {
    //静态输入区
    QString originalQuery;
    QString uploadedFileText;
    QString uploadedFileName;
    QString uploadedFilePath;
    //强类型流转区常用原子的明确输出位置
    QStringList extractedEntities;
    QList<DocChunk> recalledChunks; // 24 bytes
    QString intermediateAnswer;

    //动态插槽
    QVariantMap dynamicBlackboard;

    //执行状态
    QString errorMsg;
    bool hasError = false;
};

#endif // GLOBAL_DEFS_H
