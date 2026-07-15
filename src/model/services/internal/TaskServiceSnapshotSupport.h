#pragma once

#include "dependencies/TaskDependencyGraph.h"
#include "services/TaskService.h"

#include <QHash>
#include <QSet>

#include <optional>

namespace smartmate::model::task_service_detail {

/// 返回无花括号稳定文本，仅用于确定性排序和错误上下文。
[[nodiscard]] QString stableId(const TaskId &taskId);
/// 将 TaskId 列表排序并去重，使批量结果与 Repository 返回顺序无关。
void normalizeIds(QList<TaskId> &taskIds);

[[nodiscard]] QList<TaskId> otherInProgressTaskIds(
    const QList<Task> &tasks,
    const std::optional<TaskId> &excludedId);

/// 返回值指针只在输入快照未修改且仍存活期间有效。
[[nodiscard]] const Task *findTaskInList(const QList<Task> &tasks,
                                         const TaskId &taskId);
[[nodiscard]] QHash<TaskId, qsizetype> taskIndexesById(
    const QList<Task> &tasks);
[[nodiscard]] TaskResult singleTaskResult(TaskBatchResult batchResult,
                                          const TaskId &taskId);
void replaceTaskSnapshot(QList<Task> &tasks, const Task &replacement);

struct ProtectedDependencyViolation final {
    TaskId successorId;
    QList<TaskId> blockingTaskIds;
};

/// 验证当前或归档前为 InProgress/Done 的任务仍满足全部前置。
[[nodiscard]] QList<ProtectedDependencyViolation> protectedStateViolations(
    const QList<Task> &tasks,
    const TaskDependencyGraph &graph);
[[nodiscard]] TaskErrorContext stateViolationContext(
    const QList<ProtectedDependencyViolation> &violations);
[[nodiscard]] std::optional<TaskResult> dependencyStateFailure(
    const QList<Task> &tasks,
    const QList<TaskDependency> &dependencies,
    const TaskId &changedTaskId);
[[nodiscard]] std::optional<TaskBatchResult> batchDependencyStateFailure(
    const QList<Task> &tasks,
    const QList<TaskDependency> &dependencies,
    const QSet<TaskId> &changedTaskIds);

[[nodiscard]] QList<TaskDependency> dependenciesForSuccessor(
    const QList<TaskDependency> &dependencies,
    const TaskId &successorId);

/// 以下构造函数只生成待验证的内存快照，不访问 Repository。
[[nodiscard]] Task makeTaskWithDetails(const Task &source,
                                       const TaskDraft &draft,
                                       QDateTime updatedAtUtc);
[[nodiscard]] Task makeTaskWithStatus(
    const Task &source,
    TaskStatus status,
    std::optional<TaskStatus> statusBeforeArchive,
    QDateTime updatedAtUtc);
[[nodiscard]] Task makeNewTask(const TaskId &taskId,
                               const TaskDraft &draft,
                               const QDateTime &nowUtc);

} // namespace smartmate::model::task_service_detail
