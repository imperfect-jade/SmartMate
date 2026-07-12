#pragma once

#include "domain/Task.h"
#include "repositories/RepositoryException.h"

#include <QList>

#include <optional>

namespace smartmate::model {

/// 单个批量状态写入项；expectedStatus 用于在事务中防御预检后的并发状态变化。
struct TaskStateChange final {
    TaskId taskId;
    TaskStatus expectedStatus{TaskStatus::Todo};
    TaskStatus targetStatus{TaskStatus::Todo};
    std::optional<TaskStatus> statusBeforeArchive;
    QDateTime updatedAtUtc;
};

/// 批量状态端口的原子写入结果；存在冲突时实现必须回滚并返回 updatedTaskCount=0。
struct TaskBatchWriteResult final {
    int updatedTaskCount{0};
    QList<TaskId> conflictingTaskIds;
};

/// 批量状态转换端口；全部条件更新必须位于同一事务中，禁止产生部分成功。
class ITaskBatchTransitionRepository {
public:
    virtual ~ITaskBatchTransitionRepository() = default;

    /// 按稳定 TaskId 顺序执行条件更新；持久化故障抛出 RepositoryException。
    [[nodiscard]] virtual TaskBatchWriteResult updateTaskStatesAtomically(
        const QList<TaskStateChange> &changes) = 0;
};

} // namespace smartmate::model
