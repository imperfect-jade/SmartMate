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
#include <QButtonGroup>
#include <QComboBox>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

namespace smartmate::view::widgets {
namespace { constexpr auto taskMimeType = "application/x-smartmate-task-id"; }
using ListRole = viewmodel::TaskListContract::Role;

TaskListView::TaskListView(QWidget *parent) : QListView(parent)
{
    setObjectName(QStringLiteral("taskListView"));
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDragEnabled(true);
    setSpacing(2);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
}

void TaskListView::startDrag(Qt::DropActions)
{
    const QModelIndex index = currentIndex();
    if (!index.isValid() || !index.data(ListRole::CanStartRole).toBool()) return;
    auto *mime = new QMimeData;
    mime->setData(QString::fromLatin1(taskMimeType),
                  index.data(ListRole::TaskIdRole).toString().toUtf8());
    mime->setText(index.data(ListRole::TitleRole).toString());
    auto *drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->exec(Qt::MoveAction);
}

TaskFocusPanel::TaskFocusPanel(viewmodel::TaskFocusContract &focus,
                               viewmodel::TaskListContract &tasks,
                               QWidget *parent)
    : QFrame(parent), m_focus(focus), m_tasks(tasks)
    , m_title(new QLabel(this)), m_description(new QLabel(this))
    , m_meta(new QLabel(this)), m_details(new QPushButton(tr("查看详情"), this))
    , m_primary(new QPushButton(this))
{
    setObjectName(QStringLiteral("focusTaskSlot"));
    setFrameShape(QFrame::StyledPanel);
    setAcceptDrops(true);
    auto *layout = new QHBoxLayout(this);
    auto *text = new QVBoxLayout;
    m_title->setObjectName(QStringLiteral("sectionTitle"));
    m_description->setWordWrap(true);
    m_description->setObjectName(QStringLiteral("secondaryText"));
    m_meta->setObjectName(QStringLiteral("secondaryText"));
    text->addWidget(m_title); text->addWidget(m_description); text->addWidget(m_meta);
    layout->addLayout(text, 1);
    auto *actions = new QVBoxLayout;
    m_primary->setObjectName(QStringLiteral("focusPrimaryActionButton"));
    actions->addWidget(m_details); actions->addWidget(m_primary);
    layout->addLayout(actions);
    connect(&m_focus, &viewmodel::TaskFocusContract::focusTaskChanged,
            this, &TaskFocusPanel::synchronize);
    connect(m_details, &QPushButton::clicked, this, [this] {
        if (!m_focus.focusTaskId().isEmpty()) emit detailsRequested(m_focus.focusTaskId());
    });
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
    const auto state = m_focus.focusState();
    if (state == viewmodel::TaskFocusContract::FocusState::AllBlocked) {
        m_title->setText(tr("当前任务都被前置条件阻塞"));
        m_description->setText(tr("打开依赖图查看阻塞关系。"));
        m_primary->setText(tr("查看依赖图"));
    } else if (state == viewmodel::TaskFocusContract::FocusState::NoTasks) {
        m_title->setText(tr("还没有待办任务"));
        m_description->setText(tr("新建一项任务开始规划。"));
        m_primary->setText(tr("新建任务"));
    } else {
        m_title->setText(m_focus.focusTitle());
        m_description->setText(state == viewmodel::TaskFocusContract::FocusState::Suggested
            ? tr("推荐：%1").arg(m_focus.focusReasonText()) : m_focus.focusDescription());
        m_primary->setText(state == viewmodel::TaskFocusContract::FocusState::InProgress
            ? tr("完成任务") : tr("开始推荐任务"));
    }
    QStringList meta{m_focus.focusStatusText(), m_focus.focusPriorityText()};
    if (!m_focus.focusDeadlineText().isEmpty()) meta << tr("截止 %1").arg(m_focus.focusDeadlineText());
    if (m_focus.focusHasCategory()) meta << m_focus.focusCategoryName();
    m_meta->setText(meta.join(QStringLiteral(" · ")));
    m_details->setVisible(!m_focus.focusTaskId().isEmpty());
}

void TaskFocusPanel::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(QString::fromLatin1(taskMimeType))) event->acceptProposedAction();
}

void TaskFocusPanel::dropEvent(QDropEvent *event)
{
    const QString id = QString::fromUtf8(event->mimeData()->data(QString::fromLatin1(taskMimeType)));
    if (!id.isEmpty()) {
        event->acceptProposedAction();
        m_tasks.startTask(id);
    }
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
    , m_list(new TaskListView(this)), m_empty(new QLabel(this))
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
    m_empty->setAlignment(Qt::AlignCenter); root->addWidget(m_empty);
    root->addWidget(m_list, 1);

    m_list->setModel(&dependencies.taskList);
    auto *delegate = new TaskItemDelegate(dependencies.taskList, m_list);
    m_list->setItemDelegate(delegate);
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
    connect(m_bulkArchive, &QPushButton::clicked, this, [this] {
        if (confirm(tr("确认批量归档"), tr("确定归档选中的 %1 项任务吗？").arg(m_dependencies.taskList.bulkSelectedCount())))
            m_dependencies.taskList.archiveSelectedTasks();
    });
    connect(m_bulkDelete, &QPushButton::clicked, this, [this] {
        if (confirm(tr("确认批量永久删除"), tr("永久删除选中的 %1 项归档任务且同时清理关联依赖？").arg(m_dependencies.taskList.bulkSelectedCount())))
            m_dependencies.taskList.deleteSelectedArchivedTasks();
    });
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
    m_empty->setVisible(tasks.count() == 0);
    m_list->setVisible(tasks.count() > 0);
    m_empty->setText(tasks.hasActiveFilters() ? tr("没有符合当前搜索和筛选条件的任务")
        : tasks.showArchived() ? tr("还没有归档任务") : tr("还没有任务，从新建一项开始吧"));
    m_list->viewport()->update();
}

} // namespace smartmate::view::widgets
