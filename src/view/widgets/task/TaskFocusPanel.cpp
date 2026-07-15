#include "TaskFocusPanel.h"

#include "TaskDragMime.h"
#include "viewmodel/contracts/TaskFocusContract.h"
#include "viewmodel/contracts/TaskListContract.h"

#include <QColor>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

namespace smartmate::view::widgets {

TaskFocusPanel::TaskFocusPanel(viewmodel::TaskFocusContract &focus,
                               viewmodel::TaskListContract &tasks,
                               QWidget *parent)
    : QFrame(parent), m_focus(focus), m_tasks(tasks)
    , m_iconFrame(new QFrame(this)), m_icon(new QLabel(m_iconFrame))
    , m_eyebrow(new QLabel(this)), m_title(new QLabel(this))
    , m_description(new QLabel(this)), m_meta(new QLabel(this))
    , m_categoryBadge(new QLabel(this)), m_overdueBadge(new QLabel(this))
    , m_overdueReminder(new QLabel(this))
    , m_details(new QPushButton(tr("查看详情"), this))
    , m_primary(new QPushButton(this))
{
    setObjectName(QStringLiteral("focusTaskSlot"));
    setFrameShape(QFrame::StyledPanel);
    setMinimumHeight(158);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    setAcceptDrops(true);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(16);

    m_iconFrame->setObjectName(QStringLiteral("focusStateIcon"));
    m_iconFrame->setFixedSize(48, 48);
    auto *iconLayout = new QVBoxLayout(m_iconFrame);
    iconLayout->setContentsMargins(0, 0, 0, 0);
    m_icon->setObjectName(QStringLiteral("focusStateIconText"));
    m_icon->setAlignment(Qt::AlignCenter);
    iconLayout->addWidget(m_icon);
    layout->addWidget(m_iconFrame, 0, Qt::AlignTop);

    auto *text = new QVBoxLayout;
    text->setSpacing(5);
    auto *heading = new QHBoxLayout;
    heading->setSpacing(7);
    m_eyebrow->setObjectName(QStringLiteral("focusEyebrow"));
    m_categoryBadge->setObjectName(QStringLiteral("focusCategoryBadge"));
    m_overdueBadge->setObjectName(QStringLiteral("focusOverdueBadge"));
    m_overdueBadge->setText(tr("已逾期"));
    heading->addWidget(m_eyebrow);
    heading->addWidget(m_categoryBadge);
    heading->addWidget(m_overdueBadge);
    heading->addStretch();
    m_title->setObjectName(QStringLiteral("focusTaskTitle"));
    m_description->setWordWrap(true);
    m_description->setObjectName(QStringLiteral("focusTaskDescription"));
    m_meta->setObjectName(QStringLiteral("focusTaskMeta"));
    m_overdueReminder->setObjectName(QStringLiteral("focusOverdueReminder"));
    m_overdueReminder->setText(tr("请尽快处理，避免计划继续延误。"));
    text->addLayout(heading);
    text->addWidget(m_title);
    text->addWidget(m_description);
    text->addWidget(m_meta);
    text->addWidget(m_overdueReminder);
    layout->addLayout(text, 1);
    auto *actions = new QVBoxLayout;
    actions->setSpacing(8);
    actions->addStretch();
    m_details->setObjectName(QStringLiteral("focusDetailsButton"));
    m_primary->setObjectName(QStringLiteral("focusPrimaryActionButton"));
    actions->addWidget(m_details); actions->addWidget(m_primary);
    actions->addStretch();
    layout->addLayout(actions);
    // 先监听聚合焦点通知，再同步当前 getter，覆盖初始快照和后续变化。
    connect(&m_focus, &viewmodel::TaskFocusContract::focusTaskChanged,
            this, &TaskFocusPanel::synchronize);
    connect(m_details, &QPushButton::clicked, this, [this] {
        if (!m_focus.focusTaskId().isEmpty()) emit detailsRequested(m_focus.focusTaskId());
    });
    // 同一个主按钮根据 Contract 投影发出语义命令或页面导航，不自行计算资格。
    connect(m_primary, &QPushButton::clicked, this, [this] {
        switch (m_focus.focusState()) {
        case viewmodel::TaskFocusContract::FocusState::InProgress:
            m_tasks.completeTask(m_focus.focusTaskId()); break;
        case viewmodel::TaskFocusContract::FocusState::Suggested:
            m_tasks.startTask(m_focus.focusTaskId()); break;
        case viewmodel::TaskFocusContract::FocusState::AllBlocked:
            emit dependencyGraphRequested(); break;
        case viewmodel::TaskFocusContract::FocusState::NoTasks:
            emit createRequested(); break;
        }
    });
    synchronize();
}

void TaskFocusPanel::synchronize()
{
    // 面板只解释 Focus Contract 的聚合展示状态；拖拽覆盖层是纯 View 临时状态。
    const auto state = m_focus.focusState();
    if (m_dragActive) {
        m_icon->setText(QStringLiteral("↓"));
        m_eyebrow->setText(tr("现在做 · 拖放开始"));
        m_title->setText(tr("释放以开始任务"));
        m_description->setText(tr("任务资格和依赖约束将由任务服务最终校验。"));
        m_meta->clear();
        m_categoryBadge->hide();
        m_overdueBadge->hide();
        m_overdueReminder->hide();
        m_details->hide();
        m_primary->hide();
        applyPresentationStyle();
        return;
    }
    m_primary->show();
    if (state == viewmodel::TaskFocusContract::FocusState::AllBlocked) {
        m_icon->setText(QStringLiteral("!"));
        m_eyebrow->setText(tr("现在做 · 等待解锁"));
        m_title->setText(tr("当前任务都被前置条件阻塞"));
        m_description->setText(tr("打开依赖图查看阻塞关系。"));
        m_primary->setText(tr("查看依赖图"));
    } else if (state == viewmodel::TaskFocusContract::FocusState::NoTasks) {
        m_icon->setText(QStringLiteral("+"));
        m_eyebrow->setText(tr("现在做"));
        m_title->setText(tr("还没有待办任务"));
        m_description->setText(tr("新建一项任务开始规划。"));
        m_primary->setText(tr("新建任务"));
    } else {
        const bool inProgress = state == viewmodel::TaskFocusContract::FocusState::InProgress;
        m_icon->setText(QStringLiteral("▶"));
        m_eyebrow->setText(inProgress ? tr("现在做 · 正在进行") : tr("现在做 · 推荐任务"));
        m_title->setText(m_focus.focusTitle());
        m_description->setText(inProgress
            ? m_focus.focusDescription()
            : tr("推荐：%1 · 也可拖入任意可执行任务").arg(m_focus.focusReasonText()));
        m_primary->setText(inProgress
            ? tr("完成任务") : tr("开始推荐任务"));
    }
    QStringList meta{m_focus.focusStatusText(), m_focus.focusPriorityText()};
    if (!m_focus.focusDeadlineText().isEmpty()) meta << tr("截止 %1").arg(m_focus.focusDeadlineText());
    m_meta->setText(meta.join(QStringLiteral(" · ")));
    const bool hasTask = !m_focus.focusTaskId().isEmpty();
    m_meta->setVisible(hasTask);
    m_categoryBadge->setText(m_focus.focusCategoryName());
    m_categoryBadge->setVisible(hasTask && m_focus.focusHasCategory());
    m_overdueBadge->setVisible(hasTask && m_focus.focusOverdue());
    m_overdueReminder->setVisible(hasTask && m_focus.focusOverdue());
    m_details->setVisible(hasTask);
    applyPresentationStyle();
}

void TaskFocusPanel::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::FontChange) {
        applyPresentationStyle();
    }
}

void TaskFocusPanel::applyPresentationStyle()
{
    // setStyleSheet 会触发 PaletteChange；用重入保护避免主题刷新形成递归事件链。
    if (m_applyingStyle) return;
    m_applyingStyle = true;
    const QColor primary = palette().color(QPalette::Highlight);
    const QColor background = palette().color(QPalette::Base);
    const QColor border = m_dragActive ? primary : palette().color(QPalette::Midlight);
    const bool emphasized = m_dragActive
        || m_focus.focusState() == viewmodel::TaskFocusContract::FocusState::InProgress;
    const QColor panelBackground = emphasized
        ? QColor(primary.red(), primary.green(), primary.blue(), 25) : background;
    setStyleSheet(QStringLiteral(
        "QFrame#focusTaskSlot { background: rgba(%1,%2,%3,%4); border: %5px solid %6; "
        "border-radius: 14px; }"
        "QFrame#focusStateIcon { background: %7; border: none; border-radius: 14px; }"
        "QLabel#focusStateIconText { color: white; border: none; background: transparent; "
        "font-size: 19px; font-weight: 700; }"
        "QLabel#focusCategoryBadge { color: %8; background: %9; border: 1px solid %8; "
        "border-radius: 8px; padding: 2px 7px; font-size: 11px; font-weight: 600; }"
        "QLabel#focusOverdueBadge { color: %10; background: transparent; border: 1px solid %10; "
        "border-radius: 8px; padding: 2px 7px; font-size: 11px; font-weight: 700; }")
        .arg(panelBackground.red()).arg(panelBackground.green())
        .arg(panelBackground.blue()).arg(panelBackground.alpha())
        .arg(m_dragActive ? 2 : 1).arg(border.name())
        .arg(primary.name())
        .arg([this] {
            const QColor accent(m_focus.focusCategoryAccent());
            return accent.isValid() ? accent.name() : palette().color(QPalette::Highlight).name();
        }())
        .arg([this] {
            QColor accent(m_focus.focusCategoryAccent());
            if (!accent.isValid()) accent = palette().color(QPalette::Highlight);
            return QStringLiteral("rgba(%1,%2,%3,28)")
                .arg(accent.red()).arg(accent.green()).arg(accent.blue());
        }())
        .arg(palette().color(QPalette::BrightText).name()));
    m_applyingStyle = false;
}

void TaskFocusPanel::dragEnterEvent(QDragEnterEvent *event)
{
    const bool canAccept = m_focus.focusState()
            != viewmodel::TaskFocusContract::FocusState::InProgress
        && event->mimeData()->hasFormat(QString::fromLatin1(task_drag_detail::mimeType))
        && !event->mimeData()->data(QString::fromLatin1(task_drag_detail::mimeType)).isEmpty();
    if (!canAccept) {
        event->ignore();
        return;
    }
    setDragActive(true);
    event->acceptProposedAction();
}

void TaskFocusPanel::dragLeaveEvent(QDragLeaveEvent *event)
{
    setDragActive(false);
    event->accept();
}

void TaskFocusPanel::dropEvent(QDropEvent *event)
{
    const QString id = QString::fromUtf8(event->mimeData()->data(QString::fromLatin1(task_drag_detail::mimeType)));
    const bool canAccept = m_focus.focusState()
            != viewmodel::TaskFocusContract::FocusState::InProgress
        && event->mimeData()->hasFormat(QString::fromLatin1(task_drag_detail::mimeType))
        && !id.isEmpty();
    setDragActive(false);
    if (canAccept) {
        event->acceptProposedAction();
        // View 的接收判断只控制手势反馈，Service 仍最终复核状态、依赖和单进行中约束。
        m_tasks.startTask(id);
    } else {
        event->ignore();
    }
}

void TaskFocusPanel::setDragActive(const bool active)
{
    if (m_dragActive == active) return;
    m_dragActive = active;
    synchronize();
    update();
}

} // namespace smartmate::view::widgets

