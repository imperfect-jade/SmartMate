#include "planner/TaskOrderingPolicy.h"

#include "dependencies/TaskDependencyGraph.h"

#include <QHash>

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

[[nodiscard]] bool baseOrderComesBefore(const Task &left,
                                        const Task &right,
                                        const QDateTime &nowUtc)
{
    const int leftGroup = statusGroup(left.status());
    const int rightGroup = statusGroup(right.status());
    if (leftGroup != rightGroup) {
        return leftGroup < rightGroup;
    }
    if (left.status() == TaskStatus::Todo) {
        return todoComesBefore(left, right, nowUtc);
    }
    if (leftGroup >= 2) {
        return recentlyUpdatedComesBefore(left, right);
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
    return orderTasks(tasks, {}, nowUtc);
}

QList<PlannedTask> orderTasks(const QList<Task> &tasks,
                              const QList<TaskDependency> &dependencies,
                              const QDateTime &nowUtc)
{
    const TaskDependencyGraph graph{tasks, dependencies};
    QList<const Task *> readyTasks;
    QList<const Task *> blockedTasks;
    QList<const Task *> terminalTasks;
    QList<const Task *> archivedTasks;
    QHash<TaskId, const Task *> blockedById;

    for (const Task &task : tasks) {
        if (task.status() == TaskStatus::Archived) {
            archivedTasks.append(&task);
            continue;
        }
        if (task.status() == TaskStatus::Done
            || task.status() == TaskStatus::Cancelled) {
            terminalTasks.append(&task);
            continue;
        }
        if (graph.dependencyState(task.id()).blocked) {
            blockedTasks.append(&task);
            blockedById.insert(task.id(), &task);
        } else {
            readyTasks.append(&task);
        }
    }

    const auto baseComparator = [&nowUtc](const Task *left, const Task *right) {
        return baseOrderComesBefore(*left, *right, nowUtc);
    };
    std::sort(readyTasks.begin(), readyTasks.end(), baseComparator);
    std::sort(terminalTasks.begin(), terminalTasks.end(), baseComparator);
    std::sort(archivedTasks.begin(), archivedTasks.end(), baseComparator);

    // Blocked 分组内部使用 Kahn 排序。跨越 Ready→Blocked 的边无需计入入度，
    // 因为整个 Ready 分组已经固定出现在 Blocked 之前。
    QHash<TaskId, int> blockedIndegrees;
    QHash<TaskId, QList<TaskId>> blockedSuccessors;
    for (const Task *task : blockedTasks) {
        blockedIndegrees.insert(task->id(), 0);
    }
    for (const TaskDependency &dependency : dependencies) {
        if (blockedById.contains(dependency.predecessorId)
            && blockedById.contains(dependency.successorId)) {
            ++blockedIndegrees[dependency.successorId];
            blockedSuccessors[dependency.predecessorId].append(dependency.successorId);
        }
    }

    QList<const Task *> availableBlocked;
    for (const Task *task : blockedTasks) {
        if (blockedIndegrees.value(task->id()) == 0) {
            availableBlocked.append(task);
        }
    }

    QList<const Task *> topologicalBlocked;
    while (!availableBlocked.isEmpty()) {
        std::sort(availableBlocked.begin(), availableBlocked.end(), baseComparator);
        const Task *task = availableBlocked.takeFirst();
        topologicalBlocked.append(task);
        for (const TaskId &successorId : blockedSuccessors.value(task->id())) {
            const int newIndegree = blockedIndegrees.value(successorId) - 1;
            blockedIndegrees.insert(successorId, newIndegree);
            if (newIndegree == 0) {
                availableBlocked.append(blockedById.value(successorId));
            }
        }
    }

    // Service 会拒绝持久化环；若直接调用策略时输入了坏图，仍稳定保留全部任务。
    if (topologicalBlocked.size() != blockedTasks.size()) {
        QList<const Task *> cyclicRemainder;
        for (const Task *task : blockedTasks) {
            if (!topologicalBlocked.contains(task)) {
                cyclicRemainder.append(task);
            }
        }
        std::sort(cyclicRemainder.begin(), cyclicRemainder.end(), baseComparator);
        topologicalBlocked.append(cyclicRemainder);
    }

    QList<const Task *> orderedTasks;
    orderedTasks.reserve(tasks.size());
    orderedTasks.append(readyTasks);
    orderedTasks.append(topologicalBlocked);
    orderedTasks.append(terminalTasks);
    orderedTasks.append(archivedTasks);

    QList<PlannedTask> plan;
    plan.reserve(orderedTasks.size());
    for (const Task *task : orderedTasks) {
        plan.append(PlannedTask{*task,
                                reasonFor(*task, nowUtc),
                                graph.dependencyState(task->id()),
                                {}});
    }
    return plan;
}

} // namespace smartmate::model
