#include "mainwidget.h"
#include "ui_mainwidget.h"

// 侧边栏：历史记录管理
void MainWidget::on_btnNewChat_clicked()
{
    ui->listWidgetChat->clear();
    int newId = Datamanager::getInstance().createSession(tr("新对话 "));
    if (newId != -1) {
        m_currentSessionId = newId;
        refreshSidebar();
        ui->labelStatus->setText(tr("🟢 已开启新会话"));
    }
}
//点击存档
void MainWidget::on_listChatHistory_itemClicked(QListWidgetItem *item)
{
    int sId = item->data(Qt::UserRole).toInt();
    m_currentSessionId = sId;
    ui->listWidgetChat->clear();

    QList<MessageInfo> history = Datamanager::getInstance().getMessagesBySession(sId);
    for (const auto& msg : history) {
        // 由于我们在收发时已经标准化存了 HTML，这里直接扔给气泡就行，完美保留换行！
        appendChatMessage(msg.content, msg.role == "user");
    }

    // 如果我们切回了【正在生成】的存档，立刻复活那个气泡！
    if (m_currentSessionId == m_generatingSessionId) {
        appendChatMessage("", false); // 创建一个空壳并捏住指针
        if (m_currentAiBubble) {
            m_currentAiBubble->updateContent(m_finalHtmlBuffer); // 把后台积攒的进度灌进去
        }
    }

    ui->labelStatus->setText(tr("🟢 已加载历史记录"));
    ui->listWidgetChat->scrollToBottom();
}


//收缩机制
void MainWidget::on_btnToggleSidebar_clicked()
{
    // 防抖：防止连续点击
    ui->btnToggleSidebar->setEnabled(false);

    // 动态获取当前的真实宽度作为起点，而不是写死 250
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

    if (m_isSidebarExpanded) {
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

        m_isSidebarExpanded = false;
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

        m_isSidebarExpanded = true;
    }

    // 动画结束后的善后工作
    connect(animGroup, &QParallelAnimationGroup::finished, this, [=]() {
        ui->btnToggleSidebar->setEnabled(true);
    });

    // 启动动画组，结束后自动销毁
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
            if (m_currentSessionId == sId) {
                m_currentSessionId = -1;
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
