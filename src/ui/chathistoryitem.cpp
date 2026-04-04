#include "chathistoryitem.h"

ChatHistoryItem::ChatHistoryItem(int sessionId, const QString& title, const QString& createTime, QWidget *parent)
    : QWidget(parent), m_sessionId(sessionId)
{
    QHBoxLayout *layouth = new QHBoxLayout(this);
    QVBoxLayout *layoutv = new QVBoxLayout();
    layouth->setContentsMargins(10, 5, 10, 5);

    m_lblTitle = new QLabel(title, this);
    m_lblTitle->setStyleSheet("font-weight: bold; font-size: 14px;");

    // 既然已经是 QString 了，直接传给 QLabel 即可，不需要 QDateTime 转换了！
    m_lblTime = new QLabel(createTime, this);
    m_lblTime->setStyleSheet("color: gray; font-size: 11px;");

    m_btnDelete = new QPushButton("×", this);
    m_btnDelete->setFixedSize(40, 40);
    m_btnDelete->setCursor(Qt::PointingHandCursor);
    m_btnDelete->setStyleSheet("QPushButton { border: none; background-color: transparent; color: #94A3B8; font-weight: bold; font-size: 16px; }"
                               "QPushButton:hover { background-color: #FEE2E2; color: #EF4444; border-radius: 6px; }");
    layoutv->addWidget(m_lblTitle, 1);
    layoutv->addWidget(m_lblTime, 0);
    layouth->addLayout(layoutv);
    layouth->addWidget(m_btnDelete, 0);

    connect(m_btnDelete, &QPushButton::clicked, this, [this, title]() {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "确认删除",
                                      QString("确定要永久删除对话 '%1' 吗？").arg(title),
                                      QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            emit deleteRequested(m_sessionId);
        }
    });

    // 设置右键菜单策略
    setContextMenuPolicy(Qt::DefaultContextMenu);
}


void ChatHistoryItem::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    // 添加重命名选项
    QAction *renameAction = menu.addAction("✏️ 重命名");
    renameAction->setIconVisibleInMenu(false);

    // 添加删除选项
    QAction *deleteAction = menu.addAction("🗑️ 删除");
    deleteAction->setIconVisibleInMenu(false);

    // 添加分隔线（可选）
    menu.addSeparator();

    // 添加信息查看（可选）
    QAction *infoAction = menu.addAction("ℹ️ 查看信息");

    // 显示菜单并获取用户选择
    QAction *selectedAction = menu.exec(event->globalPos());

    if (selectedAction == renameAction) {
        // 弹出重命名对话框
        bool ok;
        QString newTitle = QInputDialog::getText(this,
                                                 "重命名对话",
                                                 "请输入新的对话名称:",
                                                 QLineEdit::Normal,
                                                 m_lblTitle->text(),
                                                 &ok);
        if (ok && !newTitle.isEmpty()) {
            // 发送重命名信号
            emit renameRequested(m_sessionId, newTitle);
            // 立即更新UI显示
            m_lblTitle->setText(newTitle);
        }
    }
    else if (selectedAction == deleteAction) {
        // 触发删除确认
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "确认删除",
                                      QString("确定要永久删除对话 '%1' 吗？").arg(m_lblTitle->text()),
                                      QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            emit deleteRequested(m_sessionId);
        }
    }
    else if (selectedAction == infoAction) {
        // 显示会话信息
        QMessageBox::information(this, "会话信息",
                                 QString("会话ID: %1\n标题: %2\n创建时间: %3")
                                     .arg(m_sessionId)
                                     .arg(m_lblTitle->text())
                                     .arg(m_lblTime->text()));
    }

    event->accept();
}

void ChatHistoryItem::updateTitle(const QString& newTitle)
{
    m_lblTitle->setText(newTitle);
}

