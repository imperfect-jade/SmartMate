#include "planner/TaskOrderingPolicy.h"

#include <algorithm>
#include <utility>

namespace smartmate::model {
namespace {

[[nodiscard]] int statusGroup(const TaskStatus status) noexcept
{
    switch (status) {
    case TaskStatus::InProgress:
        return 0;
    case TaskStatus::Todo:
        return 1;
    case TaskStatus::Done:
    case TaskStatus::Cancelled:
        return 2;
    case TaskStatus::Archived:
        return 3;
    }
    return 4;
}

[[nodiscard]] int priorityRank(const TaskPriority priority) noexcept
{
    switch (priority) {
    case TaskPriority::Urgent:
        return 0;
    case TaskPriority::High:
        return 1;
    case TaskPriority::Normal:
        return 2;
    case TaskPriority::Low:
        return 3;
    }
    return 4;
}

[[nodiscard]] QString stableId(const Task &task)
{
    return task.id().toString(QUuid::WithoutBraces);
}

[[nodiscard]] bool isOverdue(const Task &task, const QDateTime &nowUtc)
{
    // 截止时间恰好等于当前时刻仍不算逾期。
    return task.deadline().has_value() && *task.deadline() < nowUtc;
}

[[nodiscard]] bool todoComesBefore(const Task &left,
                                   const Task &right,
                                   const QDateTime &nowUtc)
{
    const bool leftOverdue = isOverdue(left, nowUtc);
    const bool rightOverdue = isOverdue(right, nowUtc);
    if (leftOverdue != rightOverdue) {
        return leftOverdue;
    }

    const int leftPriority = priorityRank(left.priority());
    const int rightPriority = priorityRank(right.priority());
    if (leftPriority != rightPriority) {
        return leftPriority < rightPriority;
    }

    const bool leftHasDeadline = left.deadline().has_value();
    const bool rightHasDeadline = right.deadline().has_value();
    if (leftHasDeadline != rightHasDeadline) {
        return leftHasDeadline;
    }
    if (leftHasDeadline && *left.deadline() != *right.deadline()) {
        return *left.deadline() < *right.deadline();
    }

    if (left.createdAtUtc() != right.createdAtUtc()) {
        return left.createdAtUtc() < right.createdAtUtc();
    }
    return stableId(left) < stableId(right);
}

[[nodiscard]] bool recentlyUpdatedComesBefore(const Task &left,
                                              const Task &right)
{
    if (left.updatedAtUtc() != right.updatedAtUtc()) {
        return left.updatedAtUtc() > right.updatedAtUtc();
    }
    return stableId(left) < stableId(right);
}

[[nodiscard]] TaskOrderReason reasonFor(const Task &task,
                                        const QDateTime &nowUtc)
{
    switch (task.status()) {
    case TaskStatus::InProgress:
        return TaskOrderReason::InProgress;
    case TaskStatus::Todo:
        if (isOverdue(task, nowUtc)) {
            return TaskOrderReason::Overdue;
        }
        if (task.priority() == TaskPriority::Urgent) {
            return TaskOrderReason::UrgentPriority;
        }
        if (task.priority() == TaskPriority::High) {
            return TaskOrderReason::HighPriority;
        }
        if (task.deadline().has_value()) {
            return TaskOrderReason::UpcomingDeadline;
        }
        return TaskOrderReason::Todo;
    case TaskStatus::Done:
        return TaskOrderReason::Completed;
    case TaskStatus::Cancelled:
        return TaskOrderReason::Cancelled;
    case TaskStatus::Archived:
        return TaskOrderReason::Archived;
    }
    return TaskOrderReason::Todo;
}

} // namespace

QList<PlannedTask> orderTasks(const QList<Task> &tasks, const QDateTime &nowUtc)
{
    QList<Task> orderedTasks = tasks;
    std::sort(orderedTasks.begin(), orderedTasks.end(),
              [&nowUtc](const Task &left, const Task &right) {
                  const int leftGroup = statusGroup(left.status());
                  const int rightGroup = statusGroup(right.status());
                  if (leftGroup != rightGroup) {
                      return leftGroup < rightGroup;
                  }

                  if (left.status() == TaskStatus::Todo) {
                      return todoComesBefore(left, right, nowUtc);
                  }
                  if (leftGroup >= 2) {
                      // 已完成与已取消共享一个分组；归档位于独立的最后分组，
                      // 两者都使用最近更新优先的确定性顺序。
                      return recentlyUpdatedComesBefore(left, right);
                  }

                  // 业务规则保证最多一个进行中任务；稳定 ID 仍为异常输入兜底。
                  return stableId(left) < stableId(right);
              });

    QList<PlannedTask> plan;
    plan.reserve(orderedTasks.size());
    for (Task &task : orderedTasks) {
        const TaskOrderReason reason = reasonFor(task, nowUtc);
        plan.append(PlannedTask{std::move(task), reason});
    }
    return plan;
}

} // namespace smartmate::model
