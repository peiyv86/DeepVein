#ifndef CHATBUBBLEWIDGET_H
#define CHATBUBBLEWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QUrl>

class ChatBubbleWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChatBubbleWidget(const QString& text, bool isUser, QWidget *parent = nullptr);
    void updateContent(const QString& htmlText);
    QString getHtmlText() const { return m_htmlText; } //暴露完整 HTML 文
signals:
    void linkClicked(const QUrl& url); // 转发超链接点击信号

private:
    QLabel* m_label;
    QString m_htmlText; // 暂存原始文本
};

#endif // CHATBUBBLEWIDGET_H
