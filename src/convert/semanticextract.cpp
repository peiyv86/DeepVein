#include "semanticextract.h"
#include "core/excepthandler.h"

#include <array>
#include <onnxruntime_cxx_api.h>
#include "tokenizers_cpp.h"

SemanticExtract::SemanticExtract()
    : m_env(nullptr), m_sessionOptions(nullptr), m_session(nullptr),
    m_memoryInfo(nullptr), m_tokenizer(nullptr) {}

SemanticExtract::~SemanticExtract() {
    if (m_session) delete static_cast<Ort::Session*>(m_session);
    if (m_sessionOptions) delete static_cast<Ort::SessionOptions*>(m_sessionOptions);
    if (m_env) delete static_cast<Ort::Env*>(m_env);
    if (m_memoryInfo) delete static_cast<Ort::MemoryInfo*>(m_memoryInfo);
    if (m_tokenizer) delete static_cast<tokenizers::Tokenizer*>(m_tokenizer);
}

bool SemanticExtract::init(const QString& modelPath, const QString& tokenizerPath) {
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

        qDebug() << "SemanticExtract:模型与分词器加载成功";
        return true;

    } catch (const std::exception& e) {
        ExceptHandler::getInstance().reportError(ErrorCode::VectorIndexError,
                                                 QString("模型初始化失败: ") + e.what());
        return false;
    }
}

std::vector<float> SemanticExtract::getEmbedding(const QString& text) {
    std::vector<float> result;
    if (!m_session || !m_tokenizer) return result;

    try {
        auto* tokenizer = static_cast<tokenizers::Tokenizer*>(m_tokenizer);

        // 获取 32位 分词 ID
        std::vector<int> input_ids = tokenizer->Encode(text.toStdString());

        // 确定最终长度，不超 512
        size_t seq_len = std::min(input_ids.size(), static_cast<size_t>(512));

        // 利用迭代器区间直接构造 64位 数组，代码更短，编译器底层优化更好
        std::vector<int64_t> input_ids_int64(input_ids.begin(), input_ids.begin() + seq_len);

        // 直接用 64位 初始化 mask 和 type
        std::vector<int64_t> attention_mask_int64(seq_len, 1);
        std::vector<int64_t> token_type_ids_int64(seq_len, 0);

        std::vector<int64_t> input_shape = {1, static_cast<int64_t>(seq_len)};
        auto* memoryInfo = static_cast<Ort::MemoryInfo*>(m_memoryInfo);

        Ort::Value input_tensor = Ort::Value::CreateTensor<int64_t>(
            *memoryInfo, input_ids_int64.data(), input_ids_int64.size(), input_shape.data(), input_shape.size());

        Ort::Value attention_tensor = Ort::Value::CreateTensor<int64_t>(
            *memoryInfo, attention_mask_int64.data(), attention_mask_int64.size(), input_shape.data(), input_shape.size());

        Ort::Value token_type_tensor = Ort::Value::CreateTensor<int64_t>(
            *memoryInfo, token_type_ids_int64.data(), token_type_ids_int64.size(), input_shape.data(), input_shape.size());

        const char* input_names[] = {"input_ids", "attention_mask", "token_type_ids"};

        auto* session = static_cast<Ort::Session*>(m_session);
        Ort::AllocatorWithDefaultOptions allocator;
        auto out_name_allocated = session->GetOutputNameAllocated(0, allocator);
        const char* output_names[] = { out_name_allocated.get() };

        // 采用栈上 array，避免堆开销
        std::array<Ort::Value, 3> input_tensors = {
            std::move(input_tensor),
            std::move(attention_tensor),
            std::move(token_type_tensor)
        };

        // 执行推理
        auto output_tensors = session->Run(Ort::RunOptions{}, input_names, input_tensors.data(), 3, output_names, 1);

        float* floatarr = output_tensors.front().GetTensorMutableData<float>();
        auto shapeInfo = output_tensors.front().GetTensorTypeAndShapeInfo().GetShape();
        size_t hidden_size = shapeInfo.back();

        result.assign(floatarr, floatarr + hidden_size);
        normalizeVector(result);

    } catch (const std::exception& e) {
        //
        ExceptHandler::getInstance().reportError(ErrorCode::VectorIndexError,
                                                 QString("提取特征失败: ") + e.what());
    }

    return result;
}

std::vector<std::vector<float>> SemanticExtract::getEmbeddingBatch(const QList<QString>& texts) {
    std::vector<std::vector<float>> results;
    int batch_size = texts.size();
    if (!m_session || !m_tokenizer || batch_size == 0) return results;

    try {
        auto* tokenizer = static_cast<tokenizers::Tokenizer*>(m_tokenizer);

        //批量分词并找出当前批次的最大长度
        std::vector<std::vector<int>> all_input_ids;
        all_input_ids.reserve(batch_size);
        size_t max_seq_len = 0;

        for (const QString& text : texts) {
            std::vector<int> input_ids = tokenizer->Encode(text.toStdString());
            // 截断超长文本 (最大不超过 512)
            size_t seq_len = std::min(input_ids.size(), static_cast<size_t>(512));
            input_ids.resize(seq_len);

            if (seq_len > max_seq_len) {
                max_seq_len = seq_len;
            }
            all_input_ids.push_back(std::move(input_ids));
        }

        // 扁平化填充 (Padding) -> ONNX 只接受 1D 数组
        // 直接初始化为 0，巧妙地完成了 PAD 动作 (0 是 token_type 和 PAD 的默认值)
        std::vector<int64_t> flat_input_ids(batch_size * max_seq_len, 0);
        std::vector<int64_t> flat_attention_mask(batch_size * max_seq_len, 0);
        std::vector<int64_t> flat_token_type_ids(batch_size * max_seq_len, 0);

        for (int i = 0; i < batch_size; ++i) {
            const auto& ids = all_input_ids[i];
            size_t offset = i * max_seq_len; // 计算当前句子在 1D 数组里的起始起点

            for (size_t j = 0; j < ids.size(); ++j) {
                flat_input_ids[offset + j] = ids[j];
                flat_attention_mask[offset + j] = 1; // 真实文字的 Mask 是 1
                // token_type_ids 保持为 0
            }
        }

        //构造 Batch 维度的 Tensor (Shape 变成了 [batch_size, max_seq_len])
        std::vector<int64_t> input_shape = {batch_size, static_cast<int64_t>(max_seq_len)};
        auto* memoryInfo = static_cast<Ort::MemoryInfo*>(m_memoryInfo);

        Ort::Value input_tensor = Ort::Value::CreateTensor<int64_t>(
            *memoryInfo, flat_input_ids.data(), flat_input_ids.size(), input_shape.data(), input_shape.size());
        Ort::Value attention_tensor = Ort::Value::CreateTensor<int64_t>(
            *memoryInfo, flat_attention_mask.data(), flat_attention_mask.size(), input_shape.data(), input_shape.size());
        Ort::Value token_type_tensor = Ort::Value::CreateTensor<int64_t>(
            *memoryInfo, flat_token_type_ids.data(), flat_token_type_ids.size(), input_shape.data(), input_shape.size());

        const char* input_names[] = {"input_ids", "attention_mask", "token_type_ids"};

        auto* session = static_cast<Ort::Session*>(m_session);
        Ort::AllocatorWithDefaultOptions allocator;
        auto out_name_allocated = session->GetOutputNameAllocated(0, allocator);
        const char* output_names[] = { out_name_allocated.get() };

        std::array<Ort::Value, 3> input_tensors = {
            std::move(input_tensor),
            std::move(attention_tensor),
            std::move(token_type_tensor)
        };

        // 释放显卡/CPU一次性推理所有句子
        auto output_tensors = session->Run(Ort::RunOptions{}, input_names, input_tensors.data(), 3, output_names, 1);

        // 偏移计算
        float* floatarr = output_tensors.front().GetTensorMutableData<float>();
        auto shapeInfo = output_tensors.front().GetTensorTypeAndShapeInfo().GetShape();

        size_t hidden_size = shapeInfo.back(); // 最后一维永远是向量维度 (如 768 或 1024)
        // 动态判断模型返回的是 [batch, hidden] 还是 [batch, seq, hidden]
        size_t seq_len_out = (shapeInfo.size() == 3) ? shapeInfo[1] : 1;

        results.reserve(batch_size);
        for (int i = 0; i < batch_size; ++i) {
            // 计算当前句子的 CLS 向量在巨型 float 数组里的绝对地址
            size_t start_idx = i * seq_len_out * hidden_size;

            std::vector<float> emb(floatarr + start_idx, floatarr + start_idx + hidden_size);

            // 归一化
            normalizeVector(emb);
            results.push_back(std::move(emb));
        }

    } catch (const std::exception& e) {
        ExceptHandler::getInstance().reportError(ErrorCode::VectorIndexError,
                                                 QString("批量提取特征失败: ") + e.what());
    }

    return results;
}

void SemanticExtract::normalizeVector(std::vector<float>& vec) {
    float norm = 0.0f;
    for (float v : vec) {
        norm += v * v;
    }
    norm = std::sqrt(norm);
    if (norm > 0) {
        for (float& v : vec) {
            v /= norm;
        }
    }
}
