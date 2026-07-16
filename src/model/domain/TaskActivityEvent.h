#pragma once

#include "domain/TaskCategory.h"
#include "domain/TaskStateMachine.h"
#include "domain/TaskTypes.h"

#include <QDateTime>
#include <QString>
#include <QUuid>

#include <optional>

namespace smartmate::model {

/// 不可变任务活动事件的稳定身份。
using TaskActivityEventId = QUuid;

/// 一次已提交任务状态转换的历史事实；所有时间使用 UTC。
///
/// 截止时间、预计用时、优先级和类别均保存转换发生时快照，后续编辑或类别变更
/// 不得反向改写历史统计。未分类事件的三个类别快照字段必须同时为空。
struct TaskActivityEvent final {
    TaskActivityEventId eventId;
    TaskId taskId;
    TaskTransition transition{TaskTransition::Start};
    TaskStatus fromStatus{TaskStatus::Todo};
    TaskStatus toStatus{TaskStatus::Todo};
    QDateTime occurredAtUtc;
    std::optional<QDateTime> deadlineSnapshotUtc;
    std::optional<int> estimatedMinutesSnapshot;
    TaskPriority prioritySnapshot{TaskPriority::Normal};
    std::optional<TaskCategoryId> categoryIdSnapshot;
    std::optional<QString> categoryNameSnapshot;
    std::optional<TaskCategoryColor> categoryColorSnapshot;

    friend bool operator==(const TaskActivityEvent &,
                           const TaskActivityEvent &) = default;
};

} // namespace smartmate::model
