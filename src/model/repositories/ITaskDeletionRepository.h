#pragma once

#include "domain/TaskTypes.h"
#include "repositories/RepositoryException.h"

#include <QList>

namespace smartmate::model {

/// 永久删除命令的原子写入结果；依赖数量用于决定是否发布依赖变更通知。
struct TaskDeletionWriteResult final {
    int deletedTaskCount{0};
    int removedDependencyCount{0};
    QList<TaskId> conflictingTaskIds;
};

/// 归档任务永久删除端口；实现必须在同一事务内清理任务及其全部依赖边。
class ITaskDeletionRepository {
public:
    virtual ~ITaskDeletionRepository() = default;

    /// 仅删除归档任务。目标缺失或已非归档时返回 deletedTaskCount=0 且不得保留部分修改；
    /// 持久化故障抛出 RepositoryException。
    [[nodiscard]] virtual TaskDeletionWriteResult
    deleteArchivedTasksWithDependencies(const QList<TaskId> &taskIds) = 0;
};

} // namespace smartmate::model
