#pragma once

#include "domain/TaskTypes.h"

#include <QList>

namespace smartmate::model {

/// Finish-to-Start 关系；predecessor 完成后 successor 才能开始。
struct TaskDependency final {
    /// 必须先解析的前置任务稳定身份。
    TaskId predecessorId;
    /// 受该关系约束的后继任务稳定身份。
    TaskId successorId;

    friend bool operator==(const TaskDependency &, const TaskDependency &) = default;
};

/// 前置关系的派生解析结果；Cancelled 表示关系保留但暂时不阻塞后继。
enum class TaskDependencyResolution {
    /// 前置仍未完成，关系当前阻塞后继。
    Pending,
    /// 前置当前已完成，关系处于满足状态。
    Satisfied,
    /// 前置已取消，关系保留但当前暂时失效。
    Cancelled,
};

/// 由任务快照和依赖关系即时计算的状态，不得作为冗余字段写入数据库。
struct TaskDependencyState final {
    /// 使用直接前置任务，始终按稳定 TaskId 排序。
    QList<TaskId> predecessorIds;
    /// 当前仍会阻塞 Finish-to-Start 的直接前置；已取消关系不属于阻塞项。
    QList<TaskId> unsatisfiedPredecessorIds;
    /// 关系仍存在但因前置已取消而暂时失效的直接前置。
    QList<TaskId> cancelledPredecessorIds;
    /// 若本任务变为 Done 或 Cancelled，将被直接解锁的 Todo 后继数量。
    int unlockCount{0};
    /// 仅 Todo/InProgress 会被标为阻塞；终态仍保留上述诊断列表。
    bool blocked{false};

    friend bool operator==(const TaskDependencyState &,
                           const TaskDependencyState &) = default;
};

} // namespace smartmate::model
