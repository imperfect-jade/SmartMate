#include "AppViewModel.h"
#include "StatisticsViewModel.h"
#include "fakes/FakeTaskActivityRepository.h"
#include "fakes/FakeTaskBatchTransitionRepository.h"
#include "fakes/FakeTaskCategoryRepository.h"
#include "fakes/FakeTaskCreationRepository.h"
#include "fakes/FakeTaskDeletionRepository.h"
#include "fakes/FakeTaskDependencyRepository.h"
#include "fakes/FakeTaskRepository.h"
#include "model/services/StatisticsService.h"
#include "model/services/TaskService.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTest>
#include <QTimeZone>
#include <QTimer>

using namespace smartmate;

namespace {

QDateTime localUtc(const QDate &date, const QTime &time,
                   const QTimeZone &zone)
{
    return QDateTime{date, time, zone}.toUTC();
}

model::TaskActivityEvent completeEvent(
    const QDateTime &occurredAtUtc,
    std::optional<QDateTime> deadlineUtc = std::nullopt,
    std::optional<model::TaskCategoryId> categoryId = std::nullopt,
    std::optional<QString> categoryName = std::nullopt,
    std::optional<model::TaskCategoryColor> categoryColor = std::nullopt)
{
    return {QUuid::createUuid(),
            QUuid::createUuid(),
            model::TaskTransition::Complete,
            model::TaskStatus::InProgress,
            model::TaskStatus::Done,
            occurredAtUtc,
            deadlineUtc,
            30,
            model::TaskPriority::Normal,
            categoryId,
            std::move(categoryName),
            categoryColor};
}

model::Task task(model::TaskStatus status,
                 model::TaskPriority priority = model::TaskPriority::Normal,
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

struct Fixture final {
    Fixture(QList<model::TaskActivityEvent> events = {},
            QList<model::Task> initialTasks = {},
            QList<model::TaskDependency> initialDependencies = {})
        : activities(std::move(events))
        , tasks(std::move(initialTasks))
        , dependencies(std::move(initialDependencies))
        , creation(tasks, dependencies)
        , transitions(tasks)
        , deletion(tasks, dependencies)
        , taskService(tasks, dependencies, creation, transitions, deletion,
                      categories)
        , statisticsService(activities, tasks, dependencies)
    {
    }

    tests::FakeTaskActivityRepository activities;
    tests::FakeTaskRepository tasks;
    tests::FakeTaskDependencyRepository dependencies;
    tests::FakeTaskCreationRepository creation;
    tests::FakeTaskBatchTransitionRepository transitions;
    tests::FakeTaskDeletionRepository deletion;
    tests::FakeTaskCategoryRepository categories;
    model::TaskService taskService;
    model::StatisticsService statisticsService;
};

struct RichFixture final {
    QTimeZone zone{"Asia/Shanghai"};
    QDateTime now{localUtc({2026, 7, 16}, {12, 0}, zone)};
    model::TaskCategoryId studyId{QUuid::createUuid()};
    model::Task ready{task(model::TaskStatus::Todo)};
    model::Task predecessor{task(model::TaskStatus::Todo)};
    model::Task blocked{task(model::TaskStatus::Todo)};
    model::Task dueSoon{task(model::TaskStatus::Todo,
                             model::TaskPriority::Normal,
                             localUtc({2026, 7, 18}, {23, 59}, zone))};
    model::Task overdue{task(model::TaskStatus::Todo,
                            model::TaskPriority::Urgent,
                            now.addSecs(-1))};
    Fixture fixture{
        {completeEvent(localUtc({2026, 7, 16}, {9, 0}, zone),
                       localUtc({2026, 7, 16}, {10, 0}, zone),
                       studyId, QStringLiteral("学习"),
                       model::TaskCategoryColor::Blue),
         completeEvent(localUtc({2026, 7, 16}, {11, 0}, zone),
                       localUtc({2026, 7, 16}, {10, 30}, zone)),
         completeEvent(localUtc({2026, 7, 15}, {9, 0}, zone),
                       std::nullopt, studyId, QStringLiteral("学习"),
                       model::TaskCategoryColor::Blue)},
        {ready, predecessor, blocked, dueSoon, overdue},
        {{predecessor.id(), blocked.id()}}};
};

} // namespace

class StatisticsViewModelTest final : public QObject {
    Q_OBJECT

private slots:
    void projectsCardsAndStableChildModels();
    void switchesRangesWithoutRedundantNotifications();
    void coalescesTaskAndDependencyInvalidations();
    void keepsLastProjectionWhenQueriesFail();
    void distinguishesNoHistoryFromAnEmptyPeriod();
    void ownsAndReschedulesSingleShotMidnightTimer();
    void appViewModelOwnsStatisticsOnlyWhenInjected();
};

void StatisticsViewModelTest::projectsCardsAndStableChildModels()
{
    RichFixture data;
    viewmodel::StatisticsViewModel viewModel{
        data.fixture.statisticsService,
        data.fixture.taskService,
        [&data] { return data.now; },
        [&data] { return data.zone; }};

    QCOMPARE(viewModel.range(), viewmodel::StatisticsContract::Last7Days);
    QCOMPARE(viewModel.todayCount(), 2);
    QCOMPARE(viewModel.todayComparisonText(),
             QStringLiteral("较昨日同期增加 1 次"));
    QCOMPARE(viewModel.todaySemantic(), viewmodel::StatisticsContract::Positive);
    QCOMPARE(viewModel.weekCount(), 3);
    QVERIFY(viewModel.hasOnTimeRate());
    QCOMPARE(viewModel.onTimeRate(), 0.5);
    QCOMPARE(viewModel.onTimeRateText(), QStringLiteral("50%"));
    QCOMPARE(viewModel.onTimeDetailText(), QStringLiteral("1 / 2 次按时完成"));
    QCOMPARE(viewModel.overdueCount(), 1);
    QCOMPARE(viewModel.urgentOverdueCount(), 1);
    QCOMPARE(viewModel.overdueDetailText(), QStringLiteral("其中 1 项为紧急任务"));
    QCOMPARE(viewModel.activeTaskCount(), 5);
    QVERIFY(viewModel.periodSummary().contains(QStringLiteral("最近 7 天完成 3 次")));
    QVERIFY(viewModel.emptyStateText().isEmpty());

    QAbstractItemModel *trend = viewModel.trend();
    QAbstractItemModel *categories = viewModel.categories();
    QAbstractItemModel *health = viewModel.health();
    QAbstractItemModelTester trendTester{
        trend, QAbstractItemModelTester::FailureReportingMode::QtTest};
    QAbstractItemModelTester categoryTester{
        categories, QAbstractItemModelTester::FailureReportingMode::QtTest};
    QAbstractItemModelTester healthTester{
        health, QAbstractItemModelTester::FailureReportingMode::QtTest};

    QCOMPARE(trend->rowCount(), 7);
    const QModelIndex lastTrend = trend->index(6, 0);
    QCOMPARE(lastTrend.data(viewmodel::StatisticsTrendContract::LabelRole)
                 .toString(),
             QStringLiteral("7/16"));
    QVERIFY(lastTrend.data(viewmodel::StatisticsTrendContract::CurrentRole)
                .toBool());
    QVERIFY(lastTrend.data(viewmodel::StatisticsTrendContract::TooltipRole)
                .toString().contains(QStringLiteral("完成 2 次")));

    QCOMPARE(categories->rowCount(), 2);
    QCOMPARE(categories->index(0, 0)
                 .data(viewmodel::StatisticsCategoryContract::LabelRole)
                 .toString(),
             QStringLiteral("学习"));
    QCOMPARE(categories->index(1, 0)
                 .data(viewmodel::StatisticsCategoryContract::ColorRole)
                 .toInt(),
             static_cast<int>(viewmodel::StatisticsCategoryContract::Unclassified));

    QCOMPARE(health->rowCount(), 4);
    QCOMPARE(health->index(0, 0)
                 .data(viewmodel::StatisticsHealthContract::TypeRole)
                 .toInt(),
             static_cast<int>(viewmodel::StatisticsHealthContract::Executable));
    QCOMPARE(health->index(1, 0)
                 .data(viewmodel::StatisticsHealthContract::TypeRole)
                 .toInt(),
             static_cast<int>(viewmodel::StatisticsHealthContract::Blocked));
    QCOMPARE(health->index(3, 0)
                 .data(viewmodel::StatisticsHealthContract::ValueRole)
                 .toInt(),
             1);
    const QList<QString> descriptions{
        QStringLiteral("现在可以开始的待办任务"),
        QStringLiteral("仍需等待前置任务完成或取消"),
        QStringLiteral("将在今天至后天内到期"),
        QStringLiteral("已超过截止时间"),
    };
    const QList<QString> accessibleTexts{
        QStringLiteral("可执行 4 项。现在可以开始的待办任务。"),
        QStringLiteral("被阻塞 1 项。仍需等待前置任务完成或取消。"),
        QStringLiteral("即将到期 1 项。将在今天至后天内到期。"),
        QStringLiteral("已经逾期 1 项。已超过截止时间。"),
    };
    for (int row = 0; row < health->rowCount(); ++row) {
        const QModelIndex index = health->index(row, 0);
        QCOMPARE(index.data(viewmodel::StatisticsHealthContract::MaximumRole).toInt(), 5);
        QCOMPARE(index.data(viewmodel::StatisticsHealthContract::DescriptionRole).toString(),
                 descriptions.at(row));
        QCOMPARE(index.data(viewmodel::StatisticsHealthContract::AccessibleTextRole).toString(),
                 accessibleTexts.at(row));
    }
}

void StatisticsViewModelTest::switchesRangesWithoutRedundantNotifications()
{
    RichFixture data;
    viewmodel::StatisticsViewModel viewModel{
        data.fixture.statisticsService, data.fixture.taskService,
        [&data] { return data.now; }, [&data] { return data.zone; }};
    QSignalSpy rangeSpy{&viewModel,
                        &viewmodel::StatisticsContract::rangeChanged};
    QSignalSpy statisticsSpy{&viewModel,
                             &viewmodel::StatisticsContract::statisticsChanged};
    const int initialQueries = data.fixture.activities.rangeQueryCount();

    QVERIFY(viewModel.setRange(viewmodel::StatisticsContract::Last7Days));
    QCOMPARE(data.fixture.activities.rangeQueryCount(), initialQueries);
    QCOMPARE(rangeSpy.count(), 0);
    QCOMPARE(statisticsSpy.count(), 0);

    QVERIFY(viewModel.setRange(viewmodel::StatisticsContract::Last30Days));
    QCOMPARE(viewModel.range(), viewmodel::StatisticsContract::Last30Days);
    QCOMPARE(viewModel.trend()->rowCount(), 30);
    QCOMPARE(rangeSpy.count(), 1);
    QCOMPARE(statisticsSpy.count(), 1);

    QVERIFY(viewModel.setRange(viewmodel::StatisticsContract::Last12Weeks));
    QCOMPARE(viewModel.range(), viewmodel::StatisticsContract::Last12Weeks);
    QCOMPARE(viewModel.trend()->rowCount(), 12);
    QCOMPARE(rangeSpy.count(), 2);
    QCOMPARE(statisticsSpy.count(), 2);
    QVERIFY(viewModel.trend()->index(0, 0)
                .data(viewmodel::StatisticsTrendContract::TooltipRole)
                .toString().contains(QChar{0x2014}));
}

void StatisticsViewModelTest::coalescesTaskAndDependencyInvalidations()
{
    RichFixture data;
    viewmodel::StatisticsViewModel viewModel{
        data.fixture.statisticsService, data.fixture.taskService,
        [&data] { return data.now; }, [&data] { return data.zone; }};
    const int initialQueries = data.fixture.activities.rangeQueryCount();

    QVERIFY(QMetaObject::invokeMethod(&data.fixture.taskService,
                                      "tasksChanged",
                                      Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(&data.fixture.taskService,
                                      "dependenciesChanged",
                                      Qt::DirectConnection));
    QTRY_COMPARE(data.fixture.activities.rangeQueryCount(), initialQueries + 1);
    QTest::qWait(1);
    QCOMPARE(data.fixture.activities.rangeQueryCount(), initialQueries + 1);
}

void StatisticsViewModelTest::keepsLastProjectionWhenQueriesFail()
{
    RichFixture data;
    viewmodel::StatisticsViewModel viewModel{
        data.fixture.statisticsService, data.fixture.taskService,
        [&data] { return data.now; }, [&data] { return data.zone; }};
    QSignalSpy notificationSpy{
        &viewModel, &viewmodel::StatisticsContract::notificationRaised};
    QSignalSpy statisticsSpy{&viewModel,
                             &viewmodel::StatisticsContract::statisticsChanged};
    const int oldToday = viewModel.todayCount();
    const int oldRows = viewModel.trend()->rowCount();
    data.fixture.activities.setReadFailure(true);

    QVERIFY(!viewModel.setRange(viewmodel::StatisticsContract::Last30Days));
    QCOMPARE(viewModel.range(), viewmodel::StatisticsContract::Last7Days);
    QCOMPARE(viewModel.todayCount(), oldToday);
    QCOMPARE(viewModel.trend()->rowCount(), oldRows);
    viewModel.reload();
    viewModel.reload();
    QCOMPARE(notificationSpy.count(), 3);
    QCOMPARE(statisticsSpy.count(), 0);
    const auto notification = qvariant_cast<common::UiNotification>(
        notificationSpy.first().constFirst());
    QCOMPARE(notification.severity, common::UiSeverity::Error);
    QCOMPARE(notification.title, QStringLiteral("统计加载失败"));
}

void StatisticsViewModelTest::distinguishesNoHistoryFromAnEmptyPeriod()
{
    const QTimeZone zone{"Asia/Shanghai"};
    QDateTime now = localUtc({2026, 7, 16}, {12, 0}, zone);
    Fixture fixture;
    viewmodel::StatisticsViewModel viewModel{
        fixture.statisticsService, fixture.taskService,
        [&now] { return now; }, [&zone] { return zone; }};

    QVERIFY(!viewModel.hasCompletionHistory());
    QCOMPARE(viewModel.emptyStateText(),
             QStringLiteral("完成新任务后开始生成趋势"));
    QVERIFY(!viewModel.hasOnTimeRate());
    QCOMPARE(viewModel.onTimeRateText(), QStringLiteral("—"));
    QCOMPARE(viewModel.onTimeDetailText(), QStringLiteral("暂无截止任务"));

    fixture.activities.setEvents(
        {completeEvent(localUtc({2026, 5, 1}, {9, 0}, zone))});
    viewModel.reload();
    QVERIFY(viewModel.hasCompletionHistory());
    QCOMPARE(viewModel.emptyStateText(), QStringLiteral("该周期暂无完成记录"));
    QCOMPARE(viewModel.todayCount(), 0);
}

void StatisticsViewModelTest::ownsAndReschedulesSingleShotMidnightTimer()
{
    const QTimeZone zone{"Asia/Shanghai"};
    QDateTime now = localUtc({2026, 7, 16}, {23, 59}, zone);
    Fixture fixture;
    viewmodel::StatisticsViewModel viewModel{
        fixture.statisticsService, fixture.taskService,
        [&now] { return now; }, [&zone] { return zone; }};
    QTimer *midnightTimer = viewModel.findChild<QTimer *>(
        QStringLiteral("statisticsMidnightTimer"));
    QVERIFY(midnightTimer != nullptr);
    QVERIFY(midnightTimer->isSingleShot());
    QVERIFY(midnightTimer->isActive());
    QVERIFY(midnightTimer->remainingTime() <= 60'000);

    now = localUtc({2026, 7, 17}, {8, 0}, zone);
    viewModel.reload();
    QVERIFY(midnightTimer->isActive());
    QVERIFY(midnightTimer->remainingTime() > 10 * 60 * 60 * 1000);
}

void StatisticsViewModelTest::appViewModelOwnsStatisticsOnlyWhenInjected()
{
    Fixture fixture;
    viewmodel::AppViewModel compatible{fixture.taskService};
    QCOMPARE(compatible.statistics(), nullptr);

    viewmodel::AppViewModel complete{fixture.taskService,
                                     fixture.statisticsService};
    QVERIFY(complete.statistics() != nullptr);
    QCOMPARE(complete.statistics(), complete.statistics());
}

QTEST_GUILESS_MAIN(StatisticsViewModelTest)
#include "tst_StatisticsViewModel.moc"
