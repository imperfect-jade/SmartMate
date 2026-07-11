#include "dependencies/TaskDependencyGraph.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <utility>

namespace smartmate::model {
namespace {

[[nodiscard]] QString stableId(const TaskId &taskId)
{
    return taskId.toString(QUuid::WithoutBraces);
}

void normalizeIds(QList<TaskId> &taskIds)
{
    std::sort(taskIds.begin(), taskIds.end(), [](const TaskId &left,
                                                 const TaskId &right) {
        return stableId(left) < stableId(right);
    });
    taskIds.erase(std::unique(taskIds.begin(), taskIds.end()), taskIds.end());
}

[[nodiscard]] QString edgeKey(const TaskDependency &dependency)
{
    return stableId(dependency.predecessorId) + QLatin1Char('>')
        + stableId(dependency.successorId);
}

} // namespace

TaskDependencyGraph::TaskDependencyGraph(QList<Task> tasks,
                                         QList<TaskDependency> dependencies)
    : m_tasks(std::move(tasks))
    , m_dependencies(std::move(dependencies))
{
    m_taskIndexes.reserve(m_tasks.size());
    for (qsizetype index = 0; index < m_tasks.size(); ++index) {
        m_taskIndexes.insert(m_tasks.at(index).id(), index);
    }
}

DependencyGraphValidation TaskDependencyGraph::validation() const
{
    QList<TaskId> missingTaskIds;
    for (const TaskDependency &dependency : m_dependencies) {
        if (!containsTask(dependency.predecessorId)) {
            missingTaskIds.append(dependency.predecessorId);
        }
        if (!containsTask(dependency.successorId)) {
            missingTaskIds.append(dependency.successorId);
        }
    }
    normalizeIds(missingTaskIds);
    if (!missingTaskIds.isEmpty()) {
        return {DependencyGraphError::MissingTask, missingTaskIds, {}};
    }

    QList<TaskId> selfDependencies;
    for (const TaskDependency &dependency : m_dependencies) {
        if (dependency.predecessorId == dependency.successorId) {
            selfDependencies.append(dependency.predecessorId);
        }
    }
    normalizeIds(selfDependencies);
    if (!selfDependencies.isEmpty()) {
        return {DependencyGraphError::SelfDependency, selfDependencies, {}};
    }

    QSet<QString> knownEdges;
    QList<TaskId> duplicateEndpoints;
    for (const TaskDependency &dependency : m_dependencies) {
        const QString key = edgeKey(dependency);
        if (knownEdges.contains(key)) {
            duplicateEndpoints.append(dependency.predecessorId);
            duplicateEndpoints.append(dependency.successorId);
        } else {
            knownEdges.insert(key);
        }
    }
    normalizeIds(duplicateEndpoints);
    if (!duplicateEndpoints.isEmpty()) {
        return {DependencyGraphError::DuplicateDependency, duplicateEndpoints, {}};
    }

    QHash<TaskId, QList<TaskId>> successorsByTask;
    for (const TaskDependency &dependency : m_dependencies) {
        successorsByTask[dependency.predecessorId].append(dependency.successorId);
    }
    for (auto iterator = successorsByTask.begin(); iterator != successorsByTask.end();
         ++iterator) {
        normalizeIds(iterator.value());
    }

    QList<TaskId> orderedTaskIds;
    orderedTaskIds.reserve(m_tasks.size());
    for (const Task &task : m_tasks) {
        orderedTaskIds.append(task.id());
    }
    normalizeIds(orderedTaskIds);

    // 0=未访问、1=当前DFS路径、2=已完成。显式栈避免极长依赖链耗尽调用栈。
    QHash<TaskId, int> colors;
    QList<TaskId> cyclePath;
    struct DfsFrame final {
        TaskId taskId;
        qsizetype nextSuccessorIndex{0};
    };

    for (const TaskId &rootTaskId : orderedTaskIds) {
        if (colors.value(rootTaskId, 0) != 0) {
            continue;
        }

        QList<DfsFrame> stack;
        stack.append({rootTaskId, 0});
        colors.insert(rootTaskId, 1);
        while (!stack.isEmpty()) {
            DfsFrame &frame = stack.last();
            const auto successors = successorsByTask.constFind(frame.taskId);
            if (successors == successorsByTask.cend()
                || frame.nextSuccessorIndex >= successors.value().size()) {
                colors.insert(frame.taskId, 2);
                stack.removeLast();
                continue;
            }

            const TaskId successorId =
                successors.value().at(frame.nextSuccessorIndex++);
            const int successorColor = colors.value(successorId, 0);
            if (successorColor == 0) {
                colors.insert(successorId, 1);
                stack.append({successorId, 0});
                continue;
            }
            if (successorColor != 1) {
                continue;
            }

            qsizetype cycleStart = 0;
            while (cycleStart < stack.size()
                   && stack.at(cycleStart).taskId != successorId) {
                ++cycleStart;
            }
            for (qsizetype index = cycleStart; index < stack.size(); ++index) {
                cyclePath.append(stack.at(index).taskId);
            }
            cyclePath.append(successorId);

            QList<TaskId> conflictingIds = cyclePath;
            normalizeIds(conflictingIds);
            return {DependencyGraphError::Cycle, conflictingIds, cyclePath};
        }
    }

    return {};
}

bool TaskDependencyGraph::containsTask(const TaskId &taskId) const
{
    return findTask(taskId) != nullptr;
}

QList<TaskId> TaskDependencyGraph::predecessorIds(const TaskId &taskId) const
{
    QList<TaskId> result;
    for (const TaskDependency &dependency : m_dependencies) {
        if (dependency.successorId == taskId) {
            result.append(dependency.predecessorId);
        }
    }
    normalizeIds(result);
    return result;
}

QList<TaskId> TaskDependencyGraph::successorIds(const TaskId &taskId) const
{
    QList<TaskId> result;
    for (const TaskDependency &dependency : m_dependencies) {
        if (dependency.predecessorId == taskId) {
            result.append(dependency.successorId);
        }
    }
    normalizeIds(result);
    return result;
}

QList<TaskId> TaskDependencyGraph::unsatisfiedPredecessorIds(const TaskId &taskId) const
{
    QList<TaskId> result;
    for (const TaskId &predecessorId : predecessorIds(taskId)) {
        const Task *predecessor = findTask(predecessorId);
        if (predecessor == nullptr || !satisfiesDependency(*predecessor)) {
            result.append(predecessorId);
        }
    }
    return result;
}

TaskDependencyState TaskDependencyGraph::dependencyState(const TaskId &taskId) const
{
    TaskDependencyState state;
    state.predecessorIds = predecessorIds(taskId);
    state.unsatisfiedPredecessorIds = unsatisfiedPredecessorIds(taskId);

    const Task *task = findTask(taskId);
    const bool active = task != nullptr
        && (task->status() == TaskStatus::Todo
            || task->status() == TaskStatus::InProgress);
    state.blocked = active && !state.unsatisfiedPredecessorIds.isEmpty();

    // 只有当前唯一未满足前置恰好是本任务时，完成本任务才会直接解锁后继。
    for (const TaskId &successorId : successorIds(taskId)) {
        const Task *successor = findTask(successorId);
        if (successor == nullptr || successor->status() != TaskStatus::Todo) {
            continue;
        }
        const QList<TaskId> blockers = unsatisfiedPredecessorIds(successorId);
        if (blockers.size() == 1 && blockers.constFirst() == taskId) {
            ++state.unlockCount;
        }
    }
    return state;
}

const QList<TaskDependency> &TaskDependencyGraph::dependencies() const noexcept
{
    return m_dependencies;
}

QHash<TaskId, int> TaskDependencyGraph::dependencyLevels() const
{
    if (!validation().ok()) {
        return {};
    }

    QHash<TaskId, int> levels;
    QHash<TaskId, int> indegrees;
    QHash<TaskId, QList<TaskId>> successorsByTask;
    QList<TaskId> readyTaskIds;
    for (const Task &task : m_tasks) {
        levels.insert(task.id(), 0);
        indegrees.insert(task.id(), 0);
    }
    for (const TaskDependency &dependency : m_dependencies) {
        ++indegrees[dependency.successorId];
        successorsByTask[dependency.predecessorId].append(dependency.successorId);
    }
    for (auto iterator = successorsByTask.begin(); iterator != successorsByTask.end();
         ++iterator) {
        normalizeIds(iterator.value());
    }
    for (const Task &task : m_tasks) {
        if (indegrees.value(task.id()) == 0) {
            readyTaskIds.append(task.id());
        }
    }
    normalizeIds(readyTaskIds);

    // Kahn 遍历保证只有在全部前置处理完毕后才确定层级，菱形图会选择最长路径。
    qsizetype nextReadyIndex = 0;
    qsizetype visitedCount = 0;
    while (nextReadyIndex < readyTaskIds.size()) {
        const TaskId taskId = readyTaskIds.at(nextReadyIndex++);
        ++visitedCount;
        for (const TaskId &successorId : successorsByTask.value(taskId)) {
            levels[successorId] = std::max(levels.value(successorId),
                                           levels.value(taskId) + 1);
            const int remainingPredecessors = indegrees.value(successorId) - 1;
            indegrees.insert(successorId, remainingPredecessors);
            if (remainingPredecessors == 0) {
                readyTaskIds.append(successorId);
            }
        }
    }

    // validation 已排除环；此防线避免未来调用路径在异常快照上返回半套层级。
    return visitedCount == m_tasks.size() ? levels : QHash<TaskId, int>{};
}

QList<TaskId> TaskDependencyGraph::connectedTaskIds(
    const QList<TaskId> &seedTaskIds) const
{
    QHash<TaskId, QList<TaskId>> neighborsByTask;
    for (const TaskDependency &dependency : m_dependencies) {
        if (!containsTask(dependency.predecessorId)
            || !containsTask(dependency.successorId)) {
            continue;
        }
        neighborsByTask[dependency.predecessorId].append(dependency.successorId);
        neighborsByTask[dependency.successorId].append(dependency.predecessorId);
    }
    for (auto iterator = neighborsByTask.begin(); iterator != neighborsByTask.end();
         ++iterator) {
        normalizeIds(iterator.value());
    }

    QList<TaskId> pendingTaskIds;
    for (const TaskId &taskId : seedTaskIds) {
        if (containsTask(taskId)) {
            pendingTaskIds.append(taskId);
        }
    }
    normalizeIds(pendingTaskIds);

    QSet<TaskId> visitedTaskIds;
    qsizetype nextPendingIndex = 0;
    while (nextPendingIndex < pendingTaskIds.size()) {
        const TaskId taskId = pendingTaskIds.at(nextPendingIndex++);
        if (visitedTaskIds.contains(taskId)) {
            continue;
        }
        visitedTaskIds.insert(taskId);
        for (const TaskId &neighborId : neighborsByTask.value(taskId)) {
            if (!visitedTaskIds.contains(neighborId)) {
                pendingTaskIds.append(neighborId);
            }
        }
    }

    QList<TaskId> connectedIds = visitedTaskIds.values();
    normalizeIds(connectedIds);
    return connectedIds;
}

bool TaskDependencyGraph::satisfiesDependency(const Task &task) noexcept
{
    if (task.status() == TaskStatus::Done) {
        return true;
    }
    return task.status() == TaskStatus::Archived
        && task.statusBeforeArchive() == std::optional<TaskStatus>{TaskStatus::Done};
}

const Task *TaskDependencyGraph::findTask(const TaskId &taskId) const
{
    const auto iterator = m_taskIndexes.constFind(taskId);
    return iterator == m_taskIndexes.cend()
        ? nullptr
        : &m_tasks.at(iterator.value());
}

} // namespace smartmate::model
