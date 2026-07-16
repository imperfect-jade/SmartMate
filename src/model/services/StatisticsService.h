#pragma once

#include "repositories/ITaskActivityRepository.h"
#include "repositories/ITaskDependencyRepository.h"
#include "repositories/ITaskRepository.h"
#include "statistics/StatisticsTypes.h"

namespace smartmate::model {

/// 任务统计的唯一业务聚合入口；只读取原始事件、任务和依赖事实。
class StatisticsService final {
public:
    StatisticsService(ITaskActivityRepository &activityRepository,
                      ITaskRepository &taskRepository,
                      ITaskDependencyRepository &dependencyRepository);

    /// 使用显式 UTC 当前时间和时区生成稳定统计快照，不在公式内部读取系统时钟。
    [[nodiscard]] StatisticsResult snapshot(const StatisticsQuery &query) const;

private:
    ITaskActivityRepository &m_activityRepository;
    ITaskRepository &m_taskRepository;
    ITaskDependencyRepository &m_dependencyRepository;
};

} // namespace smartmate::model
