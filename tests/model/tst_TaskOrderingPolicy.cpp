#include "fakes/FakeTaskRepository.h"

#include "planner/TaskOrderingPolicy.h"
#include "services/TaskService.h"

#include <QTest>
#include <QTimeZone>

#include <optional>
#include <utility>

using smartmate::model::PlannedTask;
using smartmate::model::Task;
using smartmate::model::TaskError;
using smartmate::model::TaskId;
using smartmate::model::TaskOrderReason;
using smartmate::model::TaskPriority;
using smartmate::model::TaskService;
using smartmate::model::TaskStatus;
using smartmate::model::orderTasks;
using smartmate::tests::FakeTaskRepository;

namespace {

[[nodiscard]] QDateTime timestamp(const qint64 seconds)
{
    return QDateTime::fromSecsSinceEpoch(seconds, QTimeZone::UTC);
}

[[nodiscard]] TaskId taskId(const int suffix)
{
    return TaskId::fromString(
        QStringLiteral("00000000-0000-0000-0000-%1").arg(suffix, 12, 10, QLatin1Char('0')));
}

[[nodiscard]] Task makeTask(const int idSuffix,
                            const TaskStatus status,
                            const TaskPriority priority = TaskPriority::Normal,
                            std::optional<QDateTime> deadline = std::nullopt,
                            const qint64 createdSeconds = 100,
                            const qint64 updatedSeconds = 100)
{
    return Task{taskId(idSuffix),
                QStringLiteral("Task %1").arg(idSuffix),
                QStringLiteral("description"),
                priority,
                status,
                status == TaskStatus::Archived
                    ? std::optional<TaskStatus>{TaskStatus::Todo}
                    : std::nullopt,
                std::move(deadline),
                std::nullopt,
                timestamp(createdSeconds),
                timestamp(updatedSeconds)};
}

[[nodiscard]] QList<TaskId> orderedIds(const QList<PlannedTask> &plan)
{
    QList<TaskId> ids;
    ids.reserve(plan.size());
    for (const PlannedTask &entry : plan) {
        ids.append(entry.task.id());
    }
    return ids;
}

} // namespace

// 排序策略是纯 Model 规则；测试固定当前时间，避免系统时钟导致逾期边界漂移。
class TaskOrderingPolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void ordersStatusAndTodoRulesAndAssignsReasons();
    void ordersAllPrioritiesIndependentlyOfInputOrder();
    void usesCreationAndStableIdAsTodoTieBreakers();
    void interleavesTerminalStatesAndKeepsArchivesLast();
    void serviceReturnsPlanAndMapsRepositoryFailure();
};

void TaskOrderingPolicyTest::ordersStatusAndTodoRulesAndAssignsReasons()
{
    const QDateTime now = timestamp(10'000);
    const QList<Task> tasks{
        makeTask(90, TaskStatus::Archived, TaskPriority::Urgent,
                 now.addSecs(-10), 10, 50'000),
        makeTask(80, TaskStatus::Done, TaskPriority::Urgent,
                 now.addSecs(-10), 10, 300),
        makeTask(70, TaskStatus::Cancelled, TaskPriority::Urgent,
                 now.addSecs(-10), 10, 400),
        makeTask(60, TaskStatus::Todo, TaskPriority::Normal),
        makeTask(50, TaskStatus::Todo, TaskPriority::Normal, now),
        makeTask(40, TaskStatus::Todo, TaskPriority::High),
        makeTask(30, TaskStatus::Todo, TaskPriority::Urgent),
        makeTask(20, TaskStatus::Todo, TaskPriority::Low, now.addSecs(-100)),
        makeTask(10, TaskStatus::InProgress, TaskPriority::Low),
    };

    const QList<PlannedTask> plan = orderTasks(tasks, now);

    QCOMPARE(orderedIds(plan),
             QList<TaskId>({taskId(10), taskId(20), taskId(30), taskId(40),
                            taskId(50), taskId(60), taskId(70), taskId(80),
                            taskId(90)}));
    QCOMPARE(plan.at(0).reason, TaskOrderReason::InProgress);
    QCOMPARE(plan.at(1).reason, TaskOrderReason::Overdue);
    QCOMPARE(plan.at(2).reason, TaskOrderReason::UrgentPriority);
    QCOMPARE(plan.at(3).reason, TaskOrderReason::HighPriority);
    // deadline == now 不算逾期，并由截止时间成为推荐理由。
    QCOMPARE(plan.at(4).reason, TaskOrderReason::UpcomingDeadline);
    QCOMPARE(plan.at(5).reason, TaskOrderReason::Todo);
    QCOMPARE(plan.at(6).reason, TaskOrderReason::Cancelled);
    QCOMPARE(plan.at(7).reason, TaskOrderReason::Completed);
    QCOMPARE(plan.at(8).reason, TaskOrderReason::Archived);
}

void TaskOrderingPolicyTest::ordersAllPrioritiesIndependentlyOfInputOrder()
{
    const QDateTime now = timestamp(10'000);
    const Task low = makeTask(40, TaskStatus::Todo, TaskPriority::Low);
    const Task normal = makeTask(30, TaskStatus::Todo, TaskPriority::Normal);
    const Task high = makeTask(20, TaskStatus::Todo, TaskPriority::High);
    const Task urgent = makeTask(10, TaskStatus::Todo, TaskPriority::Urgent);
    const QList<TaskId> expected{taskId(10), taskId(20), taskId(30), taskId(40)};

    QCOMPARE(orderedIds(orderTasks({low, high, urgent, normal}, now)), expected);
    QCOMPARE(orderedIds(orderTasks({normal, urgent, low, high}, now)), expected);
}

void TaskOrderingPolicyTest::usesCreationAndStableIdAsTodoTieBreakers()
{
    const QDateTime now = timestamp(10'000);
    const QDateTime earlyDeadline = now.addSecs(600);
    const QDateTime laterDeadline = now.addSecs(1'200);
    const QList<Task> tasks{
        makeTask(60, TaskStatus::Todo, TaskPriority::Normal,
                 std::nullopt, 1),
        makeTask(50, TaskStatus::Todo, TaskPriority::Normal,
                 laterDeadline, 5),
        makeTask(40, TaskStatus::Todo, TaskPriority::Normal,
                 laterDeadline, 4),
        makeTask(30, TaskStatus::Todo, TaskPriority::Normal,
                 laterDeadline, 4),
        makeTask(20, TaskStatus::Todo, TaskPriority::Normal,
                 earlyDeadline, 999),
        // 逾期组内部仍先比较优先级，再比较截止时间。
        makeTask(10, TaskStatus::Todo, TaskPriority::Low,
                 now.addSecs(-1'000)),
        makeTask(5, TaskStatus::Todo, TaskPriority::Urgent,
                 now.addSecs(-10)),
    };

    const QList<PlannedTask> plan = orderTasks(tasks, now);

    QCOMPARE(orderedIds(plan),
             QList<TaskId>({taskId(5), taskId(10), taskId(20), taskId(30),
                            taskId(40), taskId(50), taskId(60)}));
}

void TaskOrderingPolicyTest::interleavesTerminalStatesAndKeepsArchivesLast()
{
    const QDateTime now = timestamp(10'000);
    const QList<Task> tasks{
        makeTask(60, TaskStatus::Archived, TaskPriority::Normal,
                 std::nullopt, 1, 900),
        makeTask(50, TaskStatus::Archived, TaskPriority::Normal,
                 std::nullopt, 1, 1'000),
        makeTask(40, TaskStatus::Archived, TaskPriority::Normal,
                 std::nullopt, 1, 1'000),
        makeTask(30, TaskStatus::Done, TaskPriority::Normal,
                 std::nullopt, 1, 200),
        makeTask(20, TaskStatus::Cancelled, TaskPriority::Normal,
                 std::nullopt, 1, 300),
        makeTask(10, TaskStatus::Cancelled, TaskPriority::Normal,
                 std::nullopt, 1, 300),
    };

    const QList<PlannedTask> plan = orderTasks(tasks, now);

    // Done 与 Cancelled 不拆组，先按更新时间降序；Archived 无论多新都在最后。
    QCOMPARE(orderedIds(plan),
             QList<TaskId>({taskId(10), taskId(20), taskId(30), taskId(40),
                            taskId(50), taskId(60)}));
}

void TaskOrderingPolicyTest::serviceReturnsPlanAndMapsRepositoryFailure()
{
    FakeTaskRepository repository{{
        makeTask(20, TaskStatus::Todo),
        makeTask(10, TaskStatus::InProgress),
    }};
    const TaskService service{repository};

    const auto result = service.listRecommendedTasks();
    QVERIFY(result.ok());
    QCOMPARE(orderedIds(*result.value), QList<TaskId>({taskId(10), taskId(20)}));

    repository.setReadFailure(true);
    const auto failure = service.listRecommendedTasks();
    QCOMPARE(failure.error, TaskError::PersistenceFailure);
    QVERIFY(!failure.detail.isEmpty());
}

QTEST_APPLESS_MAIN(TaskOrderingPolicyTest)

#include "tst_TaskOrderingPolicy.moc"
