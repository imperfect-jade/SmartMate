#include "AppViewModel.h"
#include "TaskEditorViewModel.h"
#include "TaskListViewModel.h"
#include "domain/TaskCreationRequest.h"
#include "persistence/SqliteTaskRepository.h"
#include "services/TaskService.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

using smartmate::model::TaskCreationRequest;
using smartmate::model::TaskDraft;
using smartmate::model::TaskGraphNode;
using smartmate::model::TaskId;
using smartmate::model::TaskPriority;
using smartmate::model::TaskService;
using smartmate::model::TaskStatus;
using smartmate::model::persistence::SqliteTaskRepository;
using smartmate::viewmodel::AppViewModel;
using smartmate::viewmodel::TaskListViewModel;

/// 覆盖真实 SQLite 纵向链路，避免各层独立测试遗漏组合后的事务与刷新行为。
class TaskCreationFlowTest final : public QObject {
    Q_OBJECT

private slots:
    void createsAndReadsTaskWhenOptionalDescriptionIsUntouched();
    void derivedSearchAndOrderingDoNotModifyStoredTasks();
    void atomicallyCreatesDependencyAndUnlocksAfterReopen();
};

void TaskCreationFlowTest::createsAndReadsTaskWhenOptionalDescriptionIsUntouched()
{
    SqliteTaskRepository repository{QStringLiteral(":memory:")};
    TaskService service{repository, repository, repository};
    AppViewModel appViewModel{service};
    auto *editor = appViewModel.taskEditor();
    auto *taskList = appViewModel.taskList();
    QSignalSpy listErrorSpy{taskList, &TaskListViewModel::errorOccurred};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};

    // 精确模拟用户只填写标题、从未操作可选描述框的创建路径。
    QVERIFY(editor->beginCreate());
    editor->setTitle(QStringLiteral("只填写标题也能保存"));

    QVERIFY2(editor->save(), qPrintable(editor->errorMessage()));
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(listErrorSpy.count(), 0);
    QCOMPARE(taskList->count(), 1);

    const QModelIndex firstTask = taskList->index(0);
    const QString taskId =
        taskList->data(firstTask, TaskListViewModel::TaskIdRole).toString();
    QVERIFY(!QUuid::fromString(taskId).isNull());
    QVERIFY(editor->beginEdit(taskId));
    QCOMPARE(editor->title(), QStringLiteral("只填写标题也能保存"));
    QVERIFY(editor->description().isEmpty());

    const auto stored = repository.findById(QUuid::fromString(taskId));
    QVERIFY(stored.has_value());
    QVERIFY(stored->description().isEmpty());
    QVERIFY(!stored->description().isNull());
}

void TaskCreationFlowTest::derivedSearchAndOrderingDoNotModifyStoredTasks()
{
    SqliteTaskRepository repository{QStringLiteral(":memory:")};
    TaskService service{repository, repository, repository};

    TaskDraft urgentDraft;
    urgentDraft.title = QStringLiteral("准备课程答辩");
    urgentDraft.description = QStringLiteral("整理架构说明");
    urgentDraft.priority = TaskPriority::Urgent;
    QVERIFY(service.createTask(urgentDraft).ok());

    TaskDraft lowDraft;
    lowDraft.title = QStringLiteral("清理草稿");
    lowDraft.priority = TaskPriority::Low;
    QVERIFY(service.createTask(lowDraft).ok());

    AppViewModel appViewModel{service};
    auto *taskList = appViewModel.taskList();
    const auto before = repository.findAll();
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};

    taskList->setSearchText(QStringLiteral("架构"));
    taskList->setPriorityFilterIndex(4);
    QCOMPARE(taskList->count(), 1);
    taskList->clearFilters();
    taskList->reload();

    const auto after = repository.findAll();
    QCOMPARE(changedSpy.count(), 0);
    QVERIFY(after == before);
}

void TaskCreationFlowTest::atomicallyCreatesDependencyAndUnlocksAfterReopen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("smartmate.db"));

    TaskId predecessorId;
    TaskId successorId;
    {
        SqliteTaskRepository repository{databasePath};
        TaskService service{repository, repository, repository};
        QSignalSpy tasksChangedSpy{&service, &TaskService::tasksChanged};
        QSignalSpy dependenciesChangedSpy{&service,
                                          &TaskService::dependenciesChanged};

        TaskDraft predecessorDraft;
        predecessorDraft.title = QStringLiteral("完成需求分析");
        const auto predecessorResult = service.createTask(predecessorDraft);
        QVERIFY(predecessorResult.ok());
        predecessorId = predecessorResult.value->id();

        TaskDraft successorDraft;
        successorDraft.title = QStringLiteral("实现任务模块");
        const auto successorResult = service.createTask(
            TaskCreationRequest{successorDraft, {predecessorId}});
        QVERIFY2(successorResult.ok(), qPrintable(successorResult.detail));
        successorId = successorResult.value->id();

        // 原子创建仅形成一次任务变化和一次依赖变化，且边立即可见。
        QCOMPARE(tasksChangedSpy.count(), 2);
        QCOMPARE(dependenciesChangedSpy.count(), 1);
        const auto dependencies = service.listDependencies();
        QVERIFY(dependencies.ok());
        QCOMPARE(dependencies.value->size(), 1);
        QCOMPARE(dependencies.value->first().predecessorId, predecessorId);
        QCOMPARE(dependencies.value->first().successorId, successorId);
    }

    {
        SqliteTaskRepository repository{databasePath};
        TaskService service{repository, repository, repository};

        const auto dependencies = service.listDependencies();
        QVERIFY(dependencies.ok());
        QCOMPARE(dependencies.value->size(), 1);
        QCOMPARE(dependencies.value->first().predecessorId, predecessorId);
        QCOMPARE(dependencies.value->first().successorId, successorId);

        const auto blockedSnapshot = service.taskGraphSnapshot();
        QVERIFY(blockedSnapshot.ok());
        const auto findNode = [](const QList<TaskGraphNode> &nodes,
                                 const TaskId &taskId) -> const TaskGraphNode * {
            for (const TaskGraphNode &node : nodes) {
                if (node.task.id() == taskId) {
                    return &node;
                }
            }
            return nullptr;
        };
        const TaskGraphNode *blockedSuccessor =
            findNode(blockedSnapshot.value->nodes, successorId);
        QVERIFY(blockedSuccessor != nullptr);
        QVERIFY(blockedSuccessor->dependencyState.blocked);
        QCOMPARE(blockedSuccessor->dependencyLevel, 1);

        const auto storedPredecessor = service.findTask(predecessorId);
        QVERIFY(storedPredecessor.ok());
        TaskDraft completedDraft;
        completedDraft.title = storedPredecessor.value->title();
        completedDraft.description = storedPredecessor.value->description();
        completedDraft.priority = storedPredecessor.value->priority();
        completedDraft.status = TaskStatus::Done;
        completedDraft.deadline = storedPredecessor.value->deadline();
        completedDraft.estimatedMinutes = storedPredecessor.value->estimatedMinutes();
        QVERIFY(service.updateTask(predecessorId, completedDraft).ok());

        const auto unlockedSnapshot = service.taskGraphSnapshot();
        QVERIFY(unlockedSnapshot.ok());
        const TaskGraphNode *unlockedSuccessor =
            findNode(unlockedSnapshot.value->nodes, successorId);
        QVERIFY(unlockedSuccessor != nullptr);
        QVERIFY(!unlockedSuccessor->dependencyState.blocked);
        QVERIFY(unlockedSuccessor->dependencyState.unsatisfiedPredecessorIds.isEmpty());
    }
}

// Qt SQL 连接依赖 QCoreApplication 生命周期，测试无需 GUI 事件环境。
QTEST_GUILESS_MAIN(TaskCreationFlowTest)

#include "tst_TaskCreationFlow.moc"
