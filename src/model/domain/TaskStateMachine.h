#pragma once

#include "domain/Task.h"

#include <optional>

namespace smartmate::model {

/// 用户可执行的显式任务状态命令；普通字段编辑不得隐式触发这些转换。
enum class TaskTransition {
    /// Todo → InProgress。
    Start,
    /// Todo/InProgress → Cancelled。
    Cancel,
    /// InProgress → Done。
    Complete,
    /// Done/Cancelled → Todo。
    Redo,
    /// Done/Cancelled → Archived。
    Archive,
    /// Archived → 归档前终态，旧数据则安全恢复为 Todo。
    Restore,
};

/// 任务状态转换的唯一领域规则表，不访问 Repository，也不包含界面文案。
class TaskStateMachine final {
public:
    /// 返回命令的目标状态；来源状态不允许该命令时返回空值。
    ///
    /// Restore 会把旧数据中的 Todo/InProgress/非法恢复点安全归一为 Todo，
    /// 如果合法，返回目标状态；如果非法，返回 std::nullopt
    [[nodiscard]] static std::optional<TaskStatus> targetStatus(
        const Task &task,
        TaskTransition transition) noexcept;

    /// 仅判断状态矩阵资格；依赖、单进行中和持久化约束仍由 TaskService 检查。
    [[nodiscard]] static bool canApply(const Task &task,
                                       TaskTransition transition) noexcept;
};

} // namespace smartmate::model
