#pragma once

#include "domain/Task.h"
#include "planner/TaskOrderingPolicy.h"

#include <QList>
#include <QString>

#include <optional>
#include <utility>

namespace smartmate::model {

/// Model 与 ViewModel 之间传递的稳定错误类别；detail 仅用于技术诊断，不是界面文案。
enum class TaskError {
    None,
    EmptyTitle,
    TitleTooLong,
    DescriptionTooLong,
    InvalidDeadline,
    InvalidEstimate,
    InvalidPriority,
    InvalidStatus,
    NotFound,
    InProgressConflict,
    PersistenceFailure,
};

/// 无返回值的纯校验结果，成功时 error 为 None 且 detail 为空。
struct TaskValidationResult final {
    TaskError error{TaskError::None};
    QString detail;

    [[nodiscard]] bool ok() const noexcept
    {
        return error == TaskError::None;
    }

    [[nodiscard]] static TaskValidationResult success()
    {
        return {};
    }

    [[nodiscard]] static TaskValidationResult failure(TaskError resultError,
                                                       QString resultDetail = {})
    {
        TaskValidationResult result;
        result.error = resultError;
        result.detail = std::move(resultDetail);
        return result;
    }
};

/// Service 操作结果；成功必须同时具有 value 且 error 为 None。
template<typename T>
struct ServiceResult final {
    std::optional<T> value;
    TaskError error{TaskError::None};
    QString detail;

    [[nodiscard]] bool ok() const noexcept
    {
        return error == TaskError::None && value.has_value();
    }

    [[nodiscard]] static ServiceResult success(T resultValue)
    {
        ServiceResult result;
        result.value.emplace(std::move(resultValue));
        return result;
    }

    [[nodiscard]] static ServiceResult failure(TaskError resultError,
                                               QString resultDetail = {})
    {
        ServiceResult result;
        result.error = resultError;
        result.detail = std::move(resultDetail);
        return result;
    }
};

using TaskResult = ServiceResult<Task>;
using TaskListResult = ServiceResult<QList<Task>>;
using TaskPlanResult = ServiceResult<QList<PlannedTask>>;

} // namespace smartmate::model
