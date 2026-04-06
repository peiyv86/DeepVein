#ifndef CHATHISTORYITE_H
#define CHATHISTORYITE_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QString>
#include <QDateTime>
#include <QMessageBox>
#include <QMenu>
#include <QContextMenuEvent>
#include <QInputDialog>

class ChatHistoryItem : public QWidget
{
    Q_OBJECT
public:
    // 第三个参数改成 const QString& createTime
    explicit ChatHistoryItem(int sessionId, const QString& title, const QString& createTime, QWidget *parent = nullptr);
    // 添加更新标题的方法
    void updateTitle(const QString& newTitle);
    void contextMenuEvent(QContextMenuEvent *event) override;  // 重写右键菜单事件

signals:
    void deleteRequested(int sessionId);
    void renameRequested(int sessionId, const QString& newTitle);
private:
    int _sessionId;
    QLabel *_lbTitle;
    QLabel *_lbTime;
    QPushButton *_btnDelete;
};

#endif // CHATHISTORYITE_H
