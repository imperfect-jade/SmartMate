#pragma once

#include "domain/TaskActivityEvent.h"

#include <QList>

#include <optional>

namespace smartmate::model {

/// 任务活动事件只读端口；返回普通领域事实，禁止返回 SQL 或统计汇总类型。
class ITaskActivityRepository {
public:
    virtual ~ITaskActivityRepository() = default;

    /// 查询 UTC 半开区间 [startInclusiveUtc, endExclusiveUtc) 内的事件，按发生时间和 ID 排序。
    [[nodiscard]] virtual QList<TaskActivityEvent> findEventsByOccurredAt(
        const QDateTime &startInclusiveUtc,
        const QDateTime &endExclusiveUtc) const = 0;

    /// 返回指定 UTC 时刻前最近一条完成事件；仅用于判断是否存在完成历史。
    [[nodiscard]] virtual std::optional<TaskActivityEvent> findLatestCompletionBefore(
        const QDateTime &endExclusiveUtc) const = 0;
};

} // namespace smartmate::model
