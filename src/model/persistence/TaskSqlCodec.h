#pragma once

#include "domain/TaskCategory.h"
#include "domain/TaskStateMachine.h"
#include "domain/TaskTypes.h"

#include <QString>

namespace smartmate::model::persistence::detail {

/// 将任务优先级转换为跨版本稳定的 SQLite 文本；非法领域值抛出 RepositoryException。
[[nodiscard]] QString taskPriorityToSqlText(TaskPriority priority);
/// 从 SQLite 稳定文本恢复任务优先级；未知文本抛出 RepositoryException。
[[nodiscard]] TaskPriority taskPriorityFromSqlText(const QString &text);

/// 将任务状态转换为跨版本稳定的 SQLite 文本；归档前状态复用同一编码。
[[nodiscard]] QString taskStatusToSqlText(TaskStatus status);
/// 从 SQLite 稳定文本恢复任务状态；未知文本抛出 RepositoryException。
[[nodiscard]] TaskStatus taskStatusFromSqlText(const QString &text);

/// 将类别颜色转换为跨版本稳定的 SQLite 文本。
[[nodiscard]] QString taskCategoryColorToSqlText(TaskCategoryColor color);
/// 从 SQLite 稳定文本恢复类别颜色；未知文本抛出 RepositoryException。
[[nodiscard]] TaskCategoryColor taskCategoryColorFromSqlText(const QString &text);

/// 将任务转换类型映射为事件表稳定文本。
[[nodiscard]] QString taskTransitionToSqlText(TaskTransition transition);
/// 从事件表稳定文本恢复任务转换类型。
[[nodiscard]] TaskTransition taskTransitionFromSqlText(const QString &text);

} // namespace smartmate::model::persistence::detail
