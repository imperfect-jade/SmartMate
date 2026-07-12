#pragma once

#include "domain/Task.h"
#include "domain/TaskDependency.h"

#include <QHash>
#include <QList>

namespace smartmate::model {

/// 依赖快照本身不合法时的结构化类别；具体关联 ID 由 validation() 返回。
enum class DependencyGraphError {
    None,
    MissingTask,
    SelfDependency,
    DuplicateDependency,
    Cycle,
};

/// 图校验上下文；cyclePath 在成环时包含首尾相同的完整闭合路径。
struct DependencyGraphValidation final {
    DependencyGraphError error{DependencyGraphError::None};
    QList<TaskId> conflictingTaskIds;
    QList<TaskId> cyclePath;

    [[nodiscard]] bool ok() const noexcept
    {
        return error == DependencyGraphError::None;
    }
};

/// 任务与 Finish-to-Start 关系的纯 C++ 快照算法，不访问 Repository 或界面。
class TaskDependencyGraph final {
public:
    TaskDependencyGraph(QList<Task> tasks, QList<TaskDependency> dependencies);

    /// 依次检查悬空端点、自依赖、重复边和有向环。
    [[nodiscard]] DependencyGraphValidation validation() const;
    [[nodiscard]] bool containsTask(const TaskId &taskId) const;
    [[nodiscard]] QList<TaskId> predecessorIds(const TaskId &taskId) const;
    [[nodiscard]] QList<TaskId> successorIds(const TaskId &taskId) const;
    [[nodiscard]] QList<TaskId> unsatisfiedPredecessorIds(const TaskId &taskId) const;
    [[nodiscard]] QList<TaskId> cancelledPredecessorIds(const TaskId &taskId) const;
    [[nodiscard]] TaskDependencyState dependencyState(const TaskId &taskId) const;
    [[nodiscard]] const QList<TaskDependency> &dependencies() const noexcept;

    /// 返回每个节点的最长前置链层级；根节点为 0，非法图返回空映射。
    [[nodiscard]] QHash<TaskId, int> dependencyLevels() const;
    /// 返回指定节点全部传递前置，结果按稳定 TaskId 排序；非法或缺失节点返回空集合。
    [[nodiscard]] QList<TaskId> predecessorClosure(const TaskId &taskId) const;
    /// 返回指定节点全部传递后继，结果按稳定 TaskId 排序；非法或缺失节点返回空集合。
    [[nodiscard]] QList<TaskId> successorClosure(const TaskId &taskId) const;
    /// 将有向边视为无向连接，从一组种子求稳定闭包；悬空端点会被忽略。
    [[nodiscard]] QList<TaskId> connectedTaskIds(
        const QList<TaskId> &seedTaskIds) const;

    /// 返回前置任务对 Finish-to-Start 边的结构化解析结果。
    [[nodiscard]] static TaskDependencyResolution dependencyResolution(
        const Task &task) noexcept;

    /// 仅判断前置是否以 Done 满足关系；判断阻塞必须改用 dependencyResolution。
    [[nodiscard]] static bool satisfiesDependency(const Task &task) noexcept;

private:
    [[nodiscard]] const Task *findTask(const TaskId &taskId) const;

    QList<Task> m_tasks;
    QList<TaskDependency> m_dependencies;
    /// 稳定 TaskId 到快照下标的索引，使长链校验保持 O(V+E) 查找复杂度。
    QHash<TaskId, qsizetype> m_taskIndexes;
};

} // namespace smartmate::model
