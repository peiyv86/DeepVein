#ifndef MAINWIDGET_H
#define MAINWIDGET_H

// ==========================================
// 1. Qt Core & Base
// ==========================================
#include <QWidget>
#include <QEvent>
#include <QTimer>
#include <QDebug>
#include <QThreadPool>
#include <QDateTime>
#include <QUrl>
#include <QPointer>
#include <QTranslator>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>

// ==========================================
// 2. Qt GUI & Widgets
// ==========================================
#include <QListWidgetItem>
#include <QFileDialog>
#include <QDesktopServices>
#include <QMessageBox>

// ==========================================
// 3. Project Core & LLM
// ==========================================
#include "core/global_defs.h"
#include "storage/datamanager.h"
#include "llm/aiclient.h"

// ==========================================
// 4. File IO & Tasks
// ==========================================
#include "file_io/filescanner.h"
#include "file_io/tasks/build_index_task.h"
#include "file_io/file_factory.h"
#include "task/task_factory.h"
#include "task/workflow/workflow_engine.h"

// ==========================================
// 5. UI Components
// ==========================================
#include "ui/chatbubblewidget.h"
#include "chathistoryitem.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWidget; }
QT_END_NAMESPACE

class MainWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MainWidget(QWidget *parent = nullptr);
    ~MainWidget();

protected:
    // i18n: 重写事件分发，捕获系统语言切换事件
    void changeEvent(QEvent *event) override;

private slots:
    // --- 导航与窗口控制 ---
    void on_btnSettings_clicked();
    void on_btnBackToChat_clicked();
    void on_btnToggleSidebar_clicked();

    // --- 聊天与会话管理 ---
    void on_btnNewChat_clicked();
    void on_listChatHistory_itemClicked(QListWidgetItem *item);
    void on_btnSend_clicked();

    // --- 附件管理 ---
    void on_btnUploadFile_clicked();
    void on_btn_DeleteFile_clicked();

    // --- 设置页面操作 ---
    void on_btnBrowseScanPath_clicked();
    void on_btnBrowseVectorPath_clicked();
    void on_btnBrowseHistoryPath_clicked();
    void on_btnRebuildDB_clicked();
    void on_btnSetPort_clicked();
    void on_btnRefreshModels_clicked();
    void on_comboLanguage_currentIndexChanged(int index); // 🚀 语言切换触发

    // --- AI 网络流回调 ---
    void onErrorOccurred(const QString& errorMsg);
    void onKeywordParsed(ParsedIntent intent);
    void onAiChunkReceived(const QString &chunk, int type = 0);
    void onAiAnswerFinished();

private:
    Ui::MainWidget *ui;

    // --- 状态标志与定时器 ---
    QTimer *ollamaCheckTimer;
    int settingChanger;
    bool m_isSidebarExpanded;

    // --- 设置路径缓存 ---
    QString SCANPATH;
    QString VECTORPATH;
    QString DIALOGPATH;

    // --- AI 会话上下文 ---
    int m_currentSessionId;
    int m_generatingSessionId;      // 记录 AI 当前究竟在给哪个会话回答
    QString m_currentInput;
    QString m_streamBuffer;         // 积攒 AI 生成的纯文本
    QString m_rawStreamBuffer;      // 存储网络原始文本 (双缓存)
    QString m_finalHtmlBuffer;      // 存储解析后的完美 HTML
    QPointer<ChatBubbleWidget> m_currentAiBubble; // 安全智能指针

    // --- 附件与记忆上下文 ---
    QString m_uploadedFilePath;
    QString m_uploadedFileText;
    QString m_currentHistoryContext;

    // --- i18n 国际化 ---
    QTranslator m_translator;    // 翻译器实例
    void retranslateDynamicUi(); // 刷新代码中手写的动态字符串

    // --- 内部辅助函数 ---
    void refreshModels();
    void refreshSidebar();
    void appendChatMessage(const QString &text, bool isUser);
    void onRenameRequested(int sessionId, const QString& newTitle);
    QString parseAiStreamToHtml(const QString& rawStream);
    QString buildRecentHistoryContext(int sessionId, int maxRounds);
};

#endif // MAINWIDGET_H
