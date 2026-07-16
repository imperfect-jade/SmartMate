#pragma once

#include "domain/TaskActivityEvent.h"

#include <QList>

#include <optional>

namespace smartmate::model {

/// 单个状态写入项；expectedStatus 用于在事务中防御 Service 预检后的并发变化。
struct TaskStateChange final {
    TaskId taskId;
    TaskStatus expectedStatus{TaskStatus::Todo};
    TaskStatus targetStatus{TaskStatus::Todo};
    std::optional<TaskStatus> statusBeforeArchive;
    QDateTime updatedAtUtc;

    friend bool operator==(const TaskStateChange &, const TaskStateChange &) = default;
};

/// 一个必须共同提交的任务状态变化与活动事件。
struct TaskTransitionWrite final {
    TaskStateChange stateChange;
    TaskActivityEvent event;

    friend bool operator==(const TaskTransitionWrite &,
                           const TaskTransitionWrite &) = default;
};

/// 原子转换端口结果；存在任一冲突时实现必须回滚并返回两个计数均为零。
struct TaskTransitionWriteResult final {
    int updatedTaskCount{0};
    int insertedEventCount{0};
    QList<TaskId> conflictingTaskIds;

    TaskTransitionWriteResult() = default;
    TaskTransitionWriteResult(int updatedCount, QList<TaskId> conflicts)
        : updatedTaskCount(updatedCount)
        , insertedEventCount(updatedCount)
        , conflictingTaskIds(std::move(conflicts))
    {
    }
    TaskTransitionWriteResult(int updatedCount,
                              int insertedCount,
                              QList<TaskId> conflicts)
        : updatedTaskCount(updatedCount)
        , insertedEventCount(insertedCount)
        , conflictingTaskIds(std::move(conflicts))
    {
    }
};

/// 任务状态与活动事件的原子写入端口，单项命令也必须使用单元素批次。
class ITaskTransitionRepository {
public:
    virtual ~ITaskTransitionRepository() = default;

    /// 按稳定 TaskId 顺序写入；任一状态冲突整批回滚，持久化故障抛出 RepositoryException。
    [[nodiscard]] virtual TaskTransitionWriteResult applyTransitionsAtomically(
        const QList<TaskTransitionWrite> &writes) = 0;
};

} // namespace smartmate::model
