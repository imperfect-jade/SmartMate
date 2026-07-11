#pragma once

#include "domain/Task.h"
#include "repositories/RepositoryException.h"

#include <QList>

namespace smartmate::model {

/// 原子新建端口；具体实现必须保证任务与全部前置边要么全部写入、要么全部回滚。
class ITaskCreationRepository {
public:
    virtual ~ITaskCreationRepository() = default;

    /// 持久化失败抛出 RepositoryException，禁止通过归档模拟事务回滚。
    virtual void insertTaskWithPredecessors(
        const Task &task,
        const QList<TaskId> &predecessorIds) = 0;
};

} // namespace smartmate::model
