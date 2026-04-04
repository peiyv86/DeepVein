# DeepVein (深脉)

<p align="left">
  <img src="https://img.shields.io/badge/Qt-6.x-41CD52.svg?style=flat&logo=qt" alt="Qt 6.x">
  <img src="https://img.shields.io/badge/C++-17-00599C.svg?style=flat&logo=c%2B%2B" alt="C++17">
  <img src="https://img.shields.io/badge/License-MIT-blue.svg" alt="License MIT">
  <img src="https://img.shields.io/badge/Platform-Windows%20|%20macOS%20|%20Linux-lightgrey.svg" alt="Platform">
</p>

## 简介 (Introduction)

DeepVein 是一款基于 C++ 和 Qt6 构建的高性能、全本地化 RAG（检索增强生成）桌面大模型助手。

与传统的本地搜索工具只能依靠关键词 **“找文件”** 不同，DeepVein 实现了从“找文件”到 **“找答案”** 的跃升。它不仅提供绝对数据隐私保护，更将复杂的智能意图路由、极速的双路召回（全文+向量）与大语言模型推理深度融合。

告别在海量搜索结果中人工翻找的痛苦——您只需使用自然语言提问，DeepVein 就能像您的专属研究助理一样，理解语义、穿透文档、提取实体，并直接将提炼、对比好的最终答案递交到您手中。

**核心特性：**

  * **数据隐私安全**：基于 Ollama 驱动本地大语言模型，完全离线，敏感文档（简历、财务报表、内部代码）永远不会离开您的电脑。
  * **智能意图路由 (Smart Routing)**：已组装六个 action（精确匹配、语义搜索、单文档洞察、混合对比等），根据 Prompt 动态决定检索策略，而非死板的全文搜索。
  * **C++ 极致性能**：
      * **双路召回引擎**：SQLite FTS5 全文检索引擎 + HNSWlib 内存级高维向量图索引。
      * **内存级优化**：采用 C++17 `QStringView` 零拷贝 JSON 解析，大幅降低大段文本的堆内存开销。

-----


## 安装 / 依赖 (Installation & Dependencies)

### 系统要求

  * 操作系统：Windows 10/11，macOS 12+, Ubuntu 20.04+
  * 编译器：支持 C++17 的编译器（MSVC 2019+, GCC 9+, Clang 10+）

### 环境准备

1.  **安装 CMake** (>= 3.16)
2.  **安装 Qt 6**：必须包含 `Core`, `Gui`, `Widgets`, `Network`, `Sql` 模块。
3.  **安装 Ollama**：请前往 [Ollama 官网](https://ollama.com/) 下载并安装。

### 模型资产下载 (Models Download)
为保持代码仓库的轻量化，本项目核心运行所需的 `.onnx` 模型权重文件单独托管于云端。在编译运行前，请务必下载资产包并正确放置。

| 下载源 | 链接 |
| :--- | :---  |
| **ModelScope** | [🔗 点击下载](https://www.modelscope.cn/models/PEIYV0903/DeepVein-Assets/files) |
| **Hugging Face** | [🔗 点击下载](https://huggingface.co/PEIYV86/DeepVein-Assets/tree/main) |

**放置说明：**
请将下载的资产解压，并放置于项目根目录的 `assets/` 路径下，确保最终目录结构如下：
```text
DeepVein/
└── assets/
    └── models/
        ├── bge_onnx/
        ├── bge_reranker/
        └── ocr_models/
````

### 编译与运行

```bash
# 1. 克隆仓库
git clone [https://github.com/YourName/DeepVein.git](https://github.com/YourName/DeepVein.git)
cd DeepVein

# 2. 放置模型资产（参考上文的模型下载说明）

# 3. 创建构建目录
mkdir build && cd build

# 4. 生成项目文件并编译
cmake ..
cmake --build . --config Release

# 5. 运行程序
./DeepVein
```

#### 跨平台编译注意事项

DeepVein 的核心 C++ 逻辑完全跨平台，但如果您在非 Windows 环境下编译，请注意调整以下几点：

  * **动态库链接调整**：当前的 `CMakeLists.txt` 中包含部分 Windows 专属的底层网络库链接（如 `ntdll`, `bcrypt`, `ws2_32`）。在 macOS 或 Linux 下编译时，请移除这些依赖，或将其包裹在 `if(MSVC)` 判断中。
  * **第三方库替换**：请确保 `third_party/lib` 和 `third_party/bin` 目录下放置了对应操作系统的预编译库（例如将 `.lib/.dll` 替换为 `.so` 或 `.dylib`）。
  * **路径大小写敏感**：Linux/macOS 对文件路径严格区分大小写。如果在资源加载或扫描时报错，请检查您的物理文件名与代码中引用的名称大小写是否完全一致。

-----

## 用法说明 (Usage)

### 第一步：初始化与本地建库

1.  **启动后台 AI**：在运行 DeepVein 前，请确保 Ollama 已在系统后台运行，并已拉取所需的语言模型（推荐 `qwen2.5:7b` 或 `deepseek-r1:7b`）。
2.  **配置数据源**：打开 DeepVein，点击左侧边栏的 **⚙️ 设置**。
      * 指定 `扫描根文件夹`（存放您本地 txt、pdf、docx 等知识文档的目录）。
      * 指定 `向量库保存位置`（存放 `.bin` 高维图索引文件的目录，建议置于高速 SSD）。
3.  **一键构建**：点击 **“\<\<\< 立即同步 / 重建知识库”**。DeepVein 底层的多线程流水线将自动完成文本切片、向量嵌入（Embedding）与 SQLite 存储。此时您可以切回聊天界面正常交流，建库将在后台静默完成。

### 第二步：本地交互场景

系统内置了智能路由引擎，您无需手动切换模式，DeepVein 会根据您的 Prompt 自动选择最佳工作流：

  * **语义知识检索**

      * *触发方式*：直接询问概念或问题。
      * *示例*：“归并排序的底层原理是什么？”或“查找关于 2024 年 Q3 季度的营销策略资料”。
      * *行为*：系统将触发双路召回，提取相关文件切片，标注引用来源（文献出处），并结合大模型给出专业解答。

  * **精准文件定位**

      * *触发方式*：明确指出要找某个文件。
      * *示例*：“帮我找出名为‘秋招简历’的文件”。
      * *行为*：系统跳过耗时的语义检索，直接利用 SQLite 数据库光速锁定目标文件，并生成可点击唤醒操作系统的外部链接。

  * **单点文件深度洞察**

      * *触发方式*：点击输入框左侧的 `📎` 图标，上传一份不在知识库内的临时文档（长文本会自动触发启发式采样）。
      * *示例*：上传一份测试报告，提问“总结这份报告中提到的三个最严重的 Bug”。
      * *行为*：系统会暂时屏蔽全局知识库，强制模型对当前挂载的附件进行专项分析。

  * **常规 AI 对话**

      * *触发方式*：进行无检索需求的日常交流。
      * *示例*：“帮我用 C++ 写一个单例模式”。
      * *行为*：直接调用本地大模型进行纯粹的文本生成。

-----

## 配置指南 (Configuration)

在“设置”面板中，您可以自定义以下核心参数：

  * **本地大模型**：下拉列表会自动轮询 Ollama 已安装的模型。
  * **端口设置**：Ollama API 的默认端口为 `11434`，如遇端口冲突可在此修改。
  * **界面语言**：支持简体中文与 English（动态切换防数据丢失）。
  * **存储路径**：
      * `扫描根文件夹`：您存放 txt、md、pdf 等知识文档的源目录。
      * `向量库保存位置`：HNSWlib 的 `.bin` 索引文件存放地，建议放在高速 SSD 盘。
      * `对话存档位置`：SQLite `pdan.db` 的存放位置。

-----

## 项目结构 (Project Structure)

```text
DeepVein/
├── assets/             # 模型资产目录 (需单独下载)
├── src/
│   ├── core/           # 全局类型定义 (global_defs.h)、黑匣子异常单例 (ExceptHandler)
│   ├── llm_tools/      # 核心逻辑：提示词组装、流式截断解析、意图提取 (LlmTools)
│   ├── storage/        # 本地数据库连接池 (Datamanager)、高维图索引引擎 (VectorDB)
│   ├── task/           # 双轨制任务路由器、RAG 工作流 (TaskFactory)
│   ├── ui/             # 自适应富文本气泡 (ChatBubbleWidget)、会话列表项
│   └── mainwidget.cpp  # 主界面控制器与 UI 渲染调度
├── resources/          # UI 资产：app.qss 样式表、i18n 语言包 (.qm)、Logo 图像
└── third_party/        # 第三方依赖：HNSWlib (Header-only), nlohmann/json
```

-----

## 当前进度与未来方向 (Roadmap)

DeepVein 正在迭代中。**请注意，Agent 工作流（Workflow）功能目前处于实验性阶段，将在后续版本逐渐实现。**

### 已完成 (Completed)

  - [x] **架构解耦**：完成 UI 层、网络层与数据存取层的完全解耦。
  - [x] **检索增强 (RAG)**：融合 SQLite FTS5 全文匹配与 HNSWlib 向量相似度检索。
  - [x] **全局异常监控**：实装线程安全的 `ExceptHandler` 单例，所有底层崩溃自动输出至 `logs/pdan_error.log`。

### 进行中 (In Progress)

  - [ ] **幻觉抗性**：针对小模型（如 1.5B 参数）增加完善的 JSON 断片兜底机制，并调整prompt规范模型输出。
  - [ ] **多模态 OCR 集成**：接入 RapidOCR 引擎，支持纯图片 PDF 和截图的语义提取。
  - [ ] **模型热切换优化**：在对话中根据任务复杂度（如意图路由用 1.5B，深度洞察用 7B+）自动调度不同模型。

### 未来方向 (Future - v1.0 Milestone)

  - [ ] **Agent Workflow Engine (智能工作流引擎)**：
      * **这是 DeepVein 未来的核心进化方向。** 计划引入全局黑板(Blackboard)机制，支持大模型自主规划多步任务（如：“提取文档 A 的人员名单 -\> 检索数据库查询其绩效 -\> 对比并生成表格”）。
      * 支持动态挂载外部工具（Tool Calling API）。

-----

## 开发 / 贡献指南 (Contributing)

我们非常欢迎社区开发者参与贡献！

1.  **代码规范**：C++ 代码遵循 Google C++ Style Guide，变量命名采用驼峰式 (CamelCase)。
2.  **提交 PR**：请先 Fork 本仓库，在一个新分支上进行修改，并确保所有新增功能都不破坏 `ExceptHandler` 的日志闭环。

## 已知问题 (Known Issues)

由于目前项目还处于初期开发中，以下是当前版本（v0.x）中已知的一些技术局限性及触发时的临时解决方案：

  * **底层引擎的中文路径乱码崩溃**

      * **现象**：如果软件安装路径或选定的知识库路径中包含某些特殊的生僻中文字符或 Emoji，底层的 C++ 第三方引擎（如某些 PDF 解析库或未来的 ONNX 运行时）可能会因 Windows 默认的本地字符集编码（GBK）与 UTF-8 转换失败而导致文件找不到。
      * **临时解决方案**：部署时将本软件及知识库扫描目录放在纯英文路径下。

  * **大模型输出混乱与 JSON 幻觉 (LLM Hallucination)**

      * **现象**：当用户在设置中选择了参数量过小（如 1.5B/3B）或非指令微调的模型时，模型极有可能无法按照 `Semantic Router API` 规定的格式输出 JSON。可能会出现篡改键值名、混杂大量废话，重复输出，甚至输出残缺的 JSON 字符串。
      * **临时解决方案**：系统底层已内置了字符解析机制来尝试缓解问题。但为了使用稳定，强烈建议使用 **7B 及以上参数量的模型**（如 `qwen2.5:7b` 或 `deepseek-r1:7b`）。

  * **意图路由漂移 (Routing Failure)**

      * **现象**：当用户的提问指令模糊或关键词较少（例如仅输入“测试”或“你好”），大模型可能会在 `SemanticSearch`（知识库检索）和`ExactMatch`（精准匹配）或 `DirectChat`（直接闲聊）之间发生误判，导致明明想查资料却直接进入了闲聊模式。
      * **临时解决方案**：在提问时增加明确的“动作词”，例如使用“帮我**找一下**关于XXX的资料”或“**搜索**XXX文件”，这会强制触发系统的路由硬锁。

  * **UI 渲染与高分屏适配**

      * **现象**：在某些分辨率极高（4K，\>200% 缩放）的 Windows 设备上，`QListWidget` 滚动条可能会出现细微的像素对齐偏差，或在流式输出超大段落时出现偶发的气泡高度重算延迟。
      * **临时解决方案**：滚动鼠标滚轮或拉伸一下窗口，即可触发 Qt 布局引擎的二次重绘恢复正常。

  * **长文档与 OCR 提取盲区**

      * **现象**：首次加载含有大量图片扫描件的长 PDF 时，如果尚未开启 OCR 功能，或者 PDF 内部文本层被加密，切片提取可能返回空文本。
      * **临时解决方案**：暂需依赖纯文本的 PDF，多模态 OCR 提取模块（基于 RapidOCR）正在开发中。

  * **大规模索引内存分配/映射失败 (OOM / Malloc Error)**

      * **现象**：后台执行“重建知识库”时，如果扫描的本地文件夹极大（包含上万个文档），HNSWlib 向量引擎在尝试将整个高维图结构载入或写入物理内存时，可能会在小内存（如 8GB RAM）电脑上触发内存分配失败（抛出 `[ERROR: VectorIndexError]`）。
      * **临时解决方案**：请确保电脑有足够的可用运行内存，或在系统设置中适当增加 Windows 的虚拟内存（Page File）。未来版本将引入磁盘与内存的混合存储策略。

-----

## 许可证 (License)

采用 [MIT License](https://www.google.com/search?q=LICENSE) 许可协议。您可以自由地在商业或非商业项目中修改和分发本代码。

-----

## 致谢 (Acknowledgements)

DeepVein 的诞生与发展离不开以下优秀开源项目与社区的支持，在此向所有开源贡献者表示最诚挚的感谢：

**🧠 核心引擎与大模型支持**
* [Ollama](https://github.com/ollama/ollama) - 极其优秀的本地大模型运行与管理引擎。
* [ONNX Runtime](https://github.com/microsoft/onnxruntime) - 微软开源的高性能机器学习模型推理引擎（为本地 Embedding 和 Rerank 提供硬件加速）。

**🔍 检索与数据库**
* [Hnswlib](https://github.com/nmslib/hnswlib) - 高效的纯内存高维向量近似最近邻（ANN）搜索库。
* [SQLite](https://www.sqlite.org/) - 轻量、高可靠性的本地关系型数据库引擎（提供 FTS5 全文搜索支持）。

**📄 视觉与文档解析**
* [RapidOCR](https://github.com/RapidAI/RapidOCR) - 高效、开箱即用的跨平台 OCR 部署库。
* [PaddleOCR](https://github.com/PaddlePaddle/PaddleOCR) - 百度开源的工业级优秀多语言 OCR 模型。
* [PDFium](https://pdfium.googlesource.com/pdfium/) - Google 开源的高性能 PDF 渲染与解析引擎。
* [Pandoc](https://pandoc.org/) - 强大的通用文档格式转换工具。
* [OpenCV](https://opencv.org/) - 开源计算机视觉库（为图像预处理提供底层支持）。

**🛠️ 基础框架与工具**
* [Qt](https://www.qt.io/) - 强大的跨平台 C++ 应用程序开发框架。
* [nlohmann/json](https://github.com/nlohmann/json) - 优雅且高效的 C++ JSON 解析库。
* [pugixml](https://pugixml.org/) - 轻量级、极速的 C++ XML 解析库。

**🌟 特别鸣谢模型支持**
* [BAAI (北京智源人工智能研究院)](https://huggingface.co/BAAI) - 提供卓越的 BGE 语义向量提取模型（Embedding）与重排模型（Reranker）。

<!-- end list -->

```
