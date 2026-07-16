#include "fakes/FakeTaskActivityRepository.h"
#include "fakes/FakeTaskDependencyRepository.h"
#include "fakes/FakeTaskRepository.h"

#include "services/StatisticsService.h"

#include <QTest>
#include <QTime>
#include <QTimeZone>

using namespace smartmate::model;
using namespace smartmate::tests;

namespace {

QDateTime localUtc(const QDate &date,
                   const QTime &time,
                   const QTimeZone &zone)
{
    return QDateTime{date, time, zone}.toUTC();
}

TaskActivityEvent completeEvent(
    const QDateTime &occurredAtUtc,
    std::optional<QDateTime> deadlineUtc = std::nullopt,
    std::optional<TaskCategoryId> categoryId = std::nullopt,
    std::optional<QString> categoryName = std::nullopt,
    std::optional<TaskCategoryColor> categoryColor = std::nullopt)
{
    return {QUuid::createUuid(),
            QUuid::createUuid(),
            TaskTransition::Complete,
            TaskStatus::InProgress,
            TaskStatus::Done,
            occurredAtUtc,
            deadlineUtc,
            30,
            TaskPriority::Normal,
            categoryId,
            std::move(categoryName),
            categoryColor};
}

Task task(TaskStatus status,
          TaskPriority priority = TaskPriority::Normal,
          std::optional<QDateTime> deadline = std::nullopt)
{
    const QDateTime created = QDateTime::fromMSecsSinceEpoch(
        1'700'000'000'000LL, QTimeZone::UTC);
    return {QUuid::createUuid(),
            QStringLiteral("统计任务"),
            {},
            priority,
            status,
            std::nullopt,
            deadline,
            30,
            created,
            created};
}

} // namespace

class StatisticsServiceTest final : public QObject {
    Q_OBJECT

private slots:
    void rejectsInvalidQuery();
    void aggregatesDailyComparisonsOnTimeAndCategories();
    void groupsCategorySnapshotsAndMergesAfterTopFive();
    void derivesHealthFromAuthoritativePolicies();
    void emitsStableBucketsAcrossRangesAndDst();
    void mapsRepositoryFailure();
};

void StatisticsServiceTest::rejectsInvalidQuery()
{
    FakeTaskActivityRepository activities;
    FakeTaskRepository tasks;
    FakeTaskDependencyRepository dependencies;
    StatisticsService service{activities, tasks, dependencies};

    const auto result = service.snapshot(
        {StatisticsRange::Last7Days, {}, QTimeZone{}});
    QVERIFY(!result.ok());
    QCOMPARE(result.error, StatisticsError::InvalidQuery);
}

void StatisticsServiceTest::aggregatesDailyComparisonsOnTimeAndCategories()
{
    const QTimeZone zone{"Asia/Shanghai"};
    const QDateTime now = localUtc({2026, 7, 16}, {12, 0}, zone);
    const TaskCategoryId studyId = QUuid::createUuid();
    const TaskCategoryId workId = QUuid::createUuid();
    QList<TaskActivityEvent> events{
        completeEvent(localUtc({2026, 7, 16}, {9, 0}, zone),
                      localUtc({2026, 7, 16}, {10, 0}, zone),
                      studyId, QStringLiteral("学习"), TaskCategoryColor::Blue),
        completeEvent(localUtc({2026, 7, 16}, {11, 0}, zone),
                      localUtc({2026, 7, 16}, {10, 30}, zone),
                      workId, QStringLiteral("工作"), TaskCategoryColor::Teal),
        completeEvent(localUtc({2026, 7, 15}, {9, 0}, zone)),
        completeEvent(localUtc({2026, 7, 13}, {18, 0}, zone)),
        completeEvent(localUtc({2026, 7, 6}, {18, 0}, zone)),
    };
    FakeTaskActivityRepository activities{events};
    FakeTaskRepository tasks;
    FakeTaskDependencyRepository dependencies;
    StatisticsService service{activities, tasks, dependencies};

    const auto result = service.snapshot(
        {StatisticsRange::Last7Days, now, zone});
    QVERIFY(result.ok());
    const StatisticsSnapshot &snapshot = *result.value;
    QCOMPARE(snapshot.today.currentCount, 2);
    QCOMPARE(snapshot.today.previousCount, 1);
    QCOMPARE(snapshot.today.semantic, StatisticsChangeSemantic::Positive);
    QCOMPARE(snapshot.thisWeek.currentCount, 4);
    QCOMPARE(snapshot.thisWeek.previousCount, 1);
    QCOMPARE(snapshot.onTimeCompletionCount, 1);
    QCOMPARE(snapshot.deadlineCompletionCount, 2);
    QCOMPARE(snapshot.onTimeRate(), std::optional<double>{0.5});
    QCOMPARE(snapshot.trend.size(), 7);
    QCOMPARE(snapshot.selectedPeriod.currentCount, 4);
    QCOMPARE(snapshot.categories.size(), 3);
    QVERIFY(snapshot.hasCompletionHistory);
}

void StatisticsServiceTest::groupsCategorySnapshotsAndMergesAfterTopFive()
{
    const QTimeZone zone{"Asia/Shanghai"};
    const QDateTime now = localUtc({2026, 7, 16}, {12, 0}, zone);
    QList<TaskActivityEvent> events;
    for (int index = 0; index < 6; ++index) {
        const TaskCategoryId id = QUuid::createUuid();
        events.append(completeEvent(
            localUtc({2026, 7, 16}, {index + 1, 0}, zone),
            std::nullopt,
            id,
            QStringLiteral("类别%1").arg(index),
            static_cast<TaskCategoryColor>(index)));
    }
    const TaskCategoryId renamedId = QUuid::createUuid();
    events.append(completeEvent(localUtc({2026, 7, 15}, {9, 0}, zone),
                                std::nullopt, renamedId,
                                QStringLiteral("旧名称"), TaskCategoryColor::Blue));
    events.append(completeEvent(localUtc({2026, 7, 15}, {10, 0}, zone),
                                std::nullopt, renamedId,
                                QStringLiteral("新名称"), TaskCategoryColor::Green));
    FakeTaskActivityRepository activities{events};
    FakeTaskRepository tasks;
    FakeTaskDependencyRepository dependencies;
    StatisticsService service{activities, tasks, dependencies};

    const auto result = service.snapshot(
        {StatisticsRange::Last7Days, now, zone});
    QVERIFY(result.ok());
    QCOMPARE(result.value->categories.size(), 6);
    QCOMPARE(result.value->categories.last().kind, StatisticsCategoryKind::Other);
    QCOMPARE(result.value->categories.last().completionCount, 3);
}

void StatisticsServiceTest::derivesHealthFromAuthoritativePolicies()
{
    const QTimeZone zone{"Asia/Shanghai"};
    const QDateTime now = localUtc({2026, 7, 16}, {12, 0}, zone);
    const Task ready = task(TaskStatus::Todo);
    const Task blocked = task(TaskStatus::Todo);
    const Task predecessor = task(TaskStatus::Todo);
    const Task overdue = task(TaskStatus::Todo, TaskPriority::Urgent,
                              now.addSecs(-1));
    const Task dueSoon = task(TaskStatus::Todo, TaskPriority::Normal,
                              localUtc({2026, 7, 18}, {23, 59}, zone));
    FakeTaskRepository tasks{{ready, blocked, predecessor, overdue, dueSoon}};
    FakeTaskDependencyRepository dependencies{{{predecessor.id(), blocked.id()}}};
    FakeTaskActivityRepository activities;
    StatisticsService service{activities, tasks, dependencies};

    const auto result = service.snapshot(
        {StatisticsRange::Last7Days, now, zone});
    QVERIFY(result.ok());
    QCOMPARE(result.value->health.activeTaskCount, 5);
    QCOMPARE(result.value->health.blockedCount, 1);
    QCOMPARE(result.value->health.executableCount, 4);
    QCOMPARE(result.value->health.overdueCount, 1);
    QCOMPARE(result.value->health.urgentOverdueCount, 1);
    QCOMPARE(result.value->health.dueSoonCount, 1);
}

void StatisticsServiceTest::emitsStableBucketsAcrossRangesAndDst()
{
    const QTimeZone zone{"America/New_York"};
    const QDateTime now = localUtc({2026, 3, 15}, {12, 0}, zone);
    FakeTaskActivityRepository activities;
    FakeTaskRepository tasks;
    FakeTaskDependencyRepository dependencies;
    StatisticsService service{activities, tasks, dependencies};

    const auto seven = service.snapshot({StatisticsRange::Last7Days, now, zone});
    const auto thirty = service.snapshot({StatisticsRange::Last30Days, now, zone});
    const auto weeks = service.snapshot({StatisticsRange::Last12Weeks, now, zone});
    QVERIFY(seven.ok());
    QVERIFY(thirty.ok());
    QVERIFY(weeks.ok());
    QCOMPARE(seven.value->trend.size(), 7);
    QCOMPARE(thirty.value->trend.size(), 30);
    QCOMPARE(weeks.value->trend.size(), 12);
    for (const auto &bucket : weeks.value->trend) {
        QCOMPARE(bucket.localStartDate.dayOfWeek(), 1);
        QCOMPARE(bucket.localStartDate.daysTo(bucket.localEndDate), 6);
        QCOMPARE(bucket.completionCount, 0);
    }
    QVERIFY(!weeks.value->hasCompletionHistory);
}

void StatisticsServiceTest::mapsRepositoryFailure()
{
    const QTimeZone zone{"Asia/Shanghai"};
    FakeTaskActivityRepository activities;
    activities.setReadFailure(true);
    FakeTaskRepository tasks;
    FakeTaskDependencyRepository dependencies;
    StatisticsService service{activities, tasks, dependencies};

    const auto result = service.snapshot(
        {StatisticsRange::Last7Days,
         localUtc({2026, 7, 16}, {12, 0}, zone),
         zone});
    QVERIFY(!result.ok());
    QCOMPARE(result.error, StatisticsError::PersistenceFailure);
}

QTEST_APPLESS_MAIN(StatisticsServiceTest)
#include "tst_StatisticsService.moc"
