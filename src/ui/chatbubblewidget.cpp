#include "chatbubblewidget.h"
#include <QDesktopServices>

ChatBubbleWidget::ChatBubbleWidget(const QString& text, bool isUser, QWidget *parent)
    : QWidget(parent)
{
    // 最外层布局控制左右靠齐
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 5, 10, 5);

    // 创建一个物理边框 QFrame，专门用来背负样式
    QFrame* bubbleFrame = new QFrame(this);
    QVBoxLayout* frameLayout = new QVBoxLayout(bubbleFrame);
    frameLayout->setContentsMargins(12, 8, 12, 8); // 内边距转移到 Frame 上

    // 文字载体 QLabel
    m_label = new QLabel(bubbleFrame);
    m_label->setWordWrap(true);
    m_label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_label->setOpenExternalLinks(false);

    // 强制 Label 背景透明，完全露出底层QFrame颜色
    m_label->setStyleSheet("background: transparent;");
    frameLayout->addWidget(m_label);

    // 绑定超链接点击信号
    connect(m_label, &QLabel::linkActivated, this, [this](const QString& link) {
        emit linkClicked(QUrl(link));
    });

    // 为 QFrame 设计 QSS 圆角和背景
    // QString frameStyle;
    // if (isUser) {
    //     // 用户绿
    //     frameStyle = "QFrame { background-color: #95ec69; border-radius: 10px; }";

    //     mainLayout->addStretch();
    //     mainLayout->addWidget(bubbleFrame);
    // } else {
    //     // 将 AI 气泡去掉突兀的实线边框
    //     frameStyle = "QFrame { background-color: #f0f4f8; border-radius: 10px; }";
    //     mainLayout->addWidget(bubbleFrame);
    //     mainLayout->addStretch();
    // }
    QString frameStyle;
    if (isUser) {
        // 放弃微信绿
        frameStyle = "QFrame { background-color: #FFF3EB; border-radius: 10px; }";
        mainLayout->addStretch();
        mainLayout->addWidget(bubbleFrame);
    } else {
        // AI 气泡使用与主界面卡片一致的纯白/极浅灰，加上边框增加层次感
        frameStyle = "QFrame { background-color: #E2E8F0; border-radius: 10px; }";
        mainLayout->addWidget(bubbleFrame);
        mainLayout->addStretch();
    }

    bubbleFrame->setStyleSheet(frameStyle);
    bubbleFrame->setMaximumWidth(600); // 限制整个泡泡的最大宽度

    m_label->setText(text);
}

void ChatBubbleWidget::updateContent(const QString& htmlText) {
    m_label->setText(htmlText);
    this->adjustSize(); // 撑开布局
}


