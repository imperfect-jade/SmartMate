#include "TaskFocusViewModel.h"

#include "TaskCategoryPresentation.h"
#include "TaskPresentationFormatter.h"

#include <algorithm>

namespace smartmate::viewmodel {

TaskFocusViewModel::TaskFocusViewModel(
    TaskPlanProjectionSource &planSource,
    TaskCategoryProjectionSource &categorySource,
    QObject *parent)
    : TaskFocusContract(parent)
    , m_planSource(planSource)
    , m_categorySource(categorySource)
{
    connect(&m_planSource, &TaskPlanProjectionSource::projectionChanged,
            this, &TaskFocusViewModel::applyPlanProjection);
    connect(&m_categorySource, &TaskCategoryProjectionSource::categoriesChanged,
            this, &TaskFocusViewModel::applyCategories);
    applyPlanProjection();
}

TaskFocusContract::FocusState TaskFocusViewModel::focusState() const noexcept { return m_focusState; }
QString TaskFocusViewModel::focusTaskId() const { return m_focusTaskId.isNull() ? QString{} : m_focusTaskId.toString(QUuid::WithoutBraces); }
QString TaskFocusViewModel::focusTitle() const { const auto *task = focusTask(); return task ? task->title() : QString{}; }
QString TaskFocusViewModel::focusDescription() const { const auto *task = focusTask(); return task ? task->description() : QString{}; }
QString TaskFocusViewModel::focusStatusText() const { const auto *task = focusTask(); return task ? taskStatusText(task->status()) : QString{}; }
QString TaskFocusViewModel::focusPriorityText() const { const auto *task = focusTask(); return task ? taskPriorityText(task->priority()) : QString{}; }
QString TaskFocusViewModel::focusDeadlineText() const { const auto *task = focusTask(); return task ? taskDeadlineText(*task, {}) : QString{}; }
int TaskFocusViewModel::focusEstimatedMinutes() const noexcept { const auto *task = focusTask(); return task && task->estimatedMinutes() ? *task->estimatedMinutes() : 0; }
QString TaskFocusViewModel::focusReasonText() const { return m_planSource.projection().orderReasonTexts.value(m_focusTaskId); }
bool TaskFocusViewModel::focusOverdue() const noexcept { return m_planSource.projection().overdueStates.value(m_focusTaskId, false); }
bool TaskFocusViewModel::focusCanStart() const noexcept { return m_planSource.projection().availabilityFor(m_focusTaskId).canStart; }
bool TaskFocusViewModel::focusCanComplete() const noexcept { return m_planSource.projection().availabilityFor(m_focusTaskId).canComplete; }
QString TaskFocusViewModel::focusCategoryName() const { const auto *category = focusCategory(); return category ? category->name : QString{}; }
QString TaskFocusViewModel::focusCategoryAccent() const { const auto *category = focusCategory(); return category ? taskCategoryAccent(category->color) : taskUncategorizedAccent(); }
bool TaskFocusViewModel::focusHasCategory() const noexcept { return focusCategory() != nullptr; }

void TaskFocusViewModel::applyPlanProjection()
{
    const auto &projection = m_planSource.projection();
    model::TaskId focusId;
    FocusState state = FocusState::NoTasks;
    bool hasTodo = false;
    // 唯一进行中任务优先于推荐；没有进行中任务时选择首个 Model 判定可开始的任务。
    for (const model::Task &task : projection.tasks) {
        if (task.status() == model::TaskStatus::InProgress) {
            focusId = task.id();
            state = FocusState::InProgress;
            break;
        }
        hasTodo = hasTodo || task.status() == model::TaskStatus::Todo;
    }
    if (focusId.isNull()) {
        for (const model::Task &task : projection.tasks) {
            if (projection.availabilityFor(task.id()).canStart) {
                focusId = task.id();
                state = FocusState::Suggested;
                break;
            }
        }
    }
    if (focusId.isNull()) state = hasTodo ? FocusState::AllBlocked : FocusState::NoTasks;

    m_focusTaskId = focusId;
    m_focusState = state;
    emit focusTaskChanged();
}

void TaskFocusViewModel::applyCategories()
{
    emit focusTaskChanged();
}

const model::Task *TaskFocusViewModel::focusTask() const { return m_planSource.projection().taskForId(m_focusTaskId); }

const model::TaskCategory *TaskFocusViewModel::focusCategory() const
{
    const model::Task *task = focusTask();
    if (!task || !task->categoryId()) return nullptr;
    const auto &categories = m_categorySource.categories();
    const auto it = std::find_if(categories.cbegin(), categories.cend(),
        [task](const model::TaskCategory &category) { return category.id == *task->categoryId(); });
    return it == categories.cend() ? nullptr : &*it;
}

} // namespace smartmate::viewmodel
