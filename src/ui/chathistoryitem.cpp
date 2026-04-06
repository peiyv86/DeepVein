#include "chathistoryitem.h"

ChatHistoryItem::ChatHistoryItem(int sessionId, const QString& title, const QString& createTime, QWidget *parent)
    : QWidget(parent), _sessionId(sessionId)
{
    QHBoxLayout *layouth = new QHBoxLayout(this);
    QVBoxLayout *layoutv = new QVBoxLayout();
    layouth->setContentsMargins(10, 5, 10, 5);

    _lbTitle = new QLabel(title, this);
    _lbTitle->setStyleSheet("font-weight: bold; font-size: 14px;");

    _lbTime = new QLabel(createTime, this);
    _lbTime->setStyleSheet("color: gray; font-size: 11px;");

    _btnDelete = new QPushButton("🗑️", this);
    _btnDelete->setFixedSize(30, 30);
    _btnDelete->setCursor(Qt::PointingHandCursor);
    _btnDelete->setStyleSheet("QPushButton { border: none; background-color: transparent; color: #94A3B8; font-weight: bold; font-size: 16px; }"
                               "QPushButton:hover { background-color: #FEE2E2; color: #EF4444; border-radius: 6px; }");
    layoutv->addWidget(_lbTitle, 1);
    layoutv->addWidget(_lbTime, 0);
    layouth->addLayout(layoutv);
    layouth->addWidget(_btnDelete, 0);

    connect(_btnDelete, &QPushButton::clicked, this, [this, title]() {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "确认删除",
                                      QString("确定要永久删除对话 '%1' 吗？").arg(title),
                                      QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            emit deleteRequested(_sessionId);
        }
    });

    // 设置右键菜单策略
    setContextMenuPolicy(Qt::DefaultContextMenu);
}


void ChatHistoryItem::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    // 添加重命名选项
    QAction *renameAct = menu.addAction("✏️ 重命名");
    renameAction->setIconVisibleInMenu(false);

    // 添加删除选项
    QAction *deleteAct = menu.addAction("🗑️ 删除");
    deleteAction->setIconVisibleInMenu(false);

    // 添加分隔线（可选）
    menu.addSeparator();

    // 添加信息查看（可选）
    QAction *infoAct = menu.addAction("ℹ️ 查看信息");

    // 显示菜单并获取用户选择
    QAction *selAct = menu.exec(event->globalPos());

    if (selAct == renameAct) {
        // 弹出重命名对话框
        bool ok;
        QString newTitle = QInputDialog::getText(this,
                                                 "重命名对话",
                                                 "请输入新的对话名称:",
                                                 QLineEdit::Normal,
                                                 _lbTitle->text(),
                                                 &ok);
        if (ok && !newTitle.isEmpty()) {
            // 发送重命名信号
            emit renameRequested(_sessionId, newTitle);
            // 立即更新UI显示
            _lbTitle->setText(newTitle);
        }
    }
    else if (selAct == deleteAct) {
        // 触发删除确认
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "确认删除",
                                      QString("确定要永久删除对话 '%1' 吗？").arg(_lbTitle->text()),
                                      QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            emit deleteRequested(_sessionId);
        }
    }
    else if (selAct == infoAct) {
        // 显示会话信息
        QMessageBox::information(this, "会话信息",
                                 QString("会话ID: %1\n标题: %2\n创建时间: %3")
                                     .arg(_sessionId)
                                     .arg(_lbTitle->text())
                                     .arg(_lbTime->text()));
    }

    event->accept();
}

void ChatHistoryItem::updateTitle(const QString& newTitle)
{
    _lbTitle->setText(newTitle);
}

