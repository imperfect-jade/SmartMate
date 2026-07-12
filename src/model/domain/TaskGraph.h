#pragma once

#include "domain/Task.h"
#include "domain/TaskDependency.h"

#include <QList>

namespace smartmate::model {

/// 依赖图中的领域节点；层级是最长前置链长度，不包含任何像素或颜色信息。
struct TaskGraphNode final {
    Task task;
    TaskDependencyState dependencyState;
    int dependencyLevel{0};
    /// 上下游闭包属于领域图语义，供不同 View 复用且不包含任何布局信息。
    QList<TaskId> predecessorClosureIds;
    QList<TaskId> successorClosureIds;

    friend bool operator==(const TaskGraphNode &, const TaskGraphNode &) = default;
};

/// 依赖图中的有向边；解析状态区分完成满足、待满足与取消失效。
struct TaskGraphEdge final {
    TaskDependency dependency;
    TaskDependencyResolution resolution{TaskDependencyResolution::Pending};

    friend bool operator==(const TaskGraphEdge &, const TaskGraphEdge &) = default;
};

/// 可供 ViewModel 投影的结构化图快照；不持久化布局、缩放或选中状态。
struct TaskGraphSnapshot final {
    QList<TaskGraphNode> nodes;
    QList<TaskGraphEdge> edges;

    friend bool operator==(const TaskGraphSnapshot &,
                           const TaskGraphSnapshot &) = default;
};

} // namespace smartmate::model
