#include "reranker_engine.h"
#include <onnxruntime_cxx_api.h>
#include "tokenizers_cpp.h"
#include "core/excepthandler.h"
RerankerEngine::RerankerEngine()
    : m_env(nullptr), m_sessionOptions(nullptr), m_session(nullptr),
    m_memoryInfo(nullptr), m_tokenizer(nullptr) {}

RerankerEngine::~RerankerEngine() {
    if (m_session) delete static_cast<Ort::Session*>(m_session);
    if (m_sessionOptions) delete static_cast<Ort::SessionOptions*>(m_sessionOptions);
    if (m_env) delete static_cast<Ort::Env*>(m_env);
    if (m_memoryInfo) delete static_cast<Ort::MemoryInfo*>(m_memoryInfo);
    if (m_tokenizer) delete static_cast<tokenizers::Tokenizer*>(m_tokenizer);
}

bool RerankerEngine::init(const QString& modelPath, const QString& tokenizerPath) {
    try {
        QFile file(tokenizerPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCritical() << "无法打开 tokenizer 文件:" << tokenizerPath;
            return false;
        }
        QByteArray jsonBytes = file.readAll();
        std::string jsonContent = jsonBytes.toStdString();
        file.close();

        auto tokenizer = tokenizers::Tokenizer::FromBlobJSON(jsonContent);
        m_tokenizer = tokenizer.release();

        Ort::Env* env = new Ort::Env(ORT_LOGGING_LEVEL_WARNING, "SemanticExtract");
        m_env = env;

        Ort::SessionOptions* sessionOptions = new Ort::SessionOptions();
        sessionOptions->SetIntraOpNumThreads(4);
        m_sessionOptions = sessionOptions;

#ifdef _WIN32
        m_session = new Ort::Session(*env, modelPath.toStdWString().c_str(), *sessionOptions);
#else
        m_session = new Ort::Session(*env, modelPath.toStdString().c_str(), *sessionOptions);
#endif
        m_memoryInfo = new Ort::MemoryInfo(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault));

        qDebug() << "RerankerEngine:模型与分词器加载成功";
        return true;

    } catch (const std::exception& e) {
        ExceptHandler::getInstance().reportError(ErrorCode::VectorIndexError,
          QString("模型初始化失败: ") + e.what());
        return false;
    }
}

float RerankerEngine::computeScore(const QString& query, const QString& text) {
    if (!m_session || !m_tokenizer) return -999.0f;

    try {
        auto* tokenizer = static_cast<tokenizers::Tokenizer*>(m_tokenizer);

        // 将 Query 和 Text 拼接在一起
        // 1. 独立 Encode
        // 1. 分别独立编码 Query 和 Text (这样它们各自会带有 [CLS] 和 [SEP])
        std::vector<int> query_ids = tokenizer->Encode(query.toStdString());
        std::vector<int> text_ids = tokenizer->Encode(text.toStdString());

        if (query_ids.empty() || text_ids.empty()) return 0.0f;

        // 2. 动态获取当前模型的特殊标记 (BOS/CLS 和 EOS/SEP)
        int cls_token = query_ids.front(); // 通常是 101 或 0
        int sep_token = query_ids.back();  // 通常是 102 或 2

        // 3. 手动进行标准的 Cross-Encoder 组装: [CLS] Query [SEP] Text [SEP]
        std::vector<int> input_ids;
        input_ids.reserve(query_ids.size() + text_ids.size());

        // 放入 Query (自带 [CLS] 和 末尾的 [SEP])
        input_ids.insert(input_ids.end(), query_ids.begin(), query_ids.end());

        // 放入 Text，但要跳过它的第一个 [CLS] 标记，防止格式错乱
        if (text_ids.size() > 1) {
            input_ids.insert(input_ids.end(), text_ids.begin() + 1, text_ids.end());
        }

        // 严格截断到 512，并保护末尾的 [SEP]
        if (input_ids.size() > 512) {
            input_ids.resize(512);
            input_ids.back() = sep_token; // 强行把最后一个 token 设为结束符！
        }

        std::vector<int64_t> attention_mask_int64(input_ids.size(), 1);

        // 转换为 64 位数组送给 ONNX 执行推理 ...
        std::vector<int64_t> input_ids_int64;
        input_ids_int64.reserve(input_ids.size());
        for (int id : input_ids) {
            input_ids_int64.push_back(id);
        }

        std::vector<int64_t> input_shape = {1, static_cast<int64_t>(input_ids.size())};

        auto* memoryInfo = static_cast<Ort::MemoryInfo*>(m_memoryInfo);

        Ort::Value input_tensor = Ort::Value::CreateTensor<int64_t>(
            *memoryInfo, input_ids_int64.data(), input_ids_int64.size(), input_shape.data(), input_shape.size());
        Ort::Value attention_tensor = Ort::Value::CreateTensor<int64_t>(
            *memoryInfo, attention_mask_int64.data(), attention_mask_int64.size(), input_shape.data(), input_shape.size());

        // 输入名字只留两个
        const char* input_names[] = {"input_ids", "attention_mask"};

        auto* session = static_cast<Ort::Session*>(m_session);
        Ort::AllocatorWithDefaultOptions allocator;
        auto out_name_allocated = session->GetOutputNameAllocated(0, allocator);
        const char* output_names[] = { out_name_allocated.get() };

        // 用 std::array 只装两个 Tensor
        std::array<Ort::Value, 2> input_tensors = {
            std::move(input_tensor),
            std::move(attention_tensor)
        };

        // 执行推理，参数数量改成 2！
        auto output_tensors = session->Run(Ort::RunOptions{}, input_names, input_tensors.data(), 2, output_names, 1);

        // 获取得分
        float* floatarr = output_tensors.front().GetTensorMutableData<float>();

        // 将 Logit 转换为 0~1 的分数，并设立阈值拦截！
        float logit = floatarr[0];
        float prob = 1.0f / (1.0f + std::exp(-logit)); // Sigmoid 激活函数

        return prob;

    } catch (const std::exception& e) {
        qDebug() << "精排失败:" << e.what();
        return -999.0f;
    }
}
