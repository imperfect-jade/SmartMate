#include "services/StatisticsService.h"

#include "dependencies/TaskDependencyGraph.h"
#include "planner/TaskCommandPolicy.h"
#include "planner/TaskDeadlinePolicy.h"
#include "repositories/RepositoryException.h"

#include <QHash>
#include <QTime>

#include <algorithm>

namespace smartmate::model {
namespace {

struct RangeBoundaries final {
    QDate currentStartDate;
    QDate previousStartDate;
    QDateTime currentStartUtc;
    QDateTime previousStartUtc;
    QDateTime previousEndUtc;
    int bucketCount{0};
    int bucketDays{1};
};

QDateTime localMidnightUtc(const QDate &date, const QTimeZone &zone)
{
    return QDateTime{date, QTime{0, 0}, zone}.toUTC();
}

QDateTime sameLocalTimeUtc(const QDate &date,
                           const QTime &time,
                           const QTimeZone &zone)
{
    return QDateTime{date, time, zone}.toUTC();
}

QDate mondayOfWeek(const QDate &date)
{
    return date.addDays(1 - date.dayOfWeek());
}

RangeBoundaries rangeBoundaries(const StatisticsQuery &query,
                                const QDateTime &localNow)
{
    RangeBoundaries result;
    switch (query.range) {
    case StatisticsRange::Last7Days:
        result.bucketCount = 7;
        result.bucketDays = 1;
        result.currentStartDate = localNow.date().addDays(-6);
        result.previousStartDate = result.currentStartDate.addDays(-7);
        result.previousEndUtc = sameLocalTimeUtc(
            localNow.date().addDays(-7), localNow.time(), query.timeZone);
        break;
    case StatisticsRange::Last30Days:
        result.bucketCount = 30;
        result.bucketDays = 1;
        result.currentStartDate = localNow.date().addDays(-29);
        result.previousStartDate = result.currentStartDate.addDays(-30);
        result.previousEndUtc = sameLocalTimeUtc(
            localNow.date().addDays(-30), localNow.time(), query.timeZone);
        break;
    case StatisticsRange::Last12Weeks:
        result.bucketCount = 12;
        result.bucketDays = 7;
        result.currentStartDate = mondayOfWeek(localNow.date()).addDays(-77);
        result.previousStartDate = result.currentStartDate.addDays(-84);
        result.previousEndUtc = sameLocalTimeUtc(
            localNow.date().addDays(-84), localNow.time(), query.timeZone);
        break;
    }
    result.currentStartUtc = localMidnightUtc(result.currentStartDate, query.timeZone);
    result.previousStartUtc = localMidnightUtc(result.previousStartDate, query.timeZone);
    return result;
}

bool isCompletion(const TaskActivityEvent &event) noexcept
{
    return event.transition == TaskTransition::Complete;
}

int completionCount(const QList<TaskActivityEvent> &events,
                    const QDateTime &startInclusiveUtc,
                    const QDateTime &endExclusiveUtc)
{
    return static_cast<int>(std::count_if(
        events.cbegin(), events.cend(), [&](const TaskActivityEvent &event) {
            return isCompletion(event)
                && event.occurredAtUtc >= startInclusiveUtc
                && event.occurredAtUtc < endExclusiveUtc;
        }));
}

StatisticsComparison comparison(const int current, const int previous)
{
    const int delta = current - previous;
    return {current,
            previous,
            delta,
            delta > 0 ? StatisticsChangeSemantic::Positive
                      : (delta < 0 ? StatisticsChangeSemantic::Risk
                                   : StatisticsChangeSemantic::Neutral)};
}

QList<StatisticsTrendBucket> makeTrend(
    const QList<TaskActivityEvent> &events,
    const RangeBoundaries &boundaries,
    const QDate &currentLocalDate,
    const QTimeZone &zone)
{
    QList<StatisticsTrendBucket> buckets;
    buckets.reserve(boundaries.bucketCount);
    for (int index = 0; index < boundaries.bucketCount; ++index) {
        const QDate startDate = boundaries.currentStartDate.addDays(
            index * boundaries.bucketDays);
        const QDate endDate = startDate.addDays(boundaries.bucketDays - 1);
        const QDateTime startUtc = localMidnightUtc(startDate, zone);
        const QDateTime endUtc = localMidnightUtc(
            startDate.addDays(boundaries.bucketDays), zone);
        buckets.append({startDate,
                        endDate,
                        completionCount(events, startUtc, endUtc),
                        currentLocalDate >= startDate
                            && currentLocalDate <= endDate});
    }
    return buckets;
}

bool sameCategorySnapshot(const StatisticsCategoryBucket &bucket,
                          const TaskActivityEvent &event)
{
    if (!event.categoryIdSnapshot.has_value()) {
        return bucket.kind == StatisticsCategoryKind::Unclassified;
    }
    return bucket.kind == StatisticsCategoryKind::Categorized
        && bucket.categoryId == event.categoryIdSnapshot
        && bucket.categoryName == event.categoryNameSnapshot.value_or(QString{})
        && bucket.categoryColor == event.categoryColorSnapshot;
}

QList<StatisticsCategoryBucket> makeCategories(
    const QList<TaskActivityEvent> &events,
    const QDateTime &startUtc,
    const QDateTime &endUtc)
{
    QList<StatisticsCategoryBucket> buckets;
    for (const TaskActivityEvent &event : events) {
        if (!isCompletion(event)
            || event.occurredAtUtc < startUtc
            || event.occurredAtUtc >= endUtc) {
            continue;
        }
        auto existing = std::find_if(
            buckets.begin(), buckets.end(), [&](const StatisticsCategoryBucket &bucket) {
                return sameCategorySnapshot(bucket, event);
            });
        if (existing != buckets.end()) {
            ++existing->completionCount;
            continue;
        }
        if (event.categoryIdSnapshot.has_value()) {
            buckets.append({StatisticsCategoryKind::Categorized,
                            event.categoryIdSnapshot,
                            event.categoryNameSnapshot.value_or(QString{}),
                            event.categoryColorSnapshot,
                            1});
        } else {
            buckets.append({StatisticsCategoryKind::Unclassified,
                            std::nullopt,
                            {},
                            std::nullopt,
                            1});
        }
    }

    std::sort(buckets.begin(), buckets.end(), [](const auto &left, const auto &right) {
        if (left.completionCount != right.completionCount) {
            return left.completionCount > right.completionCount;
        }
        const int nameOrder = QString::localeAwareCompare(
            left.categoryName, right.categoryName);
        if (nameOrder != 0) {
            return nameOrder < 0;
        }
        return left.categoryId.value_or(TaskCategoryId{}).toString()
            < right.categoryId.value_or(TaskCategoryId{}).toString();
    });
    if (buckets.size() <= 5) {
        return buckets;
    }
    int otherCount = 0;
    while (buckets.size() > 5) {
        otherCount += buckets.takeLast().completionCount;
    }
    buckets.append({StatisticsCategoryKind::Other,
                    std::nullopt,
                    {},
                    std::nullopt,
                    otherCount});
    return buckets;
}

TaskHealthSnapshot makeHealth(const QList<Task> &tasks,
                              const QList<TaskDependency> &dependencies,
                              const QDateTime &nowUtc,
                              const QDateTime &dueSoonEndUtc)
{
    TaskHealthSnapshot health;
    const QHash<TaskId, TaskCommandAvailability> availabilities =
        taskCommandAvailabilities(tasks, dependencies);
    const TaskDependencyGraph graph{tasks, dependencies};
    for (const Task &task : tasks) {
        const bool active = task.status() == TaskStatus::Todo
            || task.status() == TaskStatus::InProgress;
        if (!active) {
            continue;
        }
        ++health.activeTaskCount;
        if (availabilities.value(task.id()).canStart) {
            ++health.executableCount;
        }
        if (task.status() == TaskStatus::Todo
            && graph.dependencyState(task.id()).blocked) {
            ++health.blockedCount;
        }
        if (TaskDeadlinePolicy::isOverdue(task, nowUtc)) {
            ++health.overdueCount;
            if (task.priority() == TaskPriority::Urgent) {
                ++health.urgentOverdueCount;
            }
        } else if (task.deadline().has_value()
                   && *task.deadline() >= nowUtc
                   && *task.deadline() < dueSoonEndUtc) {
            ++health.dueSoonCount;
        }
    }
    return health;
}

} // namespace

StatisticsService::StatisticsService(
    ITaskActivityRepository &activityRepository,
    ITaskRepository &taskRepository,
    ITaskDependencyRepository &dependencyRepository)
    : m_activityRepository(activityRepository)
    , m_taskRepository(taskRepository)
    , m_dependencyRepository(dependencyRepository)
{
}

StatisticsResult StatisticsService::snapshot(const StatisticsQuery &query) const
{
    if (!query.nowUtc.isValid() || !query.timeZone.isValid()) {
        return StatisticsResult::failure(
            StatisticsError::InvalidQuery,
            QStringLiteral("Statistics query requires a valid UTC time and time zone."));
    }

    try {
        const QDateTime nowUtc = query.nowUtc.toUTC();
        // SQLite 事件精度为毫秒；加一毫秒形成包含 nowUtc 的半开查询区间。
        const QDateTime endExclusiveUtc = nowUtc.addMSecs(1);
        const QDateTime localNow = nowUtc.toTimeZone(query.timeZone);
        const QDate today = localNow.date();
        const QDate weekStartDate = mondayOfWeek(today);
        const QDateTime todayStartUtc = localMidnightUtc(today, query.timeZone);
        const QDateTime yesterdayStartUtc = localMidnightUtc(
            today.addDays(-1), query.timeZone);
        const QDateTime yesterdaySameTimeUtc = sameLocalTimeUtc(
            today.addDays(-1), localNow.time(), query.timeZone).addMSecs(1);
        const QDateTime weekStartUtc = localMidnightUtc(
            weekStartDate, query.timeZone);
        const QDateTime previousWeekStartUtc = localMidnightUtc(
            weekStartDate.addDays(-7), query.timeZone);
        const QDateTime previousWeekSameTimeUtc = sameLocalTimeUtc(
            today.addDays(-7), localNow.time(), query.timeZone).addMSecs(1);
        const RangeBoundaries range = rangeBoundaries(query, localNow);
        const QDateTime earliestUtc = std::min(
            {yesterdayStartUtc, previousWeekStartUtc, range.previousStartUtc});

        const QList<TaskActivityEvent> events =
            m_activityRepository.findEventsByOccurredAt(earliestUtc,
                                                        endExclusiveUtc);
        const QList<Task> tasks = m_taskRepository.findAll();
        const QList<TaskDependency> dependencies =
            m_dependencyRepository.findAllDependencies();

        StatisticsSnapshot result;
        result.range = query.range;
        result.today = comparison(
            completionCount(events, todayStartUtc, endExclusiveUtc),
            completionCount(events, yesterdayStartUtc, yesterdaySameTimeUtc));
        result.thisWeek = comparison(
            completionCount(events, weekStartUtc, endExclusiveUtc),
            completionCount(events,
                            previousWeekStartUtc,
                            previousWeekSameTimeUtc));
        result.selectedPeriod = comparison(
            completionCount(events, range.currentStartUtc, endExclusiveUtc),
            completionCount(events,
                            range.previousStartUtc,
                            range.previousEndUtc.addMSecs(1)));

        for (const TaskActivityEvent &event : events) {
            if (!isCompletion(event)
                || event.occurredAtUtc < weekStartUtc
                || event.occurredAtUtc >= endExclusiveUtc
                || !event.deadlineSnapshotUtc.has_value()) {
                continue;
            }
            ++result.deadlineCompletionCount;
            if (event.occurredAtUtc <= *event.deadlineSnapshotUtc) {
                ++result.onTimeCompletionCount;
            }
        }

        result.trend = makeTrend(events, range, today, query.timeZone);
        result.categories = makeCategories(events,
                                           range.currentStartUtc,
                                           endExclusiveUtc);
        result.health = makeHealth(
            tasks,
            dependencies,
            nowUtc,
            localMidnightUtc(today.addDays(3), query.timeZone));
        result.hasCompletionHistory =
            m_activityRepository.findLatestCompletionBefore(endExclusiveUtc)
                .has_value();
        return StatisticsResult::success(std::move(result));
    } catch (const RepositoryException &exception) {
        return StatisticsResult::failure(
            StatisticsError::PersistenceFailure,
            QString::fromUtf8(exception.what()));
    } catch (...) {
        return StatisticsResult::failure(
            StatisticsError::PersistenceFailure,
            QStringLiteral("Unexpected statistics repository failure."));
    }
}

} // namespace smartmate::model
