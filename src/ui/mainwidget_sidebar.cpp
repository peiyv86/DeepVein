#include "mainwidget.h"
#include "ui_mainwidget.h"

//历史记录管理
void MainWidget::on_btnNewChat_clicked()
{
    ui->listWidgetChat->clear();
    int newId = Datamanager::getInstance().createSession(tr("新对话 "));
    if (newId != -1) {
        _curDialogId = newId;
        refreshSidebar();
        ui->labelStatus->setText(tr("🟢 已开启新会话"));
    }
}
//点击存档
void MainWidget::on_listChatHistory_itemClicked(QListWidgetItem *item)
{
    int sId = item->data(Qt::UserRole).toInt();
    _curDialogId = sId;
    ui->listWidgetChat->clear();

    QList<MessageInfo> history = Datamanager::getInstance().getMessagesBySession(sId);
    for (const auto& msg : history) {
        // 由于我们在收发时已经标准化存了 HTML，这里直接扔给气泡就行，完美保留换行
        addChatMsg(msg.content, msg.role == "user");
    }

    // 如果我们切回了正在生成的存档，立刻复活那个气泡
    if (_curDialogId == _nowDialogId) {
        addChatMsg("", false); // 创建一个空壳并捏住指针
        if (_curAiBubble) {
            _curAiBubble->updateContent(_finalHtml); // 把后台积攒的进度灌进去
        }
    }

    ui->labelStatus->setText(tr("🟢 已加载历史记录"));
    ui->listWidgetChat->scrollToBottom();
}


//收缩
void MainWidget::on_btnToggleSidebar_clicked()
{
    // 防连续点击
    ui->btnToggleSidebar->setEnabled(false);

    // 动态获取当前的真实宽度作为起点
    int currentWidth = ui->sidebarWidget->width();
    int expandedWidth = 250;
    int collapsedWidth = 60;

    // 创建并行动画组
    QParallelAnimationGroup *animGroup = new QParallelAnimationGroup(this);

    // 动画1控制最小宽度
    QPropertyAnimation *animMin = new QPropertyAnimation(ui->sidebarWidget, "minimumWidth");
    animMin->setDuration(300);
    animMin->setEasingCurve(QEasingCurve::InOutQuad);
    animGroup->addAnimation(animMin);

    // 动画2控制最大宽度
    QPropertyAnimation *animMax = new QPropertyAnimation(ui->sidebarWidget, "maximumWidth");
    animMax->setDuration(300);
    animMax->setEasingCurve(QEasingCurve::InOutQuad);
    animGroup->addAnimation(animMax);

    if (_isExpand) {
        // 执行收起逻辑
        // 在隐藏内部组件之前，强制锁定当前的最小宽度这样即使组件隐藏了，布局引擎也无法瞬间把侧边栏压扁
        ui->sidebarWidget->setMinimumWidth(currentWidth);
        ui->btnNewChat->hide();
        ui->listChatHistory->hide();
        ui->btnSettings->hide();
        ui->btnToggleSidebar->setText(">");

        // 让最大和最小宽度同步缩小，实现平滑推拉
        animMin->setStartValue(currentWidth);
        animMin->setEndValue(collapsedWidth);

        animMax->setStartValue(currentWidth);
        animMax->setEndValue(collapsedWidth);

        _isExpand = false;
    } else {
        // 执行展开逻辑
        ui->btnToggleSidebar->setText("<<<");

        ui->btnNewChat->show();
        ui->listChatHistory->show();
        ui->btnSettings->show();

        // 让最大和最小宽度同步放大
        animMin->setStartValue(currentWidth);
        animMin->setEndValue(expandedWidth);

        animMax->setStartValue(currentWidth);
        animMax->setEndValue(expandedWidth);

        _isExpand = true;
    }

    connect(animGroup, &QParallelAnimationGroup::finished, this, [=]() {
        ui->btnToggleSidebar->setEnabled(true);
    });

    // 启动动画组，结束后销毁
    animGroup->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWidget::refreshSidebar()
{
    ui->listChatHistory->clear();
    QList<SessionInfo> sessions = Datamanager::getInstance().getAllSessions();

    for (const auto& session : sessions) {
        QListWidgetItem* listItem = new QListWidgetItem(ui->listChatHistory);
        listItem->setSizeHint(QSize(0, 72));
        listItem->setData(Qt::UserRole, session.sessionId);

        ChatHistoryItem* customItem = new ChatHistoryItem(session.sessionId, session.title, session.createTime, this);
        connect(customItem, &ChatHistoryItem::deleteRequested, this, [this, listItem](int sId){
            Datamanager::getInstance().deleteSession(sId);
            delete ui->listChatHistory->takeItem(ui->listChatHistory->row(listItem));
            if (_curDialogId == sId) {
                _curDialogId = -1;
                ui->listWidgetChat->clear();
            }
        });

        //连接重命名信号
        connect(customItem, &ChatHistoryItem::renameRequested,
                this, [this, listItem](int sId, const QString& newTitle){
                    if (Datamanager::getInstance().renameSession(sId, newTitle)) {
                        // 数据库更新成功，UI 已通过 ChatHistoryItem 的 updateTitle 更新
                    } else {
                        // 显示错误提示
                        QMessageBox::warning(this, tr("重命名失败"), tr("无法更新会话标题，请检查数据库。"));
                    }
                });

        ui->listChatHistory->addItem(listItem);
        ui->listChatHistory->setItemWidget(listItem, customItem);
    }
}
