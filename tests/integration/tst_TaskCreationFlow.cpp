#include "AppViewModel.h"
#include "TaskEditorViewModel.h"
#include "TaskListViewModel.h"
#include "domain/TaskCreationRequest.h"
#include "persistence/SqliteTaskRepository.h"
#include "services/TaskCategoryService.h"
#include "services/TaskService.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

#include <algorithm>

using smartmate::model::TaskCreationRequest;
using smartmate::model::TaskCategoryColor;
using smartmate::model::TaskCategoryId;
using smartmate::model::TaskCategoryService;
using smartmate::model::TaskDraft;
using smartmate::model::TaskDependencyResolution;
using smartmate::model::TaskGraphCategoryScope;
using smartmate::model::TaskGraphQuery;
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
    void cancelledDependencyRemainsStoredAndDerivedAfterReopen();
    void permanentlyDeletesArchivedTaskAndRefreshesDependencyState();
    void batchDeletesConnectedArchivedTasksAndRecalculatesAfterReopen();
    void persistsCategoriesAndScopesCrossCategoryGraphAfterReopen();
};

void TaskCreationFlowTest::createsAndReadsTaskWhenOptionalDescriptionIsUntouched()
{
    SqliteTaskRepository repository{QStringLiteral(":memory:")};
    TaskService service{repository, repository, repository, repository, repository,
                        repository};
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
    TaskService service{repository, repository, repository, repository, repository,
                        repository};

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
        TaskService service{repository, repository, repository, repository, repository,
                            repository};
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
        TaskService service{repository, repository, repository, repository, repository,
                            repository};

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

        QVERIFY(service.startTask(predecessorId).ok());
        QVERIFY(service.completeTask(predecessorId).ok());

        const auto unlockedSnapshot = service.taskGraphSnapshot();
        QVERIFY(unlockedSnapshot.ok());
        const TaskGraphNode *unlockedSuccessor =
            findNode(unlockedSnapshot.value->nodes, successorId);
        QVERIFY(unlockedSuccessor != nullptr);
        QVERIFY(!unlockedSuccessor->dependencyState.blocked);
        QVERIFY(unlockedSuccessor->dependencyState.unsatisfiedPredecessorIds.isEmpty());
    }
}

void TaskCreationFlowTest::cancelledDependencyRemainsStoredAndDerivedAfterReopen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("smartmate.db"));

    TaskId predecessorId;
    TaskId successorId;
    {
        SqliteTaskRepository repository{databasePath};
        TaskService service{repository, repository, repository, repository, repository,
                            repository};
        TaskDraft predecessorDraft;
        predecessorDraft.title = QStringLiteral("可取消前置");
        const auto predecessor = service.createTask(predecessorDraft);
        QVERIFY(predecessor.ok());
        predecessorId = predecessor.value->id();

        TaskDraft successorDraft;
        successorDraft.title = QStringLiteral("取消后解锁的后继");
        const auto successor = service.createTask(
            TaskCreationRequest{successorDraft, {predecessorId}});
        QVERIFY(successor.ok());
        successorId = successor.value->id();
        QVERIFY(service.cancelTask(predecessorId).ok());
    }

    {
        SqliteTaskRepository repository{databasePath};
        TaskService service{repository, repository, repository, repository, repository,
                            repository};
        const auto dependencies = service.listDependencies();
        QVERIFY(dependencies.ok());
        QCOMPARE(dependencies.value->size(), 1);
        QCOMPARE(dependencies.value->constFirst().predecessorId, predecessorId);
        QCOMPARE(dependencies.value->constFirst().successorId, successorId);

        const auto snapshot = service.taskGraphSnapshot();
        QVERIFY(snapshot.ok());
        QCOMPARE(snapshot.value->edges.size(), 1);
        QCOMPARE(snapshot.value->edges.constFirst().resolution,
                 TaskDependencyResolution::Cancelled);
        const auto successorNode = std::find_if(
            snapshot.value->nodes.cbegin(), snapshot.value->nodes.cend(),
            [&successorId](const TaskGraphNode &node) {
                return node.task.id() == successorId;
            });
        QVERIFY(successorNode != snapshot.value->nodes.cend());
        QVERIFY(!successorNode->dependencyState.blocked);
        QCOMPARE(successorNode->dependencyState.cancelledPredecessorIds,
                 QList<TaskId>{predecessorId});

        QVERIFY(service.redoTask(predecessorId).ok());
        const auto reactivated = service.taskGraphSnapshot();
        QVERIFY(reactivated.ok());
        QCOMPARE(reactivated.value->edges.constFirst().resolution,
                 TaskDependencyResolution::Pending);
    }
}

void TaskCreationFlowTest::permanentlyDeletesArchivedTaskAndRefreshesDependencyState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("smartmate.db"));
    TaskId predecessorId;
    TaskId successorId;

    {
        SqliteTaskRepository repository{databasePath};
        TaskService service{repository, repository, repository, repository, repository,
                            repository};
        TaskDraft predecessorDraft;
        predecessorDraft.title = QStringLiteral("将被永久删除的前置");
        const auto predecessor = service.createTask(predecessorDraft);
        QVERIFY(predecessor.ok());
        predecessorId = predecessor.value->id();

        TaskDraft successorDraft;
        successorDraft.title = QStringLiteral("删除依赖后仍保留的任务");
        const auto successor = service.createTask(
            TaskCreationRequest{successorDraft, {predecessorId}});
        QVERIFY(successor.ok());
        successorId = successor.value->id();

        QVERIFY(service.cancelTask(predecessorId).ok());
        QVERIFY(service.archiveTask(predecessorId).ok());
        QSignalSpy taskSpy{&service, &TaskService::tasksChanged};
        QSignalSpy dependencySpy{&service, &TaskService::dependenciesChanged};

        const auto deleted = service.deleteArchivedTask(predecessorId);
        QVERIFY2(deleted.ok(), qPrintable(deleted.detail));
        QCOMPARE(taskSpy.count(), 1);
        QCOMPARE(dependencySpy.count(), 1);
        QVERIFY(!repository.findById(predecessorId).has_value());
        QVERIFY(repository.findById(successorId).has_value());
        QVERIFY(repository.findAllDependencies().isEmpty());

        const auto snapshot = service.taskGraphSnapshot();
        QVERIFY(snapshot.ok());
        QCOMPARE(snapshot.value->nodes.size(), 1);
        QCOMPARE(snapshot.value->nodes.constFirst().task.id(), successorId);
        QVERIFY(!snapshot.value->nodes.constFirst().dependencyState.blocked);
    }

    {
        SqliteTaskRepository repository{databasePath};
        TaskService service{repository, repository, repository, repository, repository,
                            repository};
        QVERIFY(!repository.findById(predecessorId).has_value());
        QVERIFY(repository.findById(successorId).has_value());
        QVERIFY(repository.findAllDependencies().isEmpty());
        const auto plan = service.listRecommendedTasks();
        QVERIFY(plan.ok());
        QCOMPARE(plan.value->size(), 1);
        QCOMPARE(plan.value->constFirst().task.id(), successorId);
    }
}

void TaskCreationFlowTest::batchDeletesConnectedArchivedTasksAndRecalculatesAfterReopen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("smartmate.db"));
    TaskId firstId;
    TaskId secondId;
    TaskId successorId;

    {
        SqliteTaskRepository repository{databasePath};
        TaskService service{repository, repository, repository, repository, repository,
                            repository};

        TaskDraft firstDraft;
        firstDraft.title = QStringLiteral("批量删除链首");
        const auto first = service.createTask(firstDraft);
        QVERIFY(first.ok());
        firstId = first.value->id();

        TaskDraft secondDraft;
        secondDraft.title = QStringLiteral("批量删除链中间");
        const auto second = service.createTask(
            TaskCreationRequest{secondDraft, {firstId}});
        QVERIFY(second.ok());
        secondId = second.value->id();

        TaskDraft successorDraft;
        successorDraft.title = QStringLiteral("批量删除后保留的活动任务");
        const auto successor = service.createTask(
            TaskCreationRequest{successorDraft, {secondId}});
        QVERIFY(successor.ok());
        successorId = successor.value->id();

        QVERIFY(service.cancelTask(firstId).ok());
        QVERIFY(service.cancelTask(secondId).ok());
        const auto archived = service.archiveTasks({secondId, firstId});
        QVERIFY2(archived.ok(), qPrintable(archived.detail));

        QSignalSpy taskSpy{&service, &TaskService::tasksChanged};
        QSignalSpy dependencySpy{&service, &TaskService::dependenciesChanged};
        const auto deleted = service.deleteArchivedTasks({firstId, secondId});
        QVERIFY2(deleted.ok(), qPrintable(deleted.detail));
        QCOMPARE(deleted.value->tasks.size(), 2);
        QCOMPARE(deleted.value->removedDependencyCount, 2);
        QCOMPARE(taskSpy.count(), 1);
        QCOMPARE(dependencySpy.count(), 1);
        QVERIFY(!repository.findById(firstId).has_value());
        QVERIFY(!repository.findById(secondId).has_value());
        QVERIFY(repository.findById(successorId).has_value());
        QVERIFY(repository.findAllDependencies().isEmpty());

        const auto snapshot = service.taskGraphSnapshot();
        QVERIFY(snapshot.ok());
        QCOMPARE(snapshot.value->nodes.size(), 1);
        QCOMPARE(snapshot.value->nodes.constFirst().task.id(), successorId);
        QVERIFY(!snapshot.value->nodes.constFirst().dependencyState.blocked);
    }

    // 重启后只剩活动后继，依赖图不会保留已永久删除节点或悬空边。
    {
        SqliteTaskRepository repository{databasePath};
        TaskService service{repository, repository, repository, repository, repository,
                            repository};
        QVERIFY(!repository.findById(firstId).has_value());
        QVERIFY(!repository.findById(secondId).has_value());
        QVERIFY(repository.findById(successorId).has_value());
        QVERIFY(repository.findAllDependencies().isEmpty());
        const auto plan = service.listRecommendedTasks();
        QVERIFY(plan.ok());
        QCOMPARE(plan.value->size(), 1);
        QCOMPARE(plan.value->constFirst().task.id(), successorId);
    }
}

void TaskCreationFlowTest::persistsCategoriesAndScopesCrossCategoryGraphAfterReopen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("smartmate.db"));

    TaskCategoryId studyCategoryId;
    TaskCategoryId workCategoryId;
    TaskId distantWorkTaskId;
    TaskId directPredecessorId;
    TaskId studyTaskId;
    TaskId directSuccessorId;

    {
        SqliteTaskRepository repository{databasePath};
        TaskCategoryService categoryService{repository};
        TaskService service{repository, repository, repository, repository, repository,
                            repository};

        const auto studyCategory = categoryService.createCategory(
            {QStringLiteral("学习"), TaskCategoryColor::Blue});
        const auto workCategory = categoryService.createCategory(
            {QStringLiteral("工作"), TaskCategoryColor::Teal});
        QVERIFY2(studyCategory.ok(), qPrintable(studyCategory.detail));
        QVERIFY2(workCategory.ok(), qPrintable(workCategory.detail));
        studyCategoryId = studyCategory.value->id;
        workCategoryId = workCategory.value->id;

        TaskDraft distantWorkDraft;
        distantWorkDraft.title = QStringLiteral("两跳之外的工作任务");
        distantWorkDraft.categoryId = workCategoryId;
        const auto distantWorkTask = service.createTask(distantWorkDraft);
        QVERIFY(distantWorkTask.ok());
        distantWorkTaskId = distantWorkTask.value->id();

        TaskDraft directPredecessorDraft;
        directPredecessorDraft.title = QStringLiteral("学习任务的直接工作前置");
        directPredecessorDraft.categoryId = workCategoryId;
        const auto directPredecessor = service.createTask(
            TaskCreationRequest{directPredecessorDraft, {distantWorkTaskId}});
        QVERIFY2(directPredecessor.ok(), qPrintable(directPredecessor.detail));
        directPredecessorId = directPredecessor.value->id();

        TaskDraft studyDraft;
        studyDraft.title = QStringLiteral("分类图核心学习任务");
        studyDraft.categoryId = studyCategoryId;
        const auto studyTask = service.createTask(
            TaskCreationRequest{studyDraft, {directPredecessorId}});
        QVERIFY2(studyTask.ok(), qPrintable(studyTask.detail));
        studyTaskId = studyTask.value->id();

        TaskDraft directSuccessorDraft;
        directSuccessorDraft.title = QStringLiteral("学习任务的直接工作后继");
        directSuccessorDraft.categoryId = workCategoryId;
        const auto directSuccessor = service.createTask(
            TaskCreationRequest{directSuccessorDraft, {studyTaskId}});
        QVERIFY2(directSuccessor.ok(), qPrintable(directSuccessor.detail));
        directSuccessorId = directSuccessor.value->id();
    }

    {
        SqliteTaskRepository repository{databasePath};
        TaskCategoryService categoryService{repository};
        TaskService service{repository, repository, repository, repository, repository,
                            repository};

        // 重启后类别、任务归属与跨类别依赖必须从SQLite完整重建。
        const auto categories = categoryService.listCategories();
        QVERIFY(categories.ok());
        QCOMPARE(categories.value->size(), 2);
        QVERIFY(repository.findCategoryById(studyCategoryId).has_value());
        QVERIFY(repository.findCategoryById(workCategoryId).has_value());
        const auto storedStudyTask = repository.findById(studyTaskId);
        QVERIFY(storedStudyTask.has_value());
        QCOMPARE(storedStudyTask->categoryId(),
                 std::optional<TaskCategoryId>{studyCategoryId});
        QCOMPARE(repository.findAllDependencies().size(), 3);

        TaskGraphQuery studyQuery;
        studyQuery.scope = TaskGraphCategoryScope::SpecificCategory;
        studyQuery.categoryId = studyCategoryId;
        const auto scopedGraph = service.taskGraphSnapshot(studyQuery);
        QVERIFY2(scopedGraph.ok(), qPrintable(scopedGraph.detail));
        QCOMPARE(scopedGraph.value->nodes.size(), 3);
        QCOMPARE(scopedGraph.value->edges.size(), 2);

        const auto nodeById = [&scopedGraph](const TaskId &id)
            -> const TaskGraphNode * {
            const auto iterator = std::find_if(
                scopedGraph.value->nodes.cbegin(), scopedGraph.value->nodes.cend(),
                [&id](const TaskGraphNode &node) { return node.task.id() == id; });
            return iterator == scopedGraph.value->nodes.cend() ? nullptr
                                                                : &*iterator;
        };
        const TaskGraphNode *studyNode = nodeById(studyTaskId);
        const TaskGraphNode *predecessorNode = nodeById(directPredecessorId);
        const TaskGraphNode *successorNode = nodeById(directSuccessorId);
        QVERIFY(studyNode != nullptr);
        QVERIFY(predecessorNode != nullptr);
        QVERIFY(successorNode != nullptr);
        QVERIFY(studyNode->coreNode);
        QVERIFY(!predecessorNode->coreNode);
        QVERIFY(!successorNode->coreNode);
        QVERIFY(nodeById(distantWorkTaskId) == nullptr);

        const auto hasEdge = [&scopedGraph](const TaskId &predecessorId,
                                             const TaskId &successorId) {
            return std::any_of(
                scopedGraph.value->edges.cbegin(), scopedGraph.value->edges.cend(),
                [&predecessorId, &successorId](const auto &edge) {
                return edge.dependency.predecessorId == predecessorId
                    && edge.dependency.successorId == successorId;
            });
        };
        QVERIFY(hasEdge(directPredecessorId, studyTaskId));
        QVERIFY(hasEdge(studyTaskId, directSuccessorId));
        QVERIFY(!hasEdge(distantWorkTaskId, directPredecessorId));

        QSignalSpy categoriesChangedSpy{
            &categoryService, &TaskCategoryService::categoriesChanged};
        QSignalSpy assignmentsChangedSpy{
            &categoryService,
            &TaskCategoryService::taskCategoryAssignmentsChanged};
        const auto deletion = categoryService.deleteCategory(studyCategoryId);
        QVERIFY2(deletion.ok(), qPrintable(deletion.detail));
        QCOMPARE(deletion.value->unassignedTaskCount, 1);
        QCOMPARE(categoriesChangedSpy.count(), 1);
        QCOMPARE(assignmentsChangedSpy.count(), 1);

        const auto unassignedStudyTask = repository.findById(studyTaskId);
        QVERIFY(unassignedStudyTask.has_value());
        QVERIFY(!unassignedStudyTask->categoryId().has_value());
        const auto storedPredecessor = repository.findById(directPredecessorId);
        QVERIFY(storedPredecessor.has_value());
        QCOMPARE(storedPredecessor->categoryId(),
                 std::optional<TaskCategoryId>{workCategoryId});
        // 删除类别只解除任务归属，不得清理或改写任何依赖边。
        QCOMPARE(repository.findAllDependencies().size(), 3);

        TaskGraphQuery uncategorizedQuery;
        uncategorizedQuery.scope = TaskGraphCategoryScope::Uncategorized;
        const auto uncategorizedGraph = service.taskGraphSnapshot(uncategorizedQuery);
        QVERIFY2(uncategorizedGraph.ok(), qPrintable(uncategorizedGraph.detail));
        QCOMPARE(uncategorizedGraph.value->nodes.size(), 3);
        QCOMPARE(uncategorizedGraph.value->edges.size(), 2);
    }

    // 再次重启验证类别删除、未分类归属和原有依赖关系同时持久化。
    {
        SqliteTaskRepository repository{databasePath};
        TaskService service{repository, repository, repository, repository, repository,
                            repository};
        QVERIFY(!repository.findCategoryById(studyCategoryId).has_value());
        QVERIFY(repository.findCategoryById(workCategoryId).has_value());
        const auto studyTask = repository.findById(studyTaskId);
        QVERIFY(studyTask.has_value());
        QVERIFY(!studyTask->categoryId().has_value());
        QCOMPARE(repository.findAllDependencies().size(), 3);

        TaskGraphQuery uncategorizedQuery;
        uncategorizedQuery.scope = TaskGraphCategoryScope::Uncategorized;
        const auto graph = service.taskGraphSnapshot(uncategorizedQuery);
        QVERIFY(graph.ok());
        QCOMPARE(graph.value->nodes.size(), 3);
        QCOMPARE(graph.value->edges.size(), 2);
    }
}

// Qt SQL 连接依赖 QCoreApplication 生命周期，测试无需 GUI 事件环境。
QTEST_GUILESS_MAIN(TaskCreationFlowTest)

#include "tst_TaskCreationFlow.moc"
