#pragma once

#include <QUuid>

namespace smartmate::model {

/// 任务在所有层之间传递的稳定身份；界面行号不得替代它。
using TaskId = QUuid;

/// 显式数值是展示投影的稳定索引，禁止重排；持久化层应使用独立的稳定文本。
enum class TaskPriority : int {
    /// 低优先级。
    Low = 0,
    /// 默认普通优先级。
    Normal = 1,
    /// 高优先级。
    High = 2,
    /// 最高紧急优先级。
    Urgent = 3,
};

/// 显式数值是展示投影的稳定索引，新增状态时必须同步所有状态映射。
enum class TaskStatus : int {
    /// 尚未开始，可编辑普通字段和前置集合。
    Todo = 0,
    /// 正在执行；全局任意时刻最多一个。
    InProgress = 1,
    /// 已完成，可归档或重做。
    Done = 2,
    /// 已取消，可归档或重做；作为前置时暂不阻塞后继。
    Cancelled = 3,
    /// 已软归档，仅此状态允许永久删除。
    Archived = 4,
};

} // namespace smartmate::model
