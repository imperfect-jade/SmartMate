#pragma once

#include "domain/Task.h"

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

/// 推荐顺序中的领域快照及其理由，不保存列表行号或持久化排名。
struct PlannedTask final {
    Task task;
    TaskOrderReason reason;

    friend bool operator==(const PlannedTask &, const PlannedTask &) = default;
};

/// 按任务状态、逾期、优先级和时间生成稳定推荐顺序。
///
/// `nowUtc` 由调用方注入，使“逾期”判断可以确定性测试；排序不会修改输入任务，
/// 也不会把随时间变化的派生排名写入 Repository。
[[nodiscard]] QList<PlannedTask> orderTasks(const QList<Task> &tasks,
                                            const QDateTime &nowUtc);

} // namespace smartmate::model
