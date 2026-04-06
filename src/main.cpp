#include <QApplication>
#include <QDir>
#include <QDebug>

#include "ui/mainwidget.h"
#include "storage/datamanager.h"
#include "storage/vector_db.h"
#include "convert/semanticextract.h"
#include "convert/reranker_engine.h" // 引入精排引擎头文件
#include "convert/ocrengine.h"
#include "llm/aiclient.h"

int main(int argc, char *argv[])
{
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    QFont globalFont = app.font();
    globalFont.setFamily("Microsoft YaHei");
    globalFont.setPointSize(10);
    app.setFont(globalFont);

    qDebug() << "-系统启动中...";
    QString appPath = QApplication::applicationDirPath();// 路径初始化
    QString dbDir = appPath + "/data/db";// 数据存储路径
    QString vectorDir = appPath + "/data/vector";
    // 模型资源路径
    QString modelPath     = appPath + "/models/bge_onnx/model.onnx";
    QString tokenizerPath = appPath + "/models/bge_onnx/tokenizer.json";
    // Reranker 精排模型路径
    QString rerankModelPath     = appPath + "/models/bge_reranker/model.onnx";
    QString rerankTokenizerPath = appPath + "/models/bge_reranker/tokenizer.json";
    // OCR 模型文件夹路径
    QString ocrModelsPath = appPath + "/models/ocr_models";
    // 自动创建必需的读写文件夹
    QDir dir;
    if (!dir.exists(dbDir)) dir.mkpath(dbDir);
    if (!dir.exists(vectorDir)) dir.mkpath(vectorDir);
    // 组件初始化
    // 初始化 SQLite 数据库 (存放文本和对话)
    QString dbFile = dbDir + "/pdan_main.db";
    if (!Datamanager::getInstance().init(dbFile)) {
        qCritical() << "[FATAL]数据库初始化失败-程序终止。";
        return -1;
    }
    qDebug() << "[SQLite] 数据库就绪";
    // 初始化向量库 (512维对应 BGE-large)
    QString vectorFile = vectorDir + "/hnsw_index.bin";
    if (!VectorDB::getInstance().init(vectorFile, 512, 10000)) {
        qCritical() << "[FATAL]向量数据库初始化失败-请检查系统内存。";
        return -1;
    }
    qDebug() << "[HNSW] 向量索引库就绪";
    // 预加载 Embedding 粗排引擎
    if (!SemanticExtract::getInstance().init(modelPath, tokenizerPath)) {
        qWarning() << "[WARN]语义提取引擎加载失败，请检查模型路径：";
        qWarning() << "   -> " << modelPath;
        // 不强制退出，允许无模型状态下纯靠 SQLite 检索
    } else {
        qDebug() << "[BGE] 语义提取引擎已载入";
    }
    // 预加载 Reranker 精排引擎
    if (!RerankerEngine::getInstance().init(rerankModelPath, rerankTokenizerPath)) {
        qWarning() << "[WARN]精排交叉验证引擎加载失败，请检查模型路径：";
        qWarning() << "   -> " << rerankModelPath;
    } else {
        qDebug() << "[BGE-Reranker] 精排引擎已载入";
    }
    // 唤醒 OCR 光学字符识别引擎
    if (!OcrEngine::getInstance().init(ocrModelsPath)) {
        qWarning() << "[WARN]OCR 引擎加载失败-图片解析功能将受限。请检查目录：";
        qWarning() << "   -> " << ocrModelsPath;
    } else {
        qDebug() << "[PP-OCR] 识别引擎就绪";
    }
    // 大模型接口配置
    Aiclient::getInstance().setPORT("11434");
    qDebug() << "[Ollama-API] 端口已挂载 (11434)";
    // 加载 QSS 样式表
    QFile file(":/resources/app.qss");
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream stream(&file);
        app.setStyleSheet(stream.readAll());
        file.close();
    }
    MainWidget w;
    w.show();
    qDebug() << "所有模块自检完成";
    return app.exec();
}
