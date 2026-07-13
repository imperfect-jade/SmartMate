#include "TaskDependencyViewModel.h"

#include "TaskErrorMapper.h"
#include "TaskPresentationFormatter.h"
#include "TaskCategoryPresentation.h"
#include "services/TaskCategoryService.h"
#include "services/TaskService.h"

#include <QUuid>

#include <algorithm>

namespace smartmate::viewmodel {

TaskDependencyViewModel::TaskDependencyViewModel(model::TaskService &taskService,
                                                 QObject *parent)
    : TaskDependencyViewModel(taskService, nullptr, parent)
{
}

TaskDependencyViewModel::TaskDependencyViewModel(
    model::TaskService &taskService,
    model::TaskCategoryService &categoryService,
    QObject *parent)
    : TaskDependencyViewModel(taskService, &categoryService, parent)
{
}

TaskDependencyViewModel::TaskDependencyViewModel(
    model::TaskService &taskService,
    model::TaskCategoryService *categoryService,
    QObject *parent)
    : QAbstractListModel(parent)
    , m_taskService(taskService)
    , m_categoryService(categoryService)
{
    if (m_categoryService) {
        connect(m_categoryService, &model::TaskCategoryService::categoriesChanged,
                this, &TaskDependencyViewModel::reloadCategories);
    }
    reloadCategories();
}

int TaskDependencyViewModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_candidates.size();
}

QVariant TaskDependencyViewModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_candidates.size()) {
        return {};
    }

    const model::Task &task = m_candidates.at(index.row());
    const bool selected = m_selectedPredecessors.contains(task.id());
    switch (role) {
    case TaskIdRole:
        return task.id().toString(QUuid::WithoutBraces);
    case ShortIdRole:
        return task.id().toString(QUuid::WithoutBraces).left(8);
    case TitleRole:
        return task.title();
    case StatusTextRole:
        return statusText(task.status());
    case PriorityTextRole:
        return priorityText(task.priority());
    case SelectedRole:
        return selected;
    case ArchivedRole:
        return task.status() == model::TaskStatus::Archived;
    case SelectableRole:
        return m_selectablePredecessors.contains(task.id());
    case CategoryNameRole: {
        const auto *category = categoryForTask(task);
        return category ? category->name : QString{};
    }
    case CategoryAccentRole: {
        const auto *category = categoryForTask(task);
        return category ? taskCategoryAccent(category->color) : QStringLiteral("#94a3b8");
    }
    case HasCategoryRole:
        return categoryForTask(task) != nullptr;
    default:
        return {};
    }
}

QHash<int, QByteArray> TaskDependencyViewModel::roleNames() const
{
    return {
        {TaskIdRole, "taskId"},
        {ShortIdRole, "shortId"},
        {TitleRole, "title"},
        {StatusTextRole, "statusText"},
        {PriorityTextRole, "priorityText"},
        {SelectedRole, "selected"},
        {ArchivedRole, "archived"},
        {SelectableRole, "selectable"},
        {CategoryNameRole, "categoryName"},
        {CategoryAccentRole, "categoryAccent"},
        {HasCategoryRole, "hasCategory"},
    };
}

QString TaskDependencyViewModel::taskId() const
{
    return m_taskId.toString(QUuid::WithoutBraces);
}

QString TaskDependencyViewModel::taskTitle() const
{
    return m_taskTitle;
}

int TaskDependencyViewModel::count() const noexcept
{
    return m_candidates.size();
}

int TaskDependencyViewModel::selectedCount() const noexcept
{
    return m_selectedPredecessors.size();
}

bool TaskDependencyViewModel::dirty() const noexcept
{
    return m_selectedPredecessors != m_originalPredecessors;
}

bool TaskDependencyViewModel::canSave() const noexcept
{
    return !m_taskId.isNull() && dirty();
}

QString TaskDependencyViewModel::errorMessage() const
{
    return m_errorMessage;
}

bool TaskDependencyViewModel::beginEdit(const QString &taskId)
{
    const model::TaskId id = QUuid::fromString(taskId.trimmed());
    if (id.isNull()) {
        setErrorMessage(taskErrorMessage(model::TaskError::NotFound));
        return false;
    }

    auto contextResult = m_taskService.taskDependencyEditContext(id);
    if (!contextResult.ok()) {
        setErrorMessage(dependencyErrorMessage(contextResult.error,
                                               contextResult.context));
        return false;
    }
    replaceDraft(std::move(*contextResult.value));
    return true;
}

bool TaskDependencyViewModel::setPredecessorSelected(const QString &predecessorTaskId,
                                                     const bool selected)
{
    const model::TaskId id = QUuid::fromString(predecessorTaskId.trimmed());
    const int row = candidateRow(id);
    if (id.isNull() || row < 0) {
        setErrorMessage(taskErrorMessage(model::TaskError::NotFound));
        return false;
    }

    if (selected && !m_selectablePredecessors.contains(id)) {
        setErrorMessage(QStringLiteral("不能新增已归档或已取消任务作为前置任务。"));
        return false;
    }
    if (m_selectedPredecessors.contains(id) == selected) {
        setErrorMessage({});
        return true;
    }

    if (selected) {
        m_selectedPredecessors.insert(id);
    } else {
        m_selectedPredecessors.remove(id);
    }
    emit dataChanged(index(row), index(row), {SelectedRole, SelectableRole});
    notifySelectionChanged();
    setErrorMessage({});
    return true;
}

bool TaskDependencyViewModel::save()
{
    if (!canSave()) {
        setErrorMessage(QStringLiteral("没有需要保存的依赖更改。"));
        return false;
    }

    // 按候选投影顺序提交稳定 ID，避免 QSet 的无序迭代影响测试和日志。
    QList<model::TaskId> predecessorIds;
    predecessorIds.reserve(m_selectedPredecessors.size());
    for (const model::Task &candidate : m_candidates) {
        if (m_selectedPredecessors.contains(candidate.id())) {
            predecessorIds.push_back(candidate.id());
        }
    }

    const auto result = m_taskService.replaceTaskPredecessors(m_taskId, predecessorIds);
    if (!result.ok()) {
        setErrorMessage(dependencyErrorMessage(result.error, result.context));
        return false;
    }

    m_originalPredecessors = m_selectedPredecessors;
    emit formStateChanged();
    setErrorMessage({});
    emit saved(taskId());
    return true;
}

void TaskDependencyViewModel::cancel()
{
    if (m_selectedPredecessors != m_originalPredecessors) {
        m_selectedPredecessors = m_originalPredecessors;
        if (!m_candidates.isEmpty()) {
            emit dataChanged(index(0), index(m_candidates.size() - 1),
                             {SelectedRole, SelectableRole});
        }
        notifySelectionChanged();
    }
    setErrorMessage({});
    emit cancelled();
}

void TaskDependencyViewModel::clearError()
{
    setErrorMessage({});
}

QString TaskDependencyViewModel::statusText(const model::TaskStatus status)
{
    return taskStatusText(status);
}

QString TaskDependencyViewModel::priorityText(const model::TaskPriority priority)
{
    return taskPriorityText(priority);
}

int TaskDependencyViewModel::candidateRow(const model::TaskId &taskId) const
{
    for (int row = 0; row < m_candidates.size(); ++row) {
        if (m_candidates.at(row).id() == taskId) {
            return row;
        }
    }
    return -1;
}

QString TaskDependencyViewModel::taskDisplayName(const model::TaskId &taskId) const
{
    const QString shortId = taskId.toString(QUuid::WithoutBraces).left(8);
    const QString title = m_taskTitles.value(taskId);
    if (!title.isEmpty()) {
        return QStringLiteral("%1（%2）").arg(title, shortId);
    }
    return QStringLiteral("未知任务（%1）").arg(shortId);
}

QString TaskDependencyViewModel::dependencyErrorMessage(
    const model::TaskError error,
    const model::TaskErrorContext &context) const
{
    const auto formatTasks = [this](const QList<model::TaskId> &ids,
                                    const QString &separator) {
        QStringList names;
        names.reserve(ids.size());
        for (const model::TaskId &id : ids) {
            names.push_back(taskDisplayName(id));
        }
        return names.join(separator);
    };

    if (error == model::TaskError::DependencyCycle && !context.cyclePath.isEmpty()) {
        return QStringLiteral("检测到循环依赖：%1。")
            .arg(formatTasks(context.cyclePath, QStringLiteral(" → ")));
    }
    if (error == model::TaskError::TaskBlocked
        && !context.blockingTaskIds.isEmpty()) {
        return QStringLiteral("任务仍被以下前置任务阻塞：%1。")
            .arg(formatTasks(context.blockingTaskIds, QStringLiteral("、")));
    }
    if (error == model::TaskError::DependencyStateConflict
        && !context.conflictingTaskIds.isEmpty()) {
        return QStringLiteral("状态修改会影响以下后继任务：%1。")
            .arg(formatTasks(context.conflictingTaskIds, QStringLiteral("、")));
    }
    if (error == model::TaskError::DependencyPredecessorNotEligible
        && !context.conflictingTaskIds.isEmpty()) {
        return QStringLiteral("不能新增已归档或已取消前置任务：%1。")
            .arg(formatTasks(context.conflictingTaskIds, QStringLiteral("、")));
    }
    return taskErrorMessage(error);
}

void TaskDependencyViewModel::replaceDraft(
    model::TaskDependencyEditContext context)
{
    beginResetModel();
    m_taskId = context.targetTask.id();
    m_taskTitle = context.targetTask.title();
    m_candidates.clear();
    m_selectedPredecessors.clear();
    m_selectablePredecessors.clear();
    m_candidates.reserve(context.candidates.size());
    for (const model::TaskDependencyCandidate &candidate : context.candidates) {
        m_candidates.append(candidate.task);
        if (candidate.selected) {
            m_selectedPredecessors.insert(candidate.task.id());
        }
        if (candidate.selectable) {
            m_selectablePredecessors.insert(candidate.task.id());
        }
    }
    m_taskTitles = std::move(context.taskTitles);
    m_originalPredecessors = m_selectedPredecessors;
    endResetModel();

    setErrorMessage({});
    emit contextChanged();
    emit countChanged();
    emit selectionChanged();
    emit formStateChanged();
}

void TaskDependencyViewModel::notifySelectionChanged()
{
    emit selectionChanged();
    emit formStateChanged();
}

void TaskDependencyViewModel::setErrorMessage(const QString &message)
{
    if (m_errorMessage == message) {
        return;
    }
    m_errorMessage = message;
    emit errorMessageChanged();
}

void TaskDependencyViewModel::reloadCategories()
{
    if (!m_categoryService) return;
    const auto result = m_categoryService->listCategories();
    if (!result.ok()) return;
    m_categories = *result.value;
    if (!m_candidates.isEmpty()) {
        emit dataChanged(index(0), index(m_candidates.size() - 1),
                         {CategoryNameRole, CategoryAccentRole, HasCategoryRole});
    }
}

const model::TaskCategory *TaskDependencyViewModel::categoryForTask(
    const model::Task &task) const
{
    if (!task.categoryId().has_value()) return nullptr;
    const auto iterator = std::find_if(
        m_categories.cbegin(), m_categories.cend(), [&](const auto &category) {
            return category.id == *task.categoryId();
        });
    return iterator == m_categories.cend() ? nullptr : &*iterator;
}

} // namespace smartmate::viewmodel
