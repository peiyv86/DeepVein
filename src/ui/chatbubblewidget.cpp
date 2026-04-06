#include "chatbubblewidget.h"
#include <QDesktopServices>

ChatBubbleWidget::ChatBubbleWidget(const QString& text, bool isUser, QWidget *parent)
    : QWidget(parent)
{
    // 最外层布局控制左右靠齐
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 5, 10, 5);

    //创边框QFrame背负样式
    QFrame* bubbleFrame = new QFrame(this);
    QVBoxLayout* frameLayout = new QVBoxLayout(bubbleFrame);
    frameLayout->setContentsMargins(12, 8, 12, 8); // 内边距转移到 Frame 上

    m_label = new QLabel(bubbleFrame);
    m_label->setWordWrap(true);
    m_label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_label->setOpenExternalLinks(false);
    m_label->setStyleSheet("background: transparent;");
    frameLayout->addWidget(m_label);

    //超链接点击信号
    connect(m_label, &QLabel::linkActivated, this, [this](const QString& link) {
        emit linkClicked(QUrl(link));
    });

    QString frameStyle;
    if (isUser) {
        frameStyle = "QFrame { background-color: #EFF6FF; border-radius: 18px; padding: 4px 10px; }";
        mainLayout->addStretch();
        mainLayout->addWidget(bubbleFrame);
    } else {
        frameStyle = "QFrame { background-color: #DBEAFE; border-radius: 18px; padding: 4px 10px; }";
        mainLayout->addWidget(bubbleFrame);
        mainLayout->addStretch();
    }

    bubbleFrame->setStyleSheet(frameStyle);
    bubbleFrame->setMaximumWidth(600); //最大宽度

    m_label->setText(text);
}

void ChatBubbleWidget::updateContent(const QString& htmlText) {
    m_label->setText(htmlText);
    this->adjustSize(); //撑开
}


