#include "domain/TaskStateMachine.h"

namespace smartmate::model {
namespace {

[[nodiscard]] TaskStatus safeRestoreStatus(const Task &task) noexcept
{
    // 正常新数据只会记录 Done/Cancelled；缺失或旧活动状态统一降级到 Todo。
    if (!task.statusBeforeArchive().has_value()) {
        return TaskStatus::Todo;
    }

    switch (*task.statusBeforeArchive()) {
    case TaskStatus::Done:
        return TaskStatus::Done;
    case TaskStatus::Cancelled:
        return TaskStatus::Cancelled;
    case TaskStatus::Todo:
    case TaskStatus::InProgress:
    case TaskStatus::Archived:
        return TaskStatus::Todo;
    }
    // optional 可能由旧数据库中的非法枚举值重建；未知值也必须安全降级。
    return TaskStatus::Todo;
}

} // namespace

std::optional<TaskStatus> TaskStateMachine::targetStatus(
    const Task &task,
    const TaskTransition transition) noexcept
{
    // 所有生命周期入口共用这一张显式矩阵，禁止调用方自行指定目标状态。
    switch (transition) {
    case TaskTransition::Start:
        return task.status() == TaskStatus::Todo
            ? std::optional<TaskStatus>{TaskStatus::InProgress}
            : std::nullopt;
    case TaskTransition::Cancel:
        return task.status() == TaskStatus::Todo
                || task.status() == TaskStatus::InProgress
            ? std::optional<TaskStatus>{TaskStatus::Cancelled}
            : std::nullopt;
    case TaskTransition::Complete:
        return task.status() == TaskStatus::InProgress
            ? std::optional<TaskStatus>{TaskStatus::Done}
            : std::nullopt;
    case TaskTransition::Redo:
        return task.status() == TaskStatus::Done
                || task.status() == TaskStatus::Cancelled
            ? std::optional<TaskStatus>{TaskStatus::Todo}
            : std::nullopt;
    case TaskTransition::Archive:
        return task.status() == TaskStatus::Done
                || task.status() == TaskStatus::Cancelled
            ? std::optional<TaskStatus>{TaskStatus::Archived}
            : std::nullopt;
    case TaskTransition::Restore:
        return task.status() == TaskStatus::Archived
            ? std::optional<TaskStatus>{safeRestoreStatus(task)}
            : std::nullopt;
    }
    return std::nullopt;
}

bool TaskStateMachine::canApply(const Task &task,
                                const TaskTransition transition) noexcept
{
    return targetStatus(task, transition).has_value();
}

} // namespace smartmate::model
