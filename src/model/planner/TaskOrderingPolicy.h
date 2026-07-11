#pragma once

#include "domain/Task.h"
#include "domain/TaskDependency.h"

#include <QDateTime>
#include <QList>

namespace smartmate::model {

/// 解释任务出现在推荐位置的主导语义；ViewModel 仅负责将其映射为展示文案。
enum class TaskOrderReason {
    InProgress,
    Overdue,
    UrgentPriority,
    HighPriority,
    UpcomingDeadline,
    Todo,
    Completed,
    Cancelled,
    Archived,
};

/// 当前完整快照下的命令资格；ViewModel 只投影这些结果，不复制领域规则。
struct TaskCommandAvailability final {
    bool canEditTask{false};
    bool canEditDependencies{false};
    bool canStart{false};
    bool canCancel{false};
    bool canComplete{false};
    bool canRedo{false};
    bool canArchive{false};
    bool canRestore{false};

    friend bool operator==(const TaskCommandAvailability &,
                           const TaskCommandAvailability &) = default;
};

/// 推荐顺序中的领域快照及其理由，不保存列表行号或持久化排名。
struct PlannedTask final {
    Task task;
    TaskOrderReason reason;
    TaskDependencyState dependencyState;
    TaskCommandAvailability availability;

    friend bool operator==(const PlannedTask &, const PlannedTask &) = default;
};

/// 按任务状态、逾期、优先级和时间生成稳定推荐顺序。
///
/// `nowUtc` 由调用方注入，使“逾期”判断可以确定性测试；排序不会修改输入任务，
/// 也不会把随时间变化的派生排名写入 Repository。
[[nodiscard]] QList<PlannedTask> orderTasks(const QList<Task> &tasks,
                                            const QDateTime &nowUtc);

/// 先排列当前 Ready 任务，再以拓扑顺序排列 Blocked 任务；同一候选集合继续复用
/// 逾期、优先级、截止时间和稳定 ID 规则。
[[nodiscard]] QList<PlannedTask> orderTasks(
    const QList<Task> &tasks,
    const QList<TaskDependency> &dependencies,
    const QDateTime &nowUtc);

} // namespace smartmate::model
