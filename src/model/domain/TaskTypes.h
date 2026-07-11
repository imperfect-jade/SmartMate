#pragma once

#include <QUuid>

namespace smartmate::model {

/// 任务在所有层之间传递的稳定身份；界面行号不得替代它。
using TaskId = QUuid;

/// 显式数值是展示投影的稳定索引，禁止重排；持久化层应使用独立的稳定文本。
enum class TaskPriority : int {
    Low = 0,
    Normal = 1,
    High = 2,
    Urgent = 3,
};

/// 显式数值是展示投影的稳定索引，新增状态时必须同步所有状态映射。
enum class TaskStatus : int {
    Todo = 0,
    InProgress = 1,
    Done = 2,
    Cancelled = 3,
    Archived = 4,
};

} // namespace smartmate::model
