#pragma once

#include "domain/TaskCategory.h"

#include <QList>
#include <QString>

#include <optional>
#include <utility>

namespace smartmate::model {

/// 类别业务错误使用独立枚举，避免ViewModel解析Repository技术文本。
enum class TaskCategoryError {
    None,
    EmptyName,
    NameTooLong,
    DuplicateName,
    InvalidColor,
    NotFound,
    PersistenceFailure,
};

/// 类别Service的结构化结果；成功时必须同时拥有value且error为None。
template<typename T>
struct TaskCategoryServiceResult final {
    std::optional<T> value;
    TaskCategoryError error{TaskCategoryError::None};
    QString detail;

    [[nodiscard]] bool ok() const noexcept
    {
        return error == TaskCategoryError::None && value.has_value();
    }

    [[nodiscard]] static TaskCategoryServiceResult success(T resultValue)
    {
        TaskCategoryServiceResult result;
        result.value.emplace(std::move(resultValue));
        return result;
    }

    [[nodiscard]] static TaskCategoryServiceResult failure(
        TaskCategoryError resultError,
        QString resultDetail = {})
    {
        TaskCategoryServiceResult result;
        result.error = resultError;
        result.detail = std::move(resultDetail);
        return result;
    }
};

/// 删除成功后返回被删除实体和解除归属的任务数量，供确认结果投影。
struct TaskCategoryDeletionOutcome final {
    TaskCategory category;
    int unassignedTaskCount{0};
};

using TaskCategoryResult = TaskCategoryServiceResult<TaskCategory>;
using TaskCategoryListResult = TaskCategoryServiceResult<QList<TaskCategory>>;
using TaskCategoryDeletionResult =
    TaskCategoryServiceResult<TaskCategoryDeletionOutcome>;

} // namespace smartmate::model
