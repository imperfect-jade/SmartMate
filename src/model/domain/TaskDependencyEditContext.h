#pragma once

#include "domain/Task.h"

#include <QHash>
#include <QList>

namespace smartmate::model {

/// 一个前置候选及其由 Model 判定的初始选择和可选择资格。
struct TaskDependencyCandidate final {
    /// 候选任务完整领域快照，供 ViewModel 生成展示信息。
    Task task;
    /// 打开编辑器时是否已属于当前前置集合。
    bool selected{false};
    /// 当前是否允许成为新前置；已有取消关系可保留并移除。
    bool selectable{false};

    friend bool operator==(const TaskDependencyCandidate &,
                           const TaskDependencyCandidate &) = default;
};

/// 打开依赖编辑器所需的原子业务快照；不包含任何界面文案或列表行号。
struct TaskDependencyEditContext final {
    /// 正在修改前置集合的 Todo 任务。
    Task targetTask;
    /// Model 已完成范围和资格判定的稳定候选列表。
    QList<TaskDependencyCandidate> candidates;
    /// 保留全图标题，确保循环路径经过隐藏任务时 ViewModel 仍可解释错误。
    QHash<TaskId, QString> taskTitles;

    friend bool operator==(const TaskDependencyEditContext &,
                           const TaskDependencyEditContext &) = default;
};

} // namespace smartmate::model
