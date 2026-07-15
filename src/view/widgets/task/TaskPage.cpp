#include "TaskPage.h"

#include "TaskCategoryDialog.h"
#include "TaskDependencyDialog.h"
#include "TaskDetailsDialog.h"
#include "TaskEditorDialog.h"
#include "TaskItemDelegate.h"
#include "viewmodel/contracts/TaskCategoryContract.h"
#include "viewmodel/contracts/TaskDependencyContract.h"
#include "viewmodel/contracts/TaskDetailsContract.h"
#include "viewmodel/contracts/TaskEditorContract.h"
#include "viewmodel/contracts/TaskFocusContract.h"
#include "viewmodel/contracts/TaskListContract.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace smartmate::view::widgets {

using ListRole = viewmodel::TaskListContract::Role;

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
