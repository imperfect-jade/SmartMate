#pragma once

#include "domain/Task.h"
#include "domain/TaskCommandAvailability.h"
#include "domain/TaskDependency.h"

#include <QList>

#include <optional>

namespace smartmate::model {

/// 依赖图的类别范围属于 Model 查询语义，不包含任何像素或颜色信息。
enum class TaskGraphCategoryScope : int {
    /// 展示全部可见任务及其连接分量。
    All = 0,
    /// 以未分类任务为核心，并补充一跳跨类别上下文。
    Uncategorized = 1,
    /// 以指定类别任务为核心，并补充一跳跨类别上下文。
    SpecificCategory = 2,
};

/// 分类图查询；SpecificCategory 必须携带一个仍然存在的稳定类别 ID。
struct TaskGraphQuery final {
    /// 本次图快照的类别裁剪方式。
    TaskGraphCategoryScope scope{TaskGraphCategoryScope::All};
    /// 仅 SpecificCategory 使用；其他范围应为空。
    std::optional<TaskCategoryId> categoryId;

    friend bool operator==(const TaskGraphQuery &, const TaskGraphQuery &) = default;
};

/// 依赖图中的领域节点；层级是最长前置链长度，不包含任何像素或颜色信息。
struct TaskGraphNode final {
    /// 节点对应的完整任务领域快照。
    Task task;
    /// 基于全图即时计算的阻塞、前置和解锁信息。
    TaskDependencyState dependencyState;
    /// 基于完整任务与依赖快照计算的命令资格。
    TaskCommandAvailability availability;
    /// 当前裁剪图中的最长前置链层级，根节点为 0。
    int dependencyLevel{0};
    /// 全部传递前置身份，属于领域图语义且不包含布局信息。
    QList<TaskId> predecessorClosureIds;
    /// 全部传递后继身份，属于领域图语义且不包含布局信息。
    QList<TaskId> successorClosureIds;
    /// 分类查询中的核心节点为真；直接跨类别上下文节点为假。
    bool coreNode{true};

    friend bool operator==(const TaskGraphNode &, const TaskGraphNode &) = default;
};

/// 依赖图中的有向边；解析状态区分完成满足、待满足与取消失效。
struct TaskGraphEdge final {
    /// 边的稳定前置与后继身份。
    TaskDependency dependency;
    /// 前置当前对该边的满足状态。
    TaskDependencyResolution resolution{TaskDependencyResolution::Pending};

    friend bool operator==(const TaskGraphEdge &, const TaskGraphEdge &) = default;
};

/// 可供 ViewModel 投影的结构化图快照；不持久化布局、缩放或选中状态。
struct TaskGraphSnapshot final {
    /// 按 Model 推荐顺序组织的结构化节点，不包含像素坐标。
    QList<TaskGraphNode> nodes;
    /// 按稳定端点排序的结构化边，不包含路径或箭头几何。
    QList<TaskGraphEdge> edges;

    friend bool operator==(const TaskGraphSnapshot &,
                           const TaskGraphSnapshot &) = default;
};

} // namespace smartmate::model
