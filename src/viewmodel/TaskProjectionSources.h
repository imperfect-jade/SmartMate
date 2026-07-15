#pragma once

#include "TaskPlanProjection.h"
#include "domain/TaskCategory.h"
#include "services/TaskCategoryResult.h"
#include "services/TaskResult.h"

#include <QObject>
#include <QTimer>

namespace smartmate::model {
class TaskCategoryService;
class TaskService;
}

namespace smartmate::viewmodel {

/// ViewModel 层共享的只读计划投影源；统一响应 Model 失效信号和时间刷新。
class TaskPlanProjectionSource final : public QObject {
    Q_OBJECT

public:
    explicit TaskPlanProjectionSource(
        model::TaskService &taskService,
        model::TaskCategoryService *categoryService = nullptr,
        QObject *parent = nullptr);

    [[nodiscard]] const TaskPlanProjection &projection() const noexcept;
    [[nodiscard]] model::TaskError lastError() const noexcept;

public slots:
    /// 同步重查 Model 计划；失败时保留最后一次成功快照。
    void refresh();

signals:
    /// 仅在成功快照内容实际变化时发送。
    void projectionChanged();
    /// 每次成功刷新均发送，供消费者清除各自的展示错误。
    void refreshSucceeded();
    /// 每次失败刷新均发送；消费者按自身 Contract 语义决定是否展示错误。
    void refreshFailed();

private:
    model::TaskService &m_taskService;
    QTimer m_refreshTimer;
    TaskPlanProjection m_projection;
    model::TaskError m_lastError{model::TaskError::None};
};

/// ViewModel 层共享的只读类别目录源；不承载类别编辑草稿或命令。
class TaskCategoryProjectionSource final : public QObject {
    Q_OBJECT

public:
    explicit TaskCategoryProjectionSource(
        model::TaskCategoryService *categoryService = nullptr,
        QObject *parent = nullptr);

    [[nodiscard]] const QList<model::TaskCategory> &categories() const noexcept;
    [[nodiscard]] model::TaskCategoryError lastError() const noexcept;

public slots:
    /// 同步重查类别目录；无类别 Service 时保持成功的空目录。
    void refresh();

signals:
    /// 仅在成功目录内容实际变化时发送。
    void categoriesChanged();
    void refreshSucceeded();
    void refreshFailed();
    /// 类别归属变化不必重读目录，但会使依赖图领域快照失效。
    void taskCategoryAssignmentsChanged();

private:
    model::TaskCategoryService *m_categoryService{nullptr};
    QList<model::TaskCategory> m_categories;
    model::TaskCategoryError m_lastError{model::TaskCategoryError::None};
};

} // namespace smartmate::viewmodel
