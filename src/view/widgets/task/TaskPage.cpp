#include "TaskPage.h"

#include "TaskEditorDialog.h"
#include "TaskDetailsDialog.h"
#include "TaskItemDelegate.h"
#include "TaskCategoryDialog.h"
#include "TaskDependencyDialog.h"
#include "viewmodel/contracts/TaskCategoryContract.h"
#include "viewmodel/contracts/TaskDependencyContract.h"
#include "viewmodel/contracts/TaskDetailsContract.h"
#include "viewmodel/contracts/TaskEditorContract.h"
#include "viewmodel/contracts/TaskFocusContract.h"
#include "viewmodel/contracts/TaskListContract.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace smartmate::view::widgets {
namespace { constexpr auto taskMimeType = "application/x-smartmate-task-id"; }
using ListRole = viewmodel::TaskListContract::Role;

TaskListView::TaskListView(QWidget *parent) : QListView(parent)
{
    setObjectName(QStringLiteral("taskListView"));
    setFrameShape(QFrame::NoFrame);
    setAutoFillBackground(false);
    viewport()->setAutoFillBackground(false);
    viewport()->setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet(QStringLiteral(
        "QListView#taskListView { background: transparent; border: none; outline: none; }"));
    setSelectionMode(QAbstractItemView::SingleSelection);
    // 原生 item drag 依赖模型 flags，且会把整张卡片变为拖拽源；这里由 View
    // 显式识别专用拖拽柄，资格仍只读取 Contract 的 CanStartRole。
    setDragEnabled(false);
    setMouseTracking(true);
    setSpacing(2);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
}

QColor TaskListView::cardSurfaceColor() const
{
    // QListView 的透明 QSS 会在部分平台把自身 Base role 改写为黑色；窗口根节点
    // 仍持有 WidgetTheme 注入的真实表面色，因此卡片与拖拽预览统一从这里读取。
    const QWidget *themeRoot = window();
    QColor surface = themeRoot && themeRoot != this
        ? themeRoot->palette().color(QPalette::Base)
        : QApplication::palette().color(QPalette::Base);
    if (!surface.isValid() || surface.alpha() == 0) {
        surface = QColor(QStringLiteral("#ffffff"));
    }
    return surface;
}

void TaskListView::startDrag(Qt::DropActions)
{
    // MIME 只携带稳定 TaskId 和展示标题；拖放最终仍调用 Contract 命令并由 Model 复核。
    const QModelIndex index = m_dragCandidate.isValid()
        ? QModelIndex(m_dragCandidate) : currentIndex();
    if (!index.isValid() || !index.data(ListRole::CanStartRole).toBool()) return;
    auto *mime = new QMimeData;
    mime->setData(QString::fromLatin1(taskMimeType),
                  index.data(ListRole::TaskIdRole).toString().toUtf8());
    mime->setText(index.data(ListRole::TitleRole).toString());
    auto *drag = new QDrag(this);
    drag->setMimeData(mime);
    QPixmap preview(280, 66);
    preview.fill(Qt::transparent);
    QPainter painter(&preview);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(palette().highlight().color(), 2));
    painter.setBrush(cardSurfaceColor());
    painter.drawRoundedRect(preview.rect().adjusted(1, 1, -2, -2), 11, 11);
    QFont handleFont = font();
    handleFont.setPointSizeF(handleFont.pointSizeF() + 4);
    painter.setFont(handleFont);
    painter.setPen(palette().highlight().color());
    painter.drawText(QRect{10, 0, 34, preview.height()}, Qt::AlignCenter,
                     QStringLiteral("⠿"));
    QFont titleFont = font();
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(palette().text().color());
    painter.drawText(QRect{50, 9, 218, 24}, Qt::AlignLeft | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(
                         index.data(ListRole::TitleRole).toString(),
                         Qt::ElideRight, 218));
    painter.setFont(font());
    painter.setPen(palette().placeholderText().color());
    painter.drawText(QRect{50, 34, 218, 22}, Qt::AlignLeft | Qt::AlignVCenter,
                     tr("%1 · 拖到“现在做”开始")
                         .arg(index.data(ListRole::StatusTextRole).toString()));
    painter.end();
    drag->setPixmap(preview);
    drag->setHotSpot({22, preview.height() / 2});
    emit taskDragStarted(index.data(ListRole::TaskIdRole).toString());
    drag->exec(Qt::MoveAction);
    drag->deleteLater();
}

void TaskListView::mousePressEvent(QMouseEvent *event)
{
    const QModelIndex index = indexAt(event->position().toPoint());
    const auto *delegate = qobject_cast<TaskItemDelegate *>(itemDelegate());
    if (event->button() == Qt::LeftButton && index.isValid() && delegate
        && delegate->dragHandleRect(visualRect(index), index)
               .contains(event->position().toPoint())) {
        m_dragCandidate = index;
        m_dragStartPosition = event->position().toPoint();
        setCurrentIndex(index);
        viewport()->setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    clearDragCandidate();
    QListView::mousePressEvent(event);
}

void TaskListView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragCandidate.isValid()) {
        if (!(event->buttons() & Qt::LeftButton)) {
            clearDragCandidate();
            return;
        }
        if ((event->position().toPoint() - m_dragStartPosition).manhattanLength()
            >= QApplication::startDragDistance()) {
            event->accept();
            startDrag(Qt::MoveAction);
            clearDragCandidate();
        }
        return;
    }
    QListView::mouseMoveEvent(event);
}

void TaskListView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_dragCandidate.isValid() && event->button() == Qt::LeftButton) {
        clearDragCandidate();
        event->accept();
        return;
    }
    QListView::mouseReleaseEvent(event);
}

void TaskListView::clearDragCandidate()
{
    m_dragCandidate = QPersistentModelIndex{};
    viewport()->unsetCursor();
}

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
        && event->mimeData()->hasFormat(QString::fromLatin1(taskMimeType))
        && !event->mimeData()->data(QString::fromLatin1(taskMimeType)).isEmpty();
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
    const QString id = QString::fromUtf8(event->mimeData()->data(QString::fromLatin1(taskMimeType)));
    const bool canAccept = m_focus.focusState()
            != viewmodel::TaskFocusContract::FocusState::InProgress
        && event->mimeData()->hasFormat(QString::fromLatin1(taskMimeType))
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

TaskPage::TaskPage(TaskPageDependencies dependencies, QWidget *parent)
    : QWidget(parent), m_dependencies(dependencies)
    , m_focus(new TaskFocusPanel(dependencies.taskFocus, dependencies.taskList, this))
    , m_search(new QLineEdit(this)), m_priority(new QComboBox(this))
    , m_category(new QComboBox(this))
    , m_active(new QToolButton(this)), m_archived(new QToolButton(this))
    , m_bulk(new QPushButton(tr("批量管理"), this)), m_newTask(new QPushButton(tr("新建任务"), this))
    , m_manageCategories(new QPushButton(tr("管理类别"), this))
    , m_clearFilters(new QPushButton(tr("清除条件"), this))
    , m_bulkBar(new QWidget(this)), m_bulkCount(new QLabel(m_bulkBar))
    , m_selectAll(new QPushButton(tr("全选可操作项"), m_bulkBar))
    , m_bulkArchive(new QPushButton(tr("批量归档"), m_bulkBar))
    , m_bulkRestore(new QPushButton(tr("批量恢复"), m_bulkBar))
    , m_bulkDelete(new QPushButton(tr("批量永久删除"), m_bulkBar))
    , m_content(new QStackedWidget(this)), m_empty(new QLabel(m_content))
    , m_list(new TaskListView(m_content))
    , m_details(new TaskDetailsDialog(dependencies.taskDetails, this))
    , m_editor(new TaskEditorDialog(dependencies.taskEditor, this))
    , m_categories(new TaskCategoryDialog(dependencies.taskCategories, this))
    , m_editorCategories(new TaskCategoryDialog(dependencies.taskCategories, m_editor))
    , m_dependencyEditor(new TaskDependencyDialog(dependencies.taskDependencies, this))
{
    // 编辑器为窗口模态；使用同一 Contract 的子对话框，避免同级类别窗口被模态层阻挡。
    m_editorCategories->setObjectName(QStringLiteral("taskEditorCategoryDialog"));
    setObjectName(QStringLiteral("taskPage"));
    auto *root = new QVBoxLayout(this); root->setContentsMargins(24, 22, 24, 22);
    auto *heading = new QLabel(tr("任务"), this); heading->setObjectName(QStringLiteral("pageTitle"));
    root->addWidget(heading); root->addWidget(m_focus);
    auto *filters = new QHBoxLayout;
    auto *scope = new QButtonGroup(this); scope->setExclusive(true);
    m_active->setObjectName(QStringLiteral("activeTasksButton"));
    m_archived->setObjectName(QStringLiteral("archivedTasksButton"));
    m_active->setText(tr("活动")); m_active->setCheckable(true); m_active->setChecked(true);
    m_archived->setText(tr("归档")); m_archived->setCheckable(true);
    scope->addButton(m_active, 0); scope->addButton(m_archived, 1);
    m_search->setObjectName(QStringLiteral("taskSearchField"));
    m_search->setPlaceholderText(tr("搜索标题或描述"));
    m_priority->setObjectName(QStringLiteral("priorityFilterComboBox"));
    m_category->setObjectName(QStringLiteral("categoryFilterComboBox"));
    m_bulk->setObjectName(QStringLiteral("bulkManagementButton"));
    m_newTask->setObjectName(QStringLiteral("newTaskButton"));
    m_manageCategories->setObjectName(QStringLiteral("manageCategoriesButton"));
    m_clearFilters->setObjectName(QStringLiteral("clearFiltersButton"));
    filters->addWidget(m_active); filters->addWidget(m_archived);
    filters->addWidget(m_search, 1); filters->addWidget(m_priority);
    filters->addWidget(m_category);
    filters->addWidget(m_manageCategories);
    filters->addWidget(m_clearFilters);
    filters->addWidget(m_bulk); filters->addWidget(m_newTask);
    root->addLayout(filters);
    auto *bulkLayout = new QHBoxLayout(m_bulkBar); bulkLayout->setContentsMargins(0, 0, 0, 0);
    m_bulkCount->setObjectName(QStringLiteral("bulkSelectedCountLabel"));
    m_selectAll->setObjectName(QStringLiteral("selectAllVisibleButton"));
    m_bulkArchive->setObjectName(QStringLiteral("bulkArchiveButton"));
    m_bulkRestore->setObjectName(QStringLiteral("bulkRestoreButton"));
    m_bulkDelete->setObjectName(QStringLiteral("bulkDeleteButton"));
    bulkLayout->addWidget(m_bulkCount); bulkLayout->addStretch();
    bulkLayout->addWidget(m_selectAll); bulkLayout->addWidget(m_bulkArchive);
    bulkLayout->addWidget(m_bulkRestore); bulkLayout->addWidget(m_bulkDelete);
    auto *clear = new QPushButton(tr("清空"), m_bulkBar);
    auto *exit = new QPushButton(tr("退出批量"), m_bulkBar);
    bulkLayout->addWidget(clear); bulkLayout->addWidget(exit);
    root->addWidget(m_bulkBar);
    m_empty->setObjectName(QStringLiteral("taskEmptyStateLabel"));
    m_empty->setAlignment(Qt::AlignCenter);
    m_content->setObjectName(QStringLiteral("taskContentStack"));
    m_content->setFrameShape(QFrame::NoFrame);
    m_content->setAutoFillBackground(false);
    m_content->setAttribute(Qt::WA_TranslucentBackground);
    m_content->setStyleSheet(QStringLiteral(
        "QStackedWidget#taskContentStack { background: transparent; border: none; }"));
    m_content->addWidget(m_empty);
    m_content->addWidget(m_list);
    root->addWidget(m_content, 1);

    // TaskListContract 直接作为 Qt Model；Delegate 通过稳定 Role 绘制和转发命令。
    m_list->setModel(&dependencies.taskList);
    auto *delegate = new TaskItemDelegate(dependencies.taskList, m_list);
    m_list->setItemDelegate(delegate);
    // Widget→Contract：只使用用户事件 textEdited/activated/idClicked 写入筛选会话。
    connect(scope, &QButtonGroup::idClicked, this, [this](int id) { m_dependencies.taskList.setShowArchived(id == 1); });
    connect(m_search, &QLineEdit::textEdited, &dependencies.taskList, &viewmodel::TaskListContract::setSearchText);
    connect(m_priority, &QComboBox::activated, &dependencies.taskList, &viewmodel::TaskListContract::setPriorityFilterIndex);
    connect(m_category, &QComboBox::activated, this, [this](const int index) {
        m_dependencies.taskList.setCategoryFilter(
            m_category->itemData(index, Qt::UserRole).toInt(),
            m_category->itemData(index, Qt::UserRole + 1).toString());
    });
    connect(m_manageCategories, &QPushButton::clicked, m_categories,
            &TaskCategoryDialog::openManager);
    connect(m_clearFilters, &QPushButton::clicked, &dependencies.taskList,
            &viewmodel::TaskListContract::clearFilters);
    connect(m_bulk, &QPushButton::clicked, &dependencies.taskList, &viewmodel::TaskListContract::beginBulkSelection);
    connect(m_newTask, &QPushButton::clicked, &dependencies.taskEditor, &viewmodel::TaskEditorContract::beginCreate);
    connect(m_selectAll, &QPushButton::clicked, &dependencies.taskList, &viewmodel::TaskListContract::toggleSelectAllVisible);
    connect(clear, &QPushButton::clicked, &dependencies.taskList, &viewmodel::TaskListContract::clearBulkSelection);
    connect(exit, &QPushButton::clicked, &dependencies.taskList, &viewmodel::TaskListContract::cancelBulkSelection);
    connect(m_bulkRestore, &QPushButton::clicked, &dependencies.taskList, &viewmodel::TaskListContract::restoreSelectedTasks);
    // 确认属于 View；确认后整批只提交一次命令，原子性由 Model/Repository 保证。
    connect(m_bulkArchive, &QPushButton::clicked, this, [this] {
        if (confirm(tr("确认批量归档"), tr("确定归档选中的 %1 项任务吗？").arg(m_dependencies.taskList.bulkSelectedCount())))
            m_dependencies.taskList.archiveSelectedTasks();
    });
    connect(m_bulkDelete, &QPushButton::clicked, this, [this] {
        if (confirm(tr("确认批量永久删除"), tr("永久删除选中的 %1 项归档任务且同时清理关联依赖？").arg(m_dependencies.taskList.bulkSelectedCount())))
            m_dependencies.taskList.deleteSelectedArchivedTasks();
    });
    // Delegate 只转发稳定 ID 或页面动作；页面负责打开对应的窄 Contract 对话框。
    connect(delegate, &TaskItemDelegate::detailsRequested, m_details, &TaskDetailsDialog::openTask);
    connect(delegate, &TaskItemDelegate::editRequested, this, &TaskPage::openEditor);
    connect(delegate, &TaskItemDelegate::editDependenciesRequested,
            m_dependencyEditor, &TaskDependencyDialog::openTask);
    connect(delegate, &TaskItemDelegate::cancelRequested, this, [this](const QString &id, const QString &title) {
        if (confirm(tr("确认取消任务"), tr("确定取消“%1”吗？").arg(title))) m_dependencies.taskList.cancelTask(id);
    });
    connect(delegate, &TaskItemDelegate::archiveRequested, this, [this](const QString &id, const QString &title) {
        if (confirm(tr("确认归档"), tr("确定归档“%1”吗？").arg(title))) m_dependencies.taskList.archiveTask(id);
    });
    connect(delegate, &TaskItemDelegate::deleteRequested, this, [this](const QString &id, const QString &title) {
        if (confirm(tr("确认永久删除"), tr("永久删除“%1”且同时清理全部关联依赖？").arg(title))) m_dependencies.taskList.deleteArchivedTask(id);
    });
    connect(m_list, &QListView::activated, this, [this](const QModelIndex &index) {
        m_details->openTask(index.data(ListRole::TaskIdRole).toString());
    });
    connect(m_details, &TaskDetailsDialog::editRequested, this, &TaskPage::openEditor);
    connect(m_details, &TaskDetailsDialog::editDependenciesRequested,
            m_dependencyEditor, &TaskDependencyDialog::openTask);
    connect(m_editor, &TaskEditorDialog::manageCategoriesRequested,
            m_editorCategories, &TaskCategoryDialog::openManager);
    connect(m_focus, &TaskFocusPanel::detailsRequested, m_details, &TaskDetailsDialog::openTask);
    connect(m_focus, &TaskFocusPanel::createRequested, &dependencies.taskEditor, &viewmodel::TaskEditorContract::beginCreate);
    connect(m_focus, &TaskFocusPanel::dependencyGraphRequested, this, &TaskPage::showDependencyGraphRequested);
    // Contract 属性通知与 Qt Model 结构通知统一驱动控件同步；初始化后立即读取一次 getter。
    connect(&dependencies.taskList, &viewmodel::TaskListContract::showArchivedChanged, this, &TaskPage::updateControls);
    connect(&dependencies.taskList, &viewmodel::TaskListContract::searchTextChanged, this, &TaskPage::updateControls);
    connect(&dependencies.taskList, &viewmodel::TaskListContract::priorityFilterIndexChanged, this, &TaskPage::updateControls);
    connect(&dependencies.taskList, &viewmodel::TaskListContract::categoryOptionsChanged,
            this, &TaskPage::updateControls);
    connect(&dependencies.taskList, &viewmodel::TaskListContract::categoryFilterChanged,
            this, &TaskPage::updateControls);
    connect(&dependencies.taskList, &viewmodel::TaskListContract::hasActiveFiltersChanged,
            this, &TaskPage::updateControls);
    connect(&dependencies.taskList, &viewmodel::TaskListContract::bulkSelectionChanged, this, &TaskPage::updateControls);
    connect(&dependencies.taskList, &viewmodel::TaskListContract::countChanged, this, &TaskPage::updateControls);
    connect(&dependencies.taskList, &QAbstractItemModel::modelReset, this, &TaskPage::updateControls);
    updateControls();
}

bool TaskPage::confirm(const QString &title, const QString &message)
{
    return QMessageBox::question(this, title, message,
        QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Ok;
}

void TaskPage::openEditor(const QString &taskId)
{
    m_dependencies.taskEditor.beginEdit(taskId);
}

void TaskPage::updateControls()
{
    const auto &tasks = m_dependencies.taskList;
    // 回填筛选控件时阻断其信号，防止 Contract→Widget 更新被误发为用户命令。
    const QSignalBlocker searchBlocker(m_search), priorityBlocker(m_priority),
                         categoryBlocker(m_category),
                         activeBlocker(m_active), archivedBlocker(m_archived);
    m_search->setText(tasks.searchText());
    if (m_priority->count() != tasks.priorityFilterOptions().size()) {
        m_priority->clear(); m_priority->addItems(tasks.priorityFilterOptions());
    }
    m_priority->setCurrentIndex(tasks.priorityFilterIndex());
    m_category->clear();
    int currentCategoryIndex = 0;
    const QVariantList categoryOptions = tasks.categoryFilterOptions();
    for (int i = 0; i < categoryOptions.size(); ++i) {
        const QVariantMap option = categoryOptions.at(i).toMap();
        const int mode = option.value(QStringLiteral("mode")).toInt();
        const QString categoryId = option.value(QStringLiteral("categoryId")).toString();
        m_category->addItem(option.value(QStringLiteral("name")).toString());
        m_category->setItemData(i, mode, Qt::UserRole);
        m_category->setItemData(i, categoryId, Qt::UserRole + 1);
        if (mode == tasks.categoryFilterMode()
            && (mode != 2 || categoryId == tasks.categoryFilterCategoryId())) {
            currentCategoryIndex = i;
        }
    }
    m_category->setCurrentIndex(currentCategoryIndex);
    m_clearFilters->setVisible(tasks.hasActiveFilters());
    m_active->setChecked(!tasks.showArchived()); m_archived->setChecked(tasks.showArchived());
    m_bulkBar->setVisible(tasks.bulkSelectionMode());
    m_focus->setVisible(!tasks.bulkSelectionMode());
    m_bulk->setVisible(!tasks.bulkSelectionMode()); m_newTask->setVisible(!tasks.bulkSelectionMode());
    m_bulkCount->setText(tr("已选择 %1 项").arg(tasks.bulkSelectedCount()));
    m_selectAll->setEnabled(tasks.bulkSelectableVisibleCount() > 0);
    m_bulkArchive->setVisible(!tasks.showArchived()); m_bulkArchive->setEnabled(tasks.canBulkArchive());
    m_bulkRestore->setVisible(tasks.showArchived()); m_bulkRestore->setEnabled(tasks.canBulkRestore());
    m_bulkDelete->setVisible(tasks.showArchived()); m_bulkDelete->setEnabled(tasks.canBulkDelete());
    m_content->setCurrentWidget(tasks.count() == 0
        ? static_cast<QWidget *>(m_empty) : static_cast<QWidget *>(m_list));
    m_empty->setText(tasks.hasActiveFilters() ? tr("没有符合当前搜索和筛选条件的任务")
        : tasks.showArchived() ? tr("还没有归档任务") : tr("还没有任务，从新建一项开始吧"));
    m_list->viewport()->update();
}

} // namespace smartmate::view::widgets
