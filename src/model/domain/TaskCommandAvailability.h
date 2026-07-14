#pragma once

namespace smartmate::model {

/// 当前完整任务与依赖快照下的命令资格；ViewModel 只能投影，不能自行推导。
struct TaskCommandAvailability final {
    /// 是否允许修改标题、描述、优先级、截止时间、预计用时和类别。
    bool canEditTask{false};
    /// 是否允许原子替换当前任务的前置集合。
    bool canEditDependencies{false};
    /// 是否允许由 Todo 进入 InProgress。
    bool canStart{false};
    /// 是否允许由 Todo/InProgress 进入 Cancelled。
    bool canCancel{false};
    /// 是否允许由 InProgress 进入 Done。
    bool canComplete{false};
    /// 是否允许由 Done/Cancelled 重新进入 Todo。
    bool canRedo{false};
    /// 是否允许把 Done/Cancelled 软归档。
    bool canArchive{false};
    /// 是否允许恢复 Archived 任务且不破坏依赖不变量。
    bool canRestore{false};
    /// 永久删除仅允许归档任务，由领域实体统一判定。
    bool canDeletePermanently{false};

    friend bool operator==(const TaskCommandAvailability &,
                           const TaskCommandAvailability &) = default;
};

} // namespace smartmate::model
