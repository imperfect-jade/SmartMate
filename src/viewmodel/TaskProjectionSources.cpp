#include "TaskProjectionSources.h"

#include "services/TaskCategoryService.h"
#include "services/TaskService.h"

#include <utility>

namespace smartmate::viewmodel {

TaskPlanProjectionSource::TaskPlanProjectionSource(
    model::TaskService &taskService,
    model::TaskCategoryService *categoryService,
    QObject *parent)
    : QObject(parent)
    , m_taskService(taskService)
    , m_refreshTimer(this)
{
    connect(&m_taskService, &model::TaskService::tasksChanged,
            this, &TaskPlanProjectionSource::refresh);
    connect(&m_taskService, &model::TaskService::dependenciesChanged,
            this, &TaskPlanProjectionSource::refresh);
    if (categoryService) {
        connect(categoryService,
                &model::TaskCategoryService::taskCategoryAssignmentsChanged,
                this, &TaskPlanProjectionSource::refresh);
    }
    connect(&m_refreshTimer, &QTimer::timeout,
            this, &TaskPlanProjectionSource::refresh);

    // 先建立初始快照，再启动唯一的时间派生状态刷新源。
    refresh();
    m_refreshTimer.start(60'000);
}

const TaskPlanProjection &TaskPlanProjectionSource::projection() const noexcept
{
    return m_projection;
}

model::TaskError TaskPlanProjectionSource::lastError() const noexcept
{
    return m_lastError;
}

void TaskPlanProjectionSource::refresh()
{
    const auto result = m_taskService.listRecommendedTasks();
    if (!result.ok()) {
        m_lastError = result.error;
        emit refreshFailed();
        return;
    }

    TaskPlanProjection projection = makeTaskPlanProjection(*result.value);
    const bool changed = projection != m_projection;
    if (changed) {
        m_projection = std::move(projection);
        emit projectionChanged();
    }
    m_lastError = model::TaskError::None;
    emit refreshSucceeded();
}

TaskCategoryProjectionSource::TaskCategoryProjectionSource(
    model::TaskCategoryService *categoryService,
    QObject *parent)
    : QObject(parent)
    , m_categoryService(categoryService)
{
    if (m_categoryService) {
        connect(m_categoryService, &model::TaskCategoryService::categoriesChanged,
                this, &TaskCategoryProjectionSource::refresh);
        connect(m_categoryService,
                &model::TaskCategoryService::taskCategoryAssignmentsChanged,
                this, &TaskCategoryProjectionSource::taskCategoryAssignmentsChanged);
        refresh();
    }
}

const QList<model::TaskCategory> &
TaskCategoryProjectionSource::categories() const noexcept
{
    return m_categories;
}

model::TaskCategoryError
TaskCategoryProjectionSource::lastError() const noexcept
{
    return m_lastError;
}

void TaskCategoryProjectionSource::refresh()
{
    if (!m_categoryService) {
        m_lastError = model::TaskCategoryError::None;
        emit refreshSucceeded();
        return;
    }

    const auto result = m_categoryService->listCategories();
    if (!result.ok()) {
        m_lastError = result.error;
        emit refreshFailed();
        return;
    }

    QList<model::TaskCategory> categories = *result.value;
    const bool changed = categories != m_categories;
    if (changed) {
        m_categories = std::move(categories);
        emit categoriesChanged();
    }
    m_lastError = model::TaskCategoryError::None;
    emit refreshSucceeded();
}

} // namespace smartmate::viewmodel
