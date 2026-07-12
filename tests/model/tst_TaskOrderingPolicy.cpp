#include "fakes/FakeTaskRepository.h"
#include "fakes/FakeTaskDependencyRepository.h"
#include "fakes/FakeTaskCreationRepository.h"

#include "dependencies/TaskDependencyGraph.h"
#include "planner/TaskOrderingPolicy.h"
#include "services/TaskService.h"

#include <QTest>
#include <QTimeZone>

#include <algorithm>
#include <optional>
#include <utility>

using smartmate::model::PlannedTask;
using smartmate::model::Task;
using smartmate::model::TaskDependency;
using smartmate::model::TaskDependencyGraph;
using smartmate::model::TaskError;
using smartmate::model::TaskId;
using smartmate::model::TaskOrderReason;
using smartmate::model::TaskPriority;
using smartmate::model::TaskService;
using smartmate::model::TaskStatus;
using smartmate::model::orderTasks;
using smartmate::tests::FakeTaskDependencyRepository;
using smartmate::tests::FakeTaskCreationRepository;
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
    void graphRejectsMissingSelfAndDuplicateEdges();
    void graphReportsStableClosedCyclePath();
    void graphValidatesVeryLongChainWithoutRecursion();
    void graphComputesAndBlockingAndUnlockState();
    void graphComputesStableLongestLevelsAndConnectedClosure();
    void plannerOrdersReadyBeforeTopologicalBlockedTasks();
    void plannerKeepsAllTasksWhenGivenInvalidCycle();
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

void TaskOrderingPolicyTest::graphRejectsMissingSelfAndDuplicateEdges()
{
    const QList<Task> tasks{
        makeTask(1, TaskStatus::Todo),
        makeTask(2, TaskStatus::Todo),
    };

    const auto missing = TaskDependencyGraph{
        tasks, {{taskId(99), taskId(1)}}}.validation();
    QCOMPARE(missing.error,
             smartmate::model::DependencyGraphError::MissingTask);
    QCOMPARE(missing.conflictingTaskIds,
             QList<TaskId>({taskId(99)}));

    const auto selfReference = TaskDependencyGraph{
        tasks, {{taskId(1), taskId(1)}}}.validation();
    QCOMPARE(selfReference.error,
             smartmate::model::DependencyGraphError::SelfDependency);
    QCOMPARE(selfReference.conflictingTaskIds,
             QList<TaskId>({taskId(1)}));

    const auto duplicate = TaskDependencyGraph{
        tasks,
        {{taskId(1), taskId(2)},
         {taskId(1), taskId(2)}}}.validation();
    QCOMPARE(duplicate.error,
             smartmate::model::DependencyGraphError::DuplicateDependency);
    QCOMPARE(duplicate.conflictingTaskIds,
             QList<TaskId>({taskId(1), taskId(2)}));
}

void TaskOrderingPolicyTest::graphReportsStableClosedCyclePath()
{
    const QList<Task> tasks{
        makeTask(3, TaskStatus::Todo),
        makeTask(1, TaskStatus::Todo),
        makeTask(2, TaskStatus::Todo),
    };
    // 输入故意乱序；Graph 必须仍返回稳定、首尾闭合的完整环。
    const TaskDependencyGraph graph{
        tasks,
        {{taskId(3), taskId(1)},
         {taskId(1), taskId(2)},
         {taskId(2), taskId(3)}}};

    const auto validation = graph.validation();
    QCOMPARE(validation.error, smartmate::model::DependencyGraphError::Cycle);
    QCOMPARE(validation.cyclePath,
             QList<TaskId>({taskId(1), taskId(2), taskId(3), taskId(1)}));
    QCOMPARE(validation.cyclePath.constFirst(), validation.cyclePath.constLast());
}

void TaskOrderingPolicyTest::graphValidatesVeryLongChainWithoutRecursion()
{
    constexpr int taskCount = 10'000;
    QList<Task> tasks;
    QList<TaskDependency> dependencies;
    tasks.reserve(taskCount);
    dependencies.reserve(taskCount);
    for (int suffix = 1; suffix <= taskCount; ++suffix) {
        tasks.append(makeTask(suffix, TaskStatus::Todo));
        if (suffix > 1) {
            dependencies.append({taskId(suffix - 1), taskId(suffix)});
        }
    }

    const TaskDependencyGraph longChain{tasks, dependencies};
    QVERIFY(longChain.validation().ok());

    dependencies.append({taskId(taskCount), taskId(1)});
    const auto cycle = TaskDependencyGraph{tasks, dependencies}.validation();
    QCOMPARE(cycle.error, smartmate::model::DependencyGraphError::Cycle);
    QCOMPARE(cycle.cyclePath.size(), taskCount + 1);
    QCOMPARE(cycle.cyclePath.at(0), taskId(1));
    QCOMPARE(cycle.cyclePath.at(1), taskId(2));
    QCOMPARE(cycle.cyclePath.at(taskCount - 1), taskId(taskCount));
    QCOMPARE(cycle.cyclePath.constLast(), taskId(1));
}

void TaskOrderingPolicyTest::graphComputesAndBlockingAndUnlockState()
{
    const Task archivedCompleted{
        taskId(5),
        QStringLiteral("Archived completed"),
        QStringLiteral("description"),
        TaskPriority::Normal,
        TaskStatus::Archived,
        TaskStatus::Done,
        std::nullopt,
        std::nullopt,
        timestamp(1),
        timestamp(1)};
    const QList<Task> tasks{
        makeTask(1, TaskStatus::Todo),
        makeTask(2, TaskStatus::Todo),
        makeTask(3, TaskStatus::Todo),
        makeTask(4, TaskStatus::Todo),
        archivedCompleted,
        makeTask(6, TaskStatus::Done),
    };
    // 菱形图额外包含 A→D 的冗余传递边，仍是合法 DAG。
    const TaskDependencyGraph graph{
        tasks,
        {{taskId(1), taskId(2)},
         {taskId(1), taskId(3)},
         {taskId(2), taskId(4)},
         {taskId(3), taskId(4)},
         {taskId(1), taskId(4)},
         {taskId(5), taskId(2)},
         {taskId(1), taskId(6)}}};

    QVERIFY(graph.validation().ok());
    const auto secondState = graph.dependencyState(taskId(2));
    QCOMPARE(secondState.predecessorIds,
             QList<TaskId>({taskId(1), taskId(5)}));
    // Archived-before-Done 已满足，因此只有 A 阻塞 B。
    QCOMPARE(secondState.unsatisfiedPredecessorIds,
             QList<TaskId>({taskId(1)}));
    QVERIFY(secondState.blocked);

    const auto fourthState = graph.dependencyState(taskId(4));
    QCOMPARE(fourthState.unsatisfiedPredecessorIds,
             QList<TaskId>({taskId(1), taskId(2), taskId(3)}));
    QVERIFY(fourthState.blocked);
    // AND 语义下，完成 A 只直接解锁 B、C，不会提前解锁 D。
    QCOMPARE(graph.dependencyState(taskId(1)).unlockCount, 2);

    const auto terminalState = graph.dependencyState(taskId(6));
    QCOMPARE(terminalState.unsatisfiedPredecessorIds,
             QList<TaskId>({taskId(1)}));
    QVERIFY(!terminalState.blocked);
}

void TaskOrderingPolicyTest::graphComputesStableLongestLevelsAndConnectedClosure()
{
    const QList<Task> tasks{
        makeTask(1, TaskStatus::Todo),
        makeTask(2, TaskStatus::Todo),
        makeTask(3, TaskStatus::Todo),
        makeTask(4, TaskStatus::Todo),
        makeTask(5, TaskStatus::Todo),
        makeTask(6, TaskStatus::Todo),
        makeTask(7, TaskStatus::Todo),
    };
    const QList<TaskDependency> dependencies{
        {taskId(2), taskId(4)},
        {taskId(3), taskId(4)},
        {taskId(2), taskId(3)},
        {taskId(1), taskId(3)},
        {taskId(5), taskId(6)},
    };
    const TaskDependencyGraph graph{tasks, dependencies};

    QVERIFY(graph.validation().ok());
    const QHash<TaskId, int> levels = graph.dependencyLevels();
    QCOMPARE(levels.value(taskId(1)), 0);
    QCOMPARE(levels.value(taskId(2)), 0);
    QCOMPARE(levels.value(taskId(3)), 1);
    // 2→4 是短边，但最长链 1/2→3→4 决定层级 2。
    QCOMPARE(levels.value(taskId(4)), 2);
    QCOMPARE(levels.value(taskId(5)), 0);
    QCOMPARE(levels.value(taskId(6)), 1);
    QCOMPARE(levels.value(taskId(7)), 0);
    QCOMPARE(graph.connectedTaskIds({taskId(4)}),
             QList<TaskId>({taskId(1), taskId(2), taskId(3), taskId(4)}));
    QCOMPARE(graph.connectedTaskIds({taskId(7)}),
             QList<TaskId>({taskId(7)}));
    QCOMPARE(graph.predecessorClosure(taskId(4)),
             QList<TaskId>({taskId(1), taskId(2), taskId(3)}));
    QCOMPARE(graph.successorClosure(taskId(1)),
             QList<TaskId>({taskId(3), taskId(4)}));
    QVERIFY(graph.predecessorClosure(taskId(7)).isEmpty());
    QVERIFY(graph.successorClosure(taskId(7)).isEmpty());

    QList<Task> reversedTasks = tasks;
    QList<TaskDependency> reversedDependencies = dependencies;
    std::reverse(reversedTasks.begin(), reversedTasks.end());
    std::reverse(reversedDependencies.begin(), reversedDependencies.end());
    const TaskDependencyGraph reversedGraph{reversedTasks,
                                            reversedDependencies};
    QCOMPARE(reversedGraph.dependencyLevels(), levels);
    QCOMPARE(reversedGraph.connectedTaskIds({taskId(4), taskId(1)}),
             graph.connectedTaskIds({taskId(1), taskId(4)}));
    QCOMPARE(reversedGraph.predecessorClosure(taskId(4)),
             graph.predecessorClosure(taskId(4)));
    QCOMPARE(reversedGraph.successorClosure(taskId(1)),
             graph.successorClosure(taskId(1)));

    const TaskDependencyGraph cyclicGraph{
        {makeTask(1, TaskStatus::Todo), makeTask(2, TaskStatus::Todo)},
        {{taskId(1), taskId(2)}, {taskId(2), taskId(1)}}};
    QVERIFY(cyclicGraph.dependencyLevels().isEmpty());
}

void TaskOrderingPolicyTest::plannerOrdersReadyBeforeTopologicalBlockedTasks()
{
    const QDateTime now = timestamp(10'000);
    const QList<Task> tasks{
        makeTask(1, TaskStatus::Todo, TaskPriority::Low),
        makeTask(2, TaskStatus::Todo, TaskPriority::Urgent),
        makeTask(3, TaskStatus::Todo, TaskPriority::Urgent),
        makeTask(4, TaskStatus::Todo, TaskPriority::Low),
    };
    const QList<TaskDependency> dependencies{
        {taskId(1), taskId(3)},
        {taskId(3), taskId(4)},
    };

    const QList<PlannedTask> plan = orderTasks(tasks, dependencies, now);

    // 所有当前 Ready 任务先按原规则排序，Blocked 分组再保持 A→C 的拓扑顺序。
    QCOMPARE(orderedIds(plan),
             QList<TaskId>({taskId(2), taskId(1), taskId(3), taskId(4)}));
    QVERIFY(!plan.at(0).dependencyState.blocked);
    QVERIFY(!plan.at(1).dependencyState.blocked);
    QVERIFY(plan.at(2).dependencyState.blocked);
    QVERIFY(plan.at(3).dependencyState.blocked);
}

void TaskOrderingPolicyTest::plannerKeepsAllTasksWhenGivenInvalidCycle()
{
    const QDateTime now = timestamp(10'000);
    const QList<Task> tasks{
        makeTask(1, TaskStatus::Todo),
        makeTask(2, TaskStatus::Todo),
        makeTask(3, TaskStatus::Todo),
    };
    const QList<TaskDependency> dependencies{
        {taskId(1), taskId(2)},
        {taskId(2), taskId(3)},
        {taskId(3), taskId(1)},
    };

    // 策略直调面对坏数据不能死循环或丢任务；Service 会在调用前返回 Cycle 错误。
    const QList<PlannedTask> plan = orderTasks(tasks, dependencies, now);
    QCOMPARE(plan.size(), tasks.size());
    QCOMPARE(orderedIds(plan),
             QList<TaskId>({taskId(1), taskId(2), taskId(3)}));
}

void TaskOrderingPolicyTest::serviceReturnsPlanAndMapsRepositoryFailure()
{
    FakeTaskRepository repository{{
        makeTask(20, TaskStatus::Todo),
        makeTask(10, TaskStatus::InProgress),
    }};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    const TaskService service{repository, dependencyRepository, creationRepository};

    const auto result = service.listRecommendedTasks();
    QVERIFY(result.ok());
    QCOMPARE(orderedIds(*result.value), QList<TaskId>({taskId(10), taskId(20)}));

    repository.setReadFailure(true);
    const auto failure = service.listRecommendedTasks();
    QCOMPARE(failure.error, TaskError::PersistenceFailure);
    QVERIFY(!failure.detail.isEmpty());

    repository.setReadFailure(false);
    FakeTaskDependencyRepository cyclicDependencies{
        {{taskId(10), taskId(20)},
         {taskId(20), taskId(10)}}};
    FakeTaskCreationRepository cyclicCreation{repository, cyclicDependencies};
    const TaskService cyclicService{repository, cyclicDependencies,
                                    cyclicCreation};
    const auto cycle = cyclicService.listRecommendedTasks();
    QCOMPARE(cycle.error, TaskError::DependencyCycle);
    QCOMPARE(cycle.context.cyclePath.constFirst(),
             cycle.context.cyclePath.constLast());
    QCOMPARE(cyclicService.taskGraphSnapshot().error,
             TaskError::DependencyCycle);

    const Task unfinished = makeTask(30, TaskStatus::Todo);
    const Task completedSuccessor = makeTask(40, TaskStatus::Done);
    const Task archivedActiveSuccessor{
        taskId(50),
        QStringLiteral("Archived active successor"),
        QStringLiteral("description"),
        TaskPriority::Normal,
        TaskStatus::Archived,
        TaskStatus::InProgress,
        std::nullopt,
        std::nullopt,
        timestamp(100),
        timestamp(100)};
    FakeTaskRepository inconsistentRepository{
        {unfinished, completedSuccessor, archivedActiveSuccessor}};
    FakeTaskDependencyRepository inconsistentDependencies{
        {{unfinished.id(), completedSuccessor.id()},
         {unfinished.id(), archivedActiveSuccessor.id()}}};
    FakeTaskCreationRepository inconsistentCreation{
        inconsistentRepository, inconsistentDependencies};
    const TaskService inconsistentService{
        inconsistentRepository, inconsistentDependencies,
        inconsistentCreation};
    const auto inconsistentPlan = inconsistentService.listRecommendedTasks();
    QCOMPARE(inconsistentPlan.error, TaskError::DependencyStateConflict);
    QCOMPARE(inconsistentService.taskGraphSnapshot().error,
             TaskError::DependencyStateConflict);
    QCOMPARE(inconsistentPlan.context.blockingTaskIds,
             QList<TaskId>({unfinished.id()}));
    QCOMPARE(inconsistentPlan.context.conflictingTaskIds,
             QList<TaskId>({completedSuccessor.id(), archivedActiveSuccessor.id()}));
}

QTEST_APPLESS_MAIN(TaskOrderingPolicyTest)

#include "tst_TaskOrderingPolicy.moc"
