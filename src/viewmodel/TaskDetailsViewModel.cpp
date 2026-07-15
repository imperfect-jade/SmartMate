#include "TaskDetailsViewModel.h"

#include "TaskCategoryPresentation.h"
#include "TaskPresentationFormatter.h"

#include <algorithm>

namespace smartmate::viewmodel {

TaskDetailsViewModel::TaskDetailsViewModel(
    TaskPlanProjectionSource &planSource,
    TaskCategoryProjectionSource &categorySource,
    QObject *parent)
    : TaskDetailsContract(parent)
    , m_planSource(planSource)
    , m_categorySource(categorySource)
{
    connect(&m_planSource, &TaskPlanProjectionSource::projectionChanged,
            this, &TaskDetailsViewModel::applyPlanProjection);
    connect(&m_categorySource, &TaskCategoryProjectionSource::categoriesChanged,
            this, &TaskDetailsViewModel::applyCategories);
}

QString TaskDetailsViewModel::selectedTaskId() const { return m_selectedTaskId.isNull() ? QString{} : m_selectedTaskId.toString(QUuid::WithoutBraces); }
QString TaskDetailsViewModel::selectedTitle() const { const auto *task = selectedTask(); return task ? task->title() : QString{}; }
QString TaskDetailsViewModel::selectedDescription() const { const auto *task = selectedTask(); return task ? task->description() : QString{}; }
QString TaskDetailsViewModel::selectedStatusText() const { const auto *task = selectedTask(); return task ? taskStatusText(task->status()) : QString{}; }
QString TaskDetailsViewModel::selectedPriorityText() const { const auto *task = selectedTask(); return task ? taskPriorityText(task->priority()) : QString{}; }
QString TaskDetailsViewModel::selectedDeadlineText() const { const auto *task = selectedTask(); return task ? taskDeadlineText(*task, {}) : QString{}; }
int TaskDetailsViewModel::selectedEstimatedMinutes() const noexcept { const auto *task = selectedTask(); return task && task->estimatedMinutes() ? *task->estimatedMinutes() : 0; }
QString TaskDetailsViewModel::selectedCreatedAtText() const { const auto *task = selectedTask(); return task ? taskDateTimeText(task->createdAtUtc().toLocalTime()) : QString{}; }
QString TaskDetailsViewModel::selectedUpdatedAtText() const { const auto *task = selectedTask(); return task ? taskDateTimeText(task->updatedAtUtc().toLocalTime()) : QString{}; }
QString TaskDetailsViewModel::selectedReasonText() const { return m_planSource.projection().orderReasonTexts.value(m_selectedTaskId); }
QString TaskDetailsViewModel::selectedBlockingReasonText() const { return m_planSource.projection().dependencyProjections.value(m_selectedTaskId).blockingReasonText; }
int TaskDetailsViewModel::selectedPredecessorCount() const noexcept { return m_planSource.projection().dependencyProjections.value(m_selectedTaskId).predecessorCount; }
int TaskDetailsViewModel::selectedUnlockCount() const noexcept { return m_planSource.projection().dependencyProjections.value(m_selectedTaskId).unlockCount; }
bool TaskDetailsViewModel::selectedCanEditTask() const noexcept { return m_planSource.projection().availabilityFor(m_selectedTaskId).canEditTask; }
bool TaskDetailsViewModel::selectedCanEditDependencies() const noexcept { return m_planSource.projection().availabilityFor(m_selectedTaskId).canEditDependencies; }
QString TaskDetailsViewModel::selectedCategoryName() const { const auto *category = selectedCategory(); return category ? category->name : QString{}; }
QString TaskDetailsViewModel::selectedCategoryAccent() const { const auto *category = selectedCategory(); return category ? taskCategoryAccent(category->color) : taskUncategorizedAccent(); }
bool TaskDetailsViewModel::selectedHasCategory() const noexcept { return selectedCategory() != nullptr; }

bool TaskDetailsViewModel::selectTask(const QString &taskId)
{
    const model::TaskId id = QUuid::fromString(taskId.trimmed());
    if (id.isNull() || !m_planSource.projection().taskForId(id)) return false;
    if (m_selectedTaskId == id) return true;
    m_selectedTaskId = id;
    emit selectionChanged();
    return true;
}

void TaskDetailsViewModel::clearSelection()
{
    if (m_selectedTaskId.isNull()) return;
    m_selectedTaskId = model::TaskId{};
    emit selectionChanged();
}

void TaskDetailsViewModel::applyPlanProjection()
{
    const bool selectionRemoved = !m_selectedTaskId.isNull()
        && !m_planSource.projection().taskForId(m_selectedTaskId);
    if (selectionRemoved) m_selectedTaskId = model::TaskId{};
    emit selectionChanged();
}

void TaskDetailsViewModel::applyCategories()
{
    emit selectionChanged();
}

const model::Task *TaskDetailsViewModel::selectedTask() const { return m_planSource.projection().taskForId(m_selectedTaskId); }

const model::TaskCategory *TaskDetailsViewModel::selectedCategory() const
{
    const model::Task *task = selectedTask();
    if (!task || !task->categoryId()) return nullptr;
    const auto &categories = m_categorySource.categories();
    const auto it = std::find_if(categories.cbegin(), categories.cend(),
        [task](const model::TaskCategory &category) { return category.id == *task->categoryId(); });
    return it == categories.cend() ? nullptr : &*it;
}

} // namespace smartmate::viewmodel
