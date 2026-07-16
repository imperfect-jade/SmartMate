#include "fakes/FakeTaskRepository.h"
#include "fakes/FakeTaskDependencyRepository.h"
#include "fakes/FakeTaskCreationRepository.h"
#include "fakes/FakeTaskBatchTransitionRepository.h"
#include "fakes/FakeTaskDeletionRepository.h"
#include "fakes/FakeTaskCategoryRepository.h"

#include "domain/Task.h"
#include "domain/TaskConstraints.h"
#include "services/TaskService.h"

#include <QDateTime>
#include <QSignalSpy>
#include <QTest>
#include <QTimeZone>

#include <algorithm>
#include <optional>
#include <utility>

using smartmate::model::Task;
using smartmate::model::TaskCategory;
using smartmate::model::TaskCategoryColor;
using smartmate::model::TaskCategoryId;
using smartmate::model::TaskCreationRequest;
using smartmate::model::TaskDraft;
using smartmate::model::TaskError;
using smartmate::model::TaskId;
using smartmate::model::TaskPriority;
using smartmate::model::TaskService;
using smartmate::model::TaskStatus;
using smartmate::model::TaskTransition;
using smartmate::tests::FakeTaskDependencyRepository;
using smartmate::tests::FakeTaskCreationRepository;
using smartmate::tests::FakeTaskBatchTransitionRepository;
using smartmate::tests::FakeTaskDeletionRepository;
using smartmate::tests::FakeTaskCategoryRepository;
using smartmate::tests::FakeTaskRepository;

namespace {

// 与删除无关的测试共享无状态端口；删除语义测试使用各自绑定Repository的实例。
FakeTaskDeletionRepository deletionRepository;
FakeTaskCategoryRepository categoryRepository;

[[nodiscard]] QDateTime timestamp(qint64 milliseconds = 1700000000000)
{
    return QDateTime::fromMSecsSinceEpoch(milliseconds, QTimeZone::UTC);
}

[[nodiscard]] Task storedTask(TaskStatus status = TaskStatus::Todo,
                              std::optional<TaskStatus> statusBeforeArchive = std::nullopt,
                              QString title = QStringLiteral("Stored task"))
{
    return Task{QUuid::createUuid(),
                std::move(title),
                QStringLiteral("Stored description"),
                TaskPriority::Normal,
                status,
                statusBeforeArchive,
                std::nullopt,
                30,
                timestamp(),
                timestamp()};
}

[[nodiscard]] TaskDraft validDraft()
{
    TaskDraft draft;
    draft.title = QStringLiteral("Valid task");
    draft.description = QStringLiteral("Description");
    return draft;
}

[[nodiscard]] TaskDraft draftFor(const Task &task)
{
    return {task.title(),
            task.description(),
            task.priority(),
            task.deadline(),
            task.estimatedMinutes()};
}

} // namespace

// 在不启动 SQLite 或 QML 的情况下验证任务规则、错误映射和成功写入通知。
class TaskServiceTest final : public QObject {
    Q_OBJECT

private slots:
    void listsAndFindsTasks();
    void createsTaskWithEveryField();
    void createsTaskAndPredecessorsAtomically();
    void rejectsInvalidCreationPredecessorsWithoutWriting();
    void rollsBackAtomicCreationFailureWithoutSignals();
    void listsEligibleCreationPredecessors();
    void normalizesOmittedDescriptionToEmptyText();
    void validatesDraftWithoutRepositoryAccess();
    void validatesDraftFields();
    void validatesUnicodeTextBoundaries();
    void acceptsDeadlineAndEstimateBoundaries();
    void createsEveryPriorityAsTodo();
    void startEnforcesSingleInProgressTask();
    void mapsConcurrentCreationFailureWithoutInventingStatusConflict();
    void mapsConcurrentInProgressStartConflict();
    void restoresLegacyInProgressArchiveAsTodo();
    void updatesTaskAndPreservesIdentity();
    void rejectsEditingNonTodoTasks();
    void permanentlyDeletesOnlyArchivedTasks();
    void mapsPermanentDeletionFailuresWithoutSignals();
    void rejectsEmptyAndInvalidBatchSelectionsWithoutWriting();
    void archivesAndRestoresBatchesAtomically();
    void aggregatesBatchRestoreDependencyFailures();
    void permanentlyDeletesArchivedBatchAndSharedEdgesOnce();
    void archivesAndRestoresTerminalStatuses();
    void restoresLegacyArchiveWithoutInProgressConflict();
    void reportsInvalidOperationsAndMissingTasks();
    void mapsRepositoryFailures();
    void replacesDependenciesAndReportsStructuredErrors();
    void enforcesDependencyStatusConsistency();
    void mapsDependencyRepositoryFailures();
    void buildsGraphSnapshotWithArchivedClosure();
    void buildsDependencyEditContextInModel();
    void rejectsDependencyEditContextForNonTodoTarget();
    void keepsPlanAndGraphCommandAvailabilityConsistent();
    void recordsImmutableActivityEventForEverySuccessfulTransition();
};

void TaskServiceTest::listsAndFindsTasks()
{
    const Task first = storedTask(TaskStatus::InProgress, std::nullopt,
                                  QStringLiteral("Understand MVVM"));
    const Task second = storedTask(TaskStatus::Todo, std::nullopt,
                                   QStringLiteral("Build SmartMate"));
    FakeTaskRepository repository{{first, second}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    const TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};

    const auto listResult = service.listTasks();
    QVERIFY(listResult.ok());
    QCOMPARE(listResult.value->size(), 2);
    QCOMPARE(listResult.value->at(0), first);
    QCOMPARE(listResult.value->at(1), second);

    const auto findResult = service.findTask(second.id());
    QVERIFY(findResult.ok());
    QCOMPARE(*findResult.value, second);
}

void TaskServiceTest::createsTaskWithEveryField()
{
    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};
    QSignalSpy dependencySpy{&service, &TaskService::dependenciesChanged};
    const QDateTime localDeadline =
        QDateTime::fromString(QStringLiteral("2027-02-03T14:30:00+08:00"), Qt::ISODate);

    TaskDraft draft;
    draft.title = QStringLiteral("  完成大作业  ");
    draft.description = QStringLiteral("实现任务模块的完整纵向链路");
    draft.priority = TaskPriority::Urgent;
    draft.deadline = localDeadline;
    draft.estimatedMinutes = 180;

    const auto result = service.createTask(draft);

    QVERIFY(result.ok());
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(dependencySpy.count(), 0);
    QCOMPARE(repository.tasks().size(), 1);
    const Task &created = *result.value;
    QVERIFY(!created.id().isNull());
    QCOMPARE(created.title(), QStringLiteral("完成大作业"));
    QCOMPARE(created.description(), draft.description);
    QCOMPARE(created.priority(), TaskPriority::Urgent);
    QCOMPARE(created.status(), TaskStatus::Todo);
    QVERIFY(!created.statusBeforeArchive().has_value());
    QCOMPARE(created.deadline(), std::optional<QDateTime>{localDeadline.toUTC()});
    QCOMPARE(created.estimatedMinutes(), std::optional<int>{180});
    QVERIFY(created.createdAtUtc().isValid());
    QCOMPARE(created.createdAtUtc().timeSpec(), Qt::UTC);
    QCOMPARE(created.updatedAtUtc(), created.createdAtUtc());
    QCOMPARE(repository.tasks().constFirst(), created);
}

void TaskServiceTest::createsTaskAndPredecessorsAtomically()
{
    const Task first = storedTask(TaskStatus::Done, std::nullopt,
                                  QStringLiteral("Completed predecessor"));
    const Task second = storedTask(TaskStatus::Todo, std::nullopt,
                                   QStringLiteral("Pending predecessor"));
    FakeTaskRepository repository{{first, second}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};
    QSignalSpy taskSpy{&service, &TaskService::tasksChanged};
    QSignalSpy dependencySpy{&service, &TaskService::dependenciesChanged};

    const TaskCreationRequest request{
        validDraft(),
        {second.id(), first.id()}};
    const auto result = service.createTask(request);

    QVERIFY(result.ok());
    QCOMPARE(creationRepository.insertCount(), 1);
    QCOMPARE(repository.tasks().size(), 3);
    QCOMPARE(taskSpy.count(), 1);
    QCOMPARE(dependencySpy.count(), 1);
    QCOMPARE(dependencyRepository.dependencies().size(), 2);
    QVERIFY(dependencyRepository.dependencies().contains(
        {first.id(), result.value->id()}));
    QVERIFY(dependencyRepository.dependencies().contains(
        {second.id(), result.value->id()}));
}

void TaskServiceTest::rejectsInvalidCreationPredecessorsWithoutWriting()
{
    const Task active = storedTask(TaskStatus::Todo);
    const Task archived = storedTask(TaskStatus::Archived, TaskStatus::Done);
    const Task cancelled = storedTask(TaskStatus::Cancelled);
    FakeTaskRepository repository{{active, archived, cancelled}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};
    QSignalSpy taskSpy{&service, &TaskService::tasksChanged};
    QSignalSpy dependencySpy{&service, &TaskService::dependenciesChanged};

    TaskCreationRequest request{validDraft(), {active.id(), active.id()}};
    QCOMPARE(service.createTask(request).error, TaskError::DependencyDuplicate);

    const TaskId missingId = QUuid::createUuid();
    request.predecessorIds = {missingId};
    const auto missing = service.createTask(request);
    QCOMPARE(missing.error, TaskError::DependencyEndpointNotFound);
    QCOMPARE(missing.context.conflictingTaskIds, QList<smartmate::model::TaskId>{missingId});

    request.predecessorIds = {archived.id()};
    QCOMPARE(service.createTask(request).error,
             TaskError::DependencyPredecessorNotEligible);

    request.predecessorIds = {cancelled.id()};
    QCOMPARE(service.createTask(request).error,
             TaskError::DependencyPredecessorNotEligible);

    QCOMPARE(creationRepository.insertCount(), 0);
    QCOMPARE(repository.tasks().size(), 3);
    QVERIFY(dependencyRepository.dependencies().isEmpty());
    QCOMPARE(taskSpy.count(), 0);
    QCOMPARE(dependencySpy.count(), 0);
}

void TaskServiceTest::rollsBackAtomicCreationFailureWithoutSignals()
{
    const Task predecessor = storedTask(TaskStatus::Todo);
    FakeTaskRepository repository{{predecessor}};
    FakeTaskDependencyRepository dependencyRepository;
    dependencyRepository.setWriteFailure(true);
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};
    QSignalSpy taskSpy{&service, &TaskService::tasksChanged};
    QSignalSpy dependencySpy{&service, &TaskService::dependenciesChanged};

    const auto result = service.createTask(
        TaskCreationRequest{validDraft(), {predecessor.id()}});

    QCOMPARE(result.error, TaskError::PersistenceFailure);
    QCOMPARE(repository.tasks(), QList<Task>{predecessor});
    QVERIFY(dependencyRepository.dependencies().isEmpty());
    QCOMPARE(creationRepository.insertCount(), 0);
    QCOMPARE(taskSpy.count(), 0);
    QCOMPARE(dependencySpy.count(), 0);
}

void TaskServiceTest::listsEligibleCreationPredecessors()
{
    const Task inProgress = storedTask(TaskStatus::InProgress);
    const Task todo = storedTask(TaskStatus::Todo);
    const Task done = storedTask(TaskStatus::Done);
    const Task cancelled = storedTask(TaskStatus::Cancelled);
    const Task archived = storedTask(TaskStatus::Archived, TaskStatus::Todo);
    FakeTaskRepository repository{{archived, cancelled, todo, done, inProgress}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    const TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};

    const auto result = service.listEligibleCreationPredecessors();

    QVERIFY(result.ok());
    QCOMPARE(result.value->size(), 3);
    QCOMPARE(result.value->constFirst().id(), inProgress.id());
    QVERIFY(std::none_of(result.value->cbegin(), result.value->cend(),
                         [&archived](const Task &task) {
        return task.id() == archived.id();
    }));
    QVERIFY(std::none_of(result.value->cbegin(), result.value->cend(),
                         [&cancelled](const Task &task) {
        return task.id() == cancelled.id();
    }));
}

void TaskServiceTest::normalizesOmittedDescriptionToEmptyText()
{
    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};
    TaskDraft draft;
    draft.title = QStringLiteral("只填写标题");
    // 模拟旧调用方或未触碰描述输入框时传入的 null QString。
    draft.description = QString{};

    const auto result = service.createTask(draft);

    QVERIFY(result.ok());
    QVERIFY(result.value->description().isEmpty());
    QVERIFY(!result.value->description().isNull());
    QCOMPARE(repository.tasks().size(), 1);
}

void TaskServiceTest::validatesDraftWithoutRepositoryAccess()
{
    FakeTaskRepository repository;
    repository.setReadFailure(true);
    repository.setWriteFailure(true);
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};

    TaskDraft draft = validDraft();
    draft.title = QStringLiteral("   ");
    const auto emptyTitleResult = service.validateDraft(draft);
    QVERIFY(!emptyTitleResult.ok());
    QCOMPARE(emptyTitleResult.error, TaskError::EmptyTitle);
    QVERIFY(!emptyTitleResult.detail.isEmpty());

    draft = validDraft();
    draft.estimatedMinutes = 525600;
    const auto maximumEstimateResult = service.validateDraft(draft);
    QVERIFY(maximumEstimateResult.ok());
    QCOMPARE(maximumEstimateResult.error, TaskError::None);
    QVERIFY(maximumEstimateResult.detail.isEmpty());

    draft.estimatedMinutes = 525601;
    const auto excessiveEstimateResult = service.validateDraft(draft);
    QVERIFY(!excessiveEstimateResult.ok());
    QCOMPARE(excessiveEstimateResult.error, TaskError::InvalidEstimate);
    QVERIFY(!excessiveEstimateResult.detail.isEmpty());

    QCOMPARE(changedSpy.count(), 0);
    QVERIFY(repository.tasks().isEmpty());
}

void TaskServiceTest::validatesDraftFields()
{
    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};

    const auto expectError = [&service](TaskDraft draft, TaskError expected) {
        const auto result = service.createTask(draft);
        QVERIFY(!result.ok());
        QCOMPARE(result.error, expected);
        QVERIFY(!result.detail.isEmpty());
    };

    TaskDraft draft = validDraft();
    draft.title = QStringLiteral("   ");
    expectError(draft, TaskError::EmptyTitle);

    draft = validDraft();
    draft.title = QString(201, QLatin1Char('a'));
    expectError(draft, TaskError::TitleTooLong);

    draft = validDraft();
    draft.description = QString(5001, QLatin1Char('a'));
    expectError(draft, TaskError::DescriptionTooLong);

    draft = validDraft();
    draft.deadline = QDateTime{};
    expectError(draft, TaskError::InvalidDeadline);

    draft = validDraft();
    draft.estimatedMinutes = 0;
    expectError(draft, TaskError::InvalidEstimate);

    draft.estimatedMinutes = 525601;
    expectError(draft, TaskError::InvalidEstimate);

    draft = validDraft();
    draft.priority = static_cast<TaskPriority>(999);
    expectError(draft, TaskError::InvalidPriority);

    QCOMPARE(changedSpy.count(), 0);
    QVERIFY(repository.tasks().isEmpty());
}

void TaskServiceTest::validatesUnicodeTextBoundaries()
{
    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    const TaskService service{repository, dependencyRepository, creationRepository,
                              batchTransitionRepository, deletionRepository,
                              categoryRepository};
    const QString emoji = QString::fromUcs4(U"😀");

    TaskDraft draft = validDraft();
    draft.title = emoji.repeated(
        smartmate::model::TaskConstraints::maximumTitleLength);
    QVERIFY(service.validateDraft(draft).ok());

    draft.title = emoji.repeated(
        smartmate::model::TaskConstraints::maximumTitleLength + 1);
    QCOMPARE(service.validateDraft(draft).error, TaskError::TitleTooLong);

    draft = validDraft();
    draft.description = emoji.repeated(
        smartmate::model::TaskConstraints::maximumDescriptionLength);
    QVERIFY(service.validateDraft(draft).ok());

    draft.description = emoji.repeated(
        smartmate::model::TaskConstraints::maximumDescriptionLength + 1);
    QCOMPARE(service.validateDraft(draft).error, TaskError::DescriptionTooLong);
}

void TaskServiceTest::acceptsDeadlineAndEstimateBoundaries()
{
    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    const TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};
    TaskDraft draft = validDraft();

    // 截止时间表达约束而不是输入格式约束，因此过去的有效时刻仍可保存。
    draft.deadline = timestamp(0);
    draft.estimatedMinutes = smartmate::model::TaskConstraints::minimumEstimatedMinutes;
    QVERIFY(service.validateDraft(draft).ok());

    draft.estimatedMinutes = smartmate::model::TaskConstraints::maximumEstimatedMinutes;
    QVERIFY(service.validateDraft(draft).ok());

    draft.estimatedMinutes = 0;
    QCOMPARE(service.validateDraft(draft).error, TaskError::InvalidEstimate);

    draft.estimatedMinutes =
        smartmate::model::TaskConstraints::maximumEstimatedMinutes + 1;
    QCOMPARE(service.validateDraft(draft).error, TaskError::InvalidEstimate);
}

void TaskServiceTest::createsEveryPriorityAsTodo()
{
    const QList<TaskPriority> priorities{TaskPriority::Low,
                                         TaskPriority::Normal,
                                         TaskPriority::High,
                                         TaskPriority::Urgent};

    for (const TaskPriority priority : priorities) {
        FakeTaskRepository repository;
        FakeTaskDependencyRepository dependencyRepository;
        FakeTaskCreationRepository creationRepository{repository,
                                                       dependencyRepository};
        FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
        TaskService service{repository, dependencyRepository,
                            creationRepository, batchTransitionRepository,
                            deletionRepository, categoryRepository};
        TaskDraft draft = validDraft();
        draft.priority = priority;

        const auto result = service.createTask(draft);

        QVERIFY(result.ok());
        QCOMPARE(result.value->status(), TaskStatus::Todo);
        QCOMPARE(result.value->priority(), priority);
        QVERIFY(!result.value->statusBeforeArchive().has_value());
    }
}

void TaskServiceTest::startEnforcesSingleInProgressTask()
{
    const Task active = storedTask(TaskStatus::InProgress);
    const Task todo = storedTask(TaskStatus::Todo);
    FakeTaskRepository repository{{active, todo}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};

    const auto startResult = service.startTask(todo.id());
    QCOMPARE(startResult.error, TaskError::InProgressConflict);

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(repository.tasks().size(), 2);
    QCOMPARE(repository.findById(todo.id())->status(), TaskStatus::Todo);
}

void TaskServiceTest::mapsConcurrentCreationFailureWithoutInventingStatusConflict()
{
    // 模拟业务预检通过后，另一执行方抢先写入进行中任务。
    FakeTaskRepository repository;
    repository.setCompetingTaskOnNextWrite(storedTask(TaskStatus::InProgress));
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};
    TaskDraft draft = validDraft();

    const auto result = service.createTask(draft);

    QCOMPARE(result.error, TaskError::PersistenceFailure);
    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(repository.tasks().size(), 1);
    QCOMPARE(repository.tasks().constFirst().status(), TaskStatus::InProgress);
}

void TaskServiceTest::mapsConcurrentInProgressStartConflict()
{
    const Task target = storedTask(TaskStatus::Todo);
    FakeTaskRepository repository{{target}};
    repository.setCompetingTaskOnNextWrite(storedTask(TaskStatus::InProgress));
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};
    const auto result = service.startTask(target.id());

    QCOMPARE(result.error, TaskError::InProgressConflict);
    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(repository.findById(target.id())->status(), TaskStatus::Todo);
    QCOMPARE(repository.tasks().size(), 2);
}

void TaskServiceTest::restoresLegacyInProgressArchiveAsTodo()
{
    const Task archived = storedTask(TaskStatus::Archived, TaskStatus::InProgress);
    const Task active = storedTask(TaskStatus::InProgress);
    FakeTaskRepository repository{{archived, active}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};

    const auto result = service.restoreTask(archived.id());

    QVERIFY(result.ok());
    QCOMPARE(result.value->status(), TaskStatus::Todo);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(repository.findById(archived.id())->status(), TaskStatus::Todo);
    QCOMPARE(repository.tasks().size(), 2);
}

void TaskServiceTest::updatesTaskAndPreservesIdentity()
{
    const Task original = storedTask();
    FakeTaskRepository repository{{original}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};

    TaskDraft draft;
    draft.title = QStringLiteral("  Updated title ");
    draft.description = QStringLiteral("Updated description");
    draft.priority = TaskPriority::High;
    draft.deadline = QDateTime::fromString(QStringLiteral("2028-01-02T12:00:00+02:00"),
                                           Qt::ISODate);
    draft.estimatedMinutes = 75;

    const auto result = service.updateTask(original.id(), draft);

    QVERIFY(result.ok());
    QCOMPARE(changedSpy.count(), 1);
    const Task &updated = *result.value;
    QCOMPARE(updated.id(), original.id());
    QCOMPARE(updated.createdAtUtc(), original.createdAtUtc());
    QVERIFY(updated.updatedAtUtc() >= original.updatedAtUtc());
    QCOMPARE(updated.title(), QStringLiteral("Updated title"));
    QCOMPARE(updated.description(), draft.description);
    QCOMPARE(updated.priority(), TaskPriority::High);
    QCOMPARE(updated.status(), TaskStatus::Todo);
    QCOMPARE(updated.deadline(), std::optional<QDateTime>{draft.deadline->toUTC()});
    QCOMPARE(updated.estimatedMinutes(), std::optional<int>{75});
    QCOMPARE(repository.findById(original.id()), result.value);
}

void TaskServiceTest::rejectsEditingNonTodoTasks()
{
    // 普通字段只允许Todo修改，其他状态必须先通过合法状态命令回到Todo。
    const QList<TaskStatus> statuses{TaskStatus::Todo,
                                     TaskStatus::InProgress,
                                     TaskStatus::Done,
                                     TaskStatus::Cancelled,
                                     TaskStatus::Archived};
    for (const TaskStatus status : statuses) {
        const Task candidate = storedTask(
            status,
            status == TaskStatus::Archived
                ? std::optional<TaskStatus>{TaskStatus::Done}
                : std::nullopt);
        QCOMPARE(candidate.canEditDetails(), status == TaskStatus::Todo);

        FakeTaskRepository repository{{candidate}};
        FakeTaskDependencyRepository dependencyRepository;
        FakeTaskCreationRepository creationRepository{repository,
                                                       dependencyRepository};
        FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
        TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};
        QSignalSpy changedSpy{&service, &TaskService::tasksChanged};
        TaskDraft draft = draftFor(candidate);
        draft.title = QStringLiteral("尝试修改普通字段");

        const auto findResult = service.findEditableTask(candidate.id());
        const auto updateResult = service.updateTask(candidate.id(), draft);
        if (status == TaskStatus::Todo) {
            QVERIFY(findResult.ok());
            QVERIFY(updateResult.ok());
            QCOMPARE(changedSpy.count(), 1);
        } else {
            QCOMPARE(findResult.error, TaskError::TaskDetailsNotEditable);
            QCOMPARE(updateResult.error, TaskError::TaskDetailsNotEditable);
            QCOMPARE(findResult.context.conflictingTaskIds,
                     QList<TaskId>{candidate.id()});
            QCOMPARE(repository.tasks(), QList<Task>{candidate});
            QCOMPARE(repository.updateCount(), 0);
            QCOMPARE(changedSpy.count(), 0);
        }
    }

    // 编辑器打开后状态可能被其他命令改变；保存时必须重新读取并拒绝陈旧草稿。
    const Task openedTodo = storedTask(TaskStatus::Todo);
    FakeTaskRepository staleRepository{{openedTodo}};
    FakeTaskDependencyRepository staleDependencies;
    FakeTaskCreationRepository staleCreation{staleRepository,
                                              staleDependencies};
    FakeTaskBatchTransitionRepository staleBatchTransition{staleRepository};
    TaskService staleService{staleRepository, staleDependencies, staleCreation,
                             staleBatchTransition, deletionRepository,
                             categoryRepository};
    QVERIFY(staleService.findEditableTask(openedTodo.id()).ok());
    QVERIFY(staleService.startTask(openedTodo.id()).ok());
    const int writesBeforeSave = staleRepository.updateCount();
    QSignalSpy staleChangedSpy{&staleService, &TaskService::tasksChanged};
    TaskDraft staleDraft = draftFor(openedTodo);
    staleDraft.title = QStringLiteral("陈旧编辑草稿");

    const auto staleSave = staleService.updateTask(openedTodo.id(), staleDraft);
    QCOMPARE(staleSave.error, TaskError::TaskDetailsNotEditable);
    QCOMPARE(staleRepository.updateCount(), writesBeforeSave);
    QCOMPARE(staleChangedSpy.count(), 0);
}

void TaskServiceTest::permanentlyDeletesOnlyArchivedTasks()
{
    const Task first = storedTask(TaskStatus::Todo, std::nullopt,
                                  QStringLiteral("保留任务A"));
    const Task archived = storedTask(TaskStatus::Archived, TaskStatus::Done,
                                     QStringLiteral("永久删除目标"));
    const Task second = storedTask(TaskStatus::Todo, std::nullopt,
                                   QStringLiteral("保留任务B"));
    FakeTaskRepository repository{{first, archived, second}};
    FakeTaskDependencyRepository dependencyRepository{
        {{first.id(), archived.id()},
         {archived.id(), second.id()},
         {first.id(), second.id()}}};
    FakeTaskCreationRepository creationRepository{repository,
                                                   dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    FakeTaskDeletionRepository boundDeletionRepository{repository,
                                                       dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, boundDeletionRepository,
                        categoryRepository};
    QSignalSpy taskSpy{&service, &TaskService::tasksChanged};
    QSignalSpy dependencySpy{&service, &TaskService::dependenciesChanged};

    const auto result = service.deleteArchivedTask(archived.id());

    QVERIFY(result.ok());
    QCOMPARE(*result.value, archived);
    QVERIFY(!repository.findById(archived.id()).has_value());
    QCOMPARE(repository.tasks().size(), 2);
    const QList<smartmate::model::TaskDependency> expectedDependencies{
        {first.id(), second.id()}};
    QCOMPARE(dependencyRepository.dependencies(), expectedDependencies);
    QCOMPARE(boundDeletionRepository.deletedTaskIds(),
             QList<TaskId>{archived.id()});
    QCOMPARE(taskSpy.count(), 1);
    QCOMPARE(dependencySpy.count(), 1);

    // 永久删除资格属于Model：四种活动状态必须在访问删除端口前被拒绝。
    for (const TaskStatus status : {TaskStatus::Todo,
                                    TaskStatus::InProgress,
                                    TaskStatus::Done,
                                    TaskStatus::Cancelled}) {
        const Task active = storedTask(status);
        FakeTaskRepository activeRepository{{active}};
        FakeTaskDependencyRepository activeDependencies;
        FakeTaskCreationRepository activeCreation{activeRepository,
                                                   activeDependencies};
        FakeTaskBatchTransitionRepository activeBatchTransition{activeRepository};
        FakeTaskDeletionRepository activeDeletion{activeRepository,
                                                   activeDependencies};
        TaskService activeService{activeRepository, activeDependencies,
                                  activeCreation, activeBatchTransition,
                                  activeDeletion, categoryRepository};
        QSignalSpy activeTaskSpy{&activeService, &TaskService::tasksChanged};

        const auto rejected = activeService.deleteArchivedTask(active.id());
        QCOMPARE(rejected.error, TaskError::TaskDeletionNotAllowed);
        QVERIFY(activeDeletion.deletedTaskIds().isEmpty());
        QCOMPARE(activeRepository.tasks(), QList<Task>{active});
        QCOMPARE(activeTaskSpy.count(), 0);
    }

    const Task noEdgeArchived = storedTask(TaskStatus::Archived,
                                           TaskStatus::Cancelled);
    FakeTaskRepository noEdgeRepository{{noEdgeArchived}};
    FakeTaskDependencyRepository noEdgeDependencies;
    FakeTaskCreationRepository noEdgeCreation{noEdgeRepository,
                                              noEdgeDependencies};
    FakeTaskBatchTransitionRepository noEdgeBatchTransition{noEdgeRepository};
    FakeTaskDeletionRepository noEdgeDeletion{noEdgeRepository,
                                              noEdgeDependencies};
    TaskService noEdgeService{noEdgeRepository, noEdgeDependencies,
                              noEdgeCreation, noEdgeBatchTransition,
                              noEdgeDeletion, categoryRepository};
    QSignalSpy noEdgeTaskSpy{&noEdgeService, &TaskService::tasksChanged};
    QSignalSpy noEdgeDependencySpy{&noEdgeService,
                                   &TaskService::dependenciesChanged};
    QVERIFY(noEdgeService.deleteArchivedTask(noEdgeArchived.id()).ok());
    QCOMPARE(noEdgeTaskSpy.count(), 1);
    QCOMPARE(noEdgeDependencySpy.count(), 0);
}

void TaskServiceTest::mapsPermanentDeletionFailuresWithoutSignals()
{
    const Task archived = storedTask(TaskStatus::Archived, TaskStatus::Done);
    FakeTaskRepository repository{{archived}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository,
                                                   dependencyRepository};
    FakeTaskDeletionRepository deletionFailure{repository,
                                                dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    deletionFailure.setWriteFailure(true);
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionFailure,
                        categoryRepository};
    QSignalSpy taskSpy{&service, &TaskService::tasksChanged};
    QSignalSpy dependencySpy{&service, &TaskService::dependenciesChanged};

    const auto persistenceFailure = service.deleteArchivedTask(archived.id());
    QCOMPARE(persistenceFailure.error, TaskError::PersistenceFailure);
    QCOMPARE(repository.tasks(), QList<Task>{archived});
    QCOMPARE(taskSpy.count(), 0);
    QCOMPARE(dependencySpy.count(), 0);

    deletionFailure.setWriteFailure(false);
    deletionFailure.setResult({false, 0});
    const auto inconsistentResult = service.deleteArchivedTask(archived.id());
    QCOMPARE(inconsistentResult.error, TaskError::PersistenceFailure);
    QCOMPARE(taskSpy.count(), 0);

    const auto missing = service.deleteArchivedTask(QUuid::createUuid());
    QCOMPARE(missing.error, TaskError::NotFound);
    QCOMPARE(deletionFailure.deletedTaskIds().size(), 1);
    QCOMPARE(taskSpy.count(), 0);

    // 条件删除回滚后重读：预检后被另一实例删除必须报告NotFound，而非状态不允许。
    const Task disappeared = storedTask(TaskStatus::Archived, TaskStatus::Done);
    FakeTaskRepository disappearedRepository{{disappeared}};
    FakeTaskDependencyRepository disappearedDependencies;
    FakeTaskCreationRepository disappearedCreation{disappearedRepository,
                                                     disappearedDependencies};
    FakeTaskBatchTransitionRepository disappearedTransitions{
        disappearedRepository};
    FakeTaskDeletionRepository disappearedDeletion{disappearedRepository,
                                                    disappearedDependencies};
    disappearedDeletion.setMissingConflictOnNextWrite(true);
    TaskService disappearedService{disappearedRepository,
                                   disappearedDependencies,
                                   disappearedCreation,
                                   disappearedTransitions,
                                   disappearedDeletion,
                                   categoryRepository};
    QSignalSpy disappearedTaskSpy{&disappearedService,
                                  &TaskService::tasksChanged};

    const auto disappearedResult =
        disappearedService.deleteArchivedTask(disappeared.id());
    QCOMPARE(disappearedResult.error, TaskError::NotFound);
    QCOMPARE(disappearedResult.context.conflictingTaskIds,
             QList<TaskId>{disappeared.id()});
    QCOMPARE(disappearedTaskSpy.count(), 0);
}

void TaskServiceTest::rejectsEmptyAndInvalidBatchSelectionsWithoutWriting()
{
    const Task done = storedTask(TaskStatus::Done);
    const Task todo = storedTask(TaskStatus::Todo);
    FakeTaskRepository repository{{done, todo}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository,
                                                   dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    FakeTaskDeletionRepository batchDeletionRepository{repository,
                                                       dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, batchDeletionRepository,
                        categoryRepository};
    QSignalSpy taskSpy{&service, &TaskService::tasksChanged};

    QCOMPARE(service.archiveTasks({}).error, TaskError::EmptyTaskSelection);
    QCOMPARE(service.restoreTasks({}).error, TaskError::EmptyTaskSelection);
    QCOMPARE(service.deleteArchivedTasks({}).error,
             TaskError::EmptyTaskSelection);

    const auto invalid = service.archiveTasks({done.id(), todo.id()});
    QCOMPARE(invalid.error, TaskError::InvalidTaskTransition);
    QCOMPARE(invalid.context.conflictingTaskIds, QList<TaskId>{todo.id()});

    const TaskId missingId = QUuid::createUuid();
    const auto missing = service.archiveTasks({done.id(), missingId});
    QCOMPARE(missing.error, TaskError::NotFound);
    QCOMPARE(missing.context.conflictingTaskIds, QList<TaskId>{missingId});
    QCOMPARE(batchTransitionRepository.callCount(), 0);
    QVERIFY(batchDeletionRepository.deletedTaskIds().isEmpty());
    QCOMPARE(repository.tasks(), QList<Task>({done, todo}));
    QCOMPARE(taskSpy.count(), 0);

    batchTransitionRepository.setResult({0, {done.id()}});
    const auto concurrent = service.archiveTasks({done.id()});
    QCOMPARE(concurrent.error, TaskError::InvalidTaskTransition);
    QCOMPARE(concurrent.context.conflictingTaskIds, QList<TaskId>{done.id()});
    QCOMPARE(repository.tasks(), QList<Task>({done, todo}));
    QCOMPARE(taskSpy.count(), 0);
}

void TaskServiceTest::archivesAndRestoresBatchesAtomically()
{
    const Task done = storedTask(TaskStatus::Done);
    const Task cancelled = storedTask(TaskStatus::Cancelled);
    FakeTaskRepository repository{{done, cancelled}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository,
                                                   dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    FakeTaskDeletionRepository batchDeletionRepository{repository,
                                                       dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, batchDeletionRepository,
                        categoryRepository};
    QSignalSpy taskSpy{&service, &TaskService::tasksChanged};

    const auto archived = service.archiveTasks(
        {cancelled.id(), done.id(), cancelled.id()});
    QVERIFY(archived.ok());
    QCOMPARE(archived.value->tasks.size(), 2);
    QVERIFY(archived.value->tasks.at(0).id().toString(QUuid::WithoutBraces)
            < archived.value->tasks.at(1).id().toString(QUuid::WithoutBraces));
    QCOMPARE(batchTransitionRepository.callCount(), 1);
    QCOMPARE(batchTransitionRepository.lastChanges().size(), 2);
    QCOMPARE(batchTransitionRepository.lastChanges().at(0).updatedAtUtc,
             batchTransitionRepository.lastChanges().at(1).updatedAtUtc);
    QCOMPARE(taskSpy.count(), 1);
    for (const Task &task : archived.value->tasks) {
        QCOMPARE(task.status(), TaskStatus::Archived);
        const TaskStatus expectedBefore = task.id() == done.id()
            ? TaskStatus::Done
            : TaskStatus::Cancelled;
        QCOMPARE(task.statusBeforeArchive(),
                 std::optional<TaskStatus>{expectedBefore});
    }

    const auto restored = service.restoreTasks({done.id(), cancelled.id()});
    QVERIFY(restored.ok());
    QCOMPARE(restored.value->tasks.size(), 2);
    QCOMPARE(taskSpy.count(), 2);
    QCOMPARE(repository.findById(done.id())->status(), TaskStatus::Done);
    QCOMPARE(repository.findById(cancelled.id())->status(),
             TaskStatus::Cancelled);

    const Task legacy = storedTask(TaskStatus::Archived,
                                   TaskStatus::InProgress);
    FakeTaskRepository legacyRepository{{legacy}};
    FakeTaskDependencyRepository legacyDependencies;
    FakeTaskCreationRepository legacyCreation{legacyRepository,
                                               legacyDependencies};
    FakeTaskBatchTransitionRepository legacyBatch{legacyRepository};
    FakeTaskDeletionRepository legacyDeletion{legacyRepository,
                                              legacyDependencies};
    TaskService legacyService{legacyRepository, legacyDependencies,
                              legacyCreation, legacyBatch, legacyDeletion,
                              categoryRepository};
    const auto legacyRestore = legacyService.restoreTasks({legacy.id()});
    QVERIFY(legacyRestore.ok());
    QCOMPARE(legacyRestore.value->tasks.constFirst().status(), TaskStatus::Todo);
}

void TaskServiceTest::aggregatesBatchRestoreDependencyFailures()
{
    const Task firstPredecessor = storedTask(TaskStatus::Todo);
    const Task secondPredecessor = storedTask(TaskStatus::Todo);
    const Task firstBlocked = storedTask(TaskStatus::Archived, TaskStatus::Done);
    const Task secondBlocked = storedTask(TaskStatus::Archived, TaskStatus::Done);
    FakeTaskRepository repository{
        {firstPredecessor, secondPredecessor, firstBlocked, secondBlocked}};
    FakeTaskDependencyRepository dependencyRepository{
        {{firstPredecessor.id(), firstBlocked.id()},
         {secondPredecessor.id(), secondBlocked.id()}}};
    FakeTaskCreationRepository creationRepository{repository,
                                                   dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    FakeTaskDeletionRepository deletionRepository{repository,
                                                  dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};
    QSignalSpy taskSpy{&service, &TaskService::tasksChanged};

    const auto result = service.restoreTasks(
        {secondBlocked.id(), firstBlocked.id()});
    QCOMPARE(result.error, TaskError::TaskBlocked);
    QCOMPARE(result.context.blockingTaskIds.size(), 2);
    QVERIFY(result.context.blockingTaskIds.contains(firstPredecessor.id()));
    QVERIFY(result.context.blockingTaskIds.contains(secondPredecessor.id()));
    QCOMPARE(result.context.conflictingTaskIds.size(), 2);
    QVERIFY(result.context.conflictingTaskIds.contains(firstBlocked.id()));
    QVERIFY(result.context.conflictingTaskIds.contains(secondBlocked.id()));
    QCOMPARE(batchTransitionRepository.callCount(), 0);
    QCOMPARE(taskSpy.count(), 0);
}

void TaskServiceTest::permanentlyDeletesArchivedBatchAndSharedEdgesOnce()
{
    const Task retainedA = storedTask(TaskStatus::Todo);
    const Task archivedA = storedTask(TaskStatus::Archived, TaskStatus::Done);
    const Task archivedB = storedTask(TaskStatus::Archived,
                                      TaskStatus::Cancelled);
    const Task retainedB = storedTask(TaskStatus::Todo);
    FakeTaskRepository repository{{retainedA, archivedA, archivedB, retainedB}};
    FakeTaskDependencyRepository dependencyRepository{
        {{retainedA.id(), archivedA.id()},
         {archivedA.id(), archivedB.id()},
         {archivedB.id(), retainedB.id()},
         {retainedA.id(), retainedB.id()}}};
    FakeTaskCreationRepository creationRepository{repository,
                                                   dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    FakeTaskDeletionRepository deletionRepository{repository,
                                                  dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};
    QSignalSpy taskSpy{&service, &TaskService::tasksChanged};
    QSignalSpy dependencySpy{&service, &TaskService::dependenciesChanged};

    const auto result = service.deleteArchivedTasks(
        {archivedB.id(), archivedA.id(), archivedA.id()});
    QVERIFY(result.ok());
    QCOMPARE(result.value->tasks.size(), 2);
    QCOMPARE(result.value->removedDependencyCount, 3);
    QVERIFY(!repository.findById(archivedA.id()).has_value());
    QVERIFY(!repository.findById(archivedB.id()).has_value());
    QCOMPARE(repository.tasks(), QList<Task>({retainedA, retainedB}));
    QCOMPARE(dependencyRepository.dependencies(),
             QList<smartmate::model::TaskDependency>(
                 {{retainedA.id(), retainedB.id()}}));
    QCOMPARE(taskSpy.count(), 1);
    QCOMPARE(dependencySpy.count(), 1);

    const auto invalid = service.deleteArchivedTasks({retainedA.id()});
    QCOMPARE(invalid.error, TaskError::TaskDeletionNotAllowed);
    QCOMPARE(taskSpy.count(), 1);
}

void TaskServiceTest::archivesAndRestoresTerminalStatuses()
{
    for (const TaskStatus terminal : {TaskStatus::Done, TaskStatus::Cancelled}) {
        const Task original = storedTask(terminal);
        FakeTaskRepository repository{{original}};
        FakeTaskDependencyRepository dependencyRepository;
        FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
        FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
        TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};
        QSignalSpy changedSpy{&service, &TaskService::tasksChanged};

        const auto archiveResult = service.archiveTask(original.id());
        QVERIFY(archiveResult.ok());
        QCOMPARE(archiveResult.value->status(), TaskStatus::Archived);
        QCOMPARE(archiveResult.value->statusBeforeArchive(),
                 std::optional<TaskStatus>{terminal});

        const auto restoreResult = service.restoreTask(original.id());
        QVERIFY(restoreResult.ok());
        QCOMPARE(restoreResult.value->status(), terminal);
        QVERIFY(!restoreResult.value->statusBeforeArchive().has_value());
        QCOMPARE(changedSpy.count(), 2);
    }
}

void TaskServiceTest::restoresLegacyArchiveWithoutInProgressConflict()
{
    const Task archived = storedTask(TaskStatus::Archived, TaskStatus::InProgress);
    const Task active = storedTask(TaskStatus::InProgress);
    FakeTaskRepository repository{{archived, active}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};

    const auto result = service.restoreTask(archived.id());

    QVERIFY(result.ok());
    QCOMPARE(result.value->status(), TaskStatus::Todo);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(repository.findById(archived.id())->status(), TaskStatus::Todo);
    QVERIFY(!repository.findById(archived.id())->statusBeforeArchive().has_value());
}

void TaskServiceTest::reportsInvalidOperationsAndMissingTasks()
{
    const Task todo = storedTask();
    FakeTaskRepository repository{{todo}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};
    const QUuid missingId = QUuid::createUuid();

    QCOMPARE(service.findTask(missingId).error, TaskError::NotFound);
    QCOMPARE(service.updateTask(missingId, validDraft()).error, TaskError::NotFound);
    QCOMPARE(service.archiveTask(missingId).error, TaskError::NotFound);
    QCOMPARE(service.restoreTask(missingId).error, TaskError::NotFound);
    QCOMPARE(service.restoreTask(todo.id()).error,
             TaskError::InvalidTaskTransition);
    QCOMPARE(changedSpy.count(), 0);
}

void TaskServiceTest::mapsRepositoryFailures()
{
    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};

    repository.setReadFailure(true);
    const auto listResult = service.listTasks();
    QCOMPARE(listResult.error, TaskError::PersistenceFailure);
    QVERIFY(!listResult.detail.isEmpty());
    QCOMPARE(service.findTask(QUuid::createUuid()).error, TaskError::PersistenceFailure);

    repository.setReadFailure(false);
    repository.setWriteFailure(true);
    const auto createResult = service.createTask(validDraft());
    QCOMPARE(createResult.error, TaskError::PersistenceFailure);
    QVERIFY(!createResult.detail.isEmpty());
    QCOMPARE(changedSpy.count(), 0);
}

void TaskServiceTest::replacesDependenciesAndReportsStructuredErrors()
{
    const Task predecessor = storedTask(TaskStatus::Todo, std::nullopt,
                                        QStringLiteral("Predecessor"));
    const Task target = storedTask(TaskStatus::Todo, std::nullopt,
                                   QStringLiteral("Target"));
    const Task archived = storedTask(TaskStatus::Archived, TaskStatus::Todo,
                                     QStringLiteral("Archived"));
    const Task completed = storedTask(TaskStatus::Done, std::nullopt,
                                      QStringLiteral("Completed"));
    const Task cancelled = storedTask(TaskStatus::Cancelled, std::nullopt,
                                      QStringLiteral("Cancelled"));
    FakeTaskRepository repository{
        {predecessor, target, archived, completed, cancelled}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};
    QSignalSpy changedSpy{&service, &TaskService::dependenciesChanged};

    QVERIFY(service.listDependencies().ok());
    QVERIFY(service.listDependencies().value->isEmpty());

    const auto replaced = service.replaceTaskPredecessors(
        target.id(), {predecessor.id()});
    QVERIFY(replaced.ok());
    QCOMPARE(replaced.value->size(), 1);
    QCOMPARE(dependencyRepository.replaceCount(), 1);
    QCOMPARE(changedSpy.count(), 1);

    // 相同集合是无写入的幂等成功。
    QVERIFY(service.replaceTaskPredecessors(target.id(), {predecessor.id()}).ok());
    QCOMPARE(dependencyRepository.replaceCount(), 1);
    QCOMPARE(changedSpy.count(), 1);

    const auto selfReference = service.replaceTaskPredecessors(
        target.id(), {target.id()});
    QCOMPARE(selfReference.error, TaskError::DependencySelfReference);
    QCOMPARE(selfReference.context.conflictingTaskIds,
             QList<smartmate::model::TaskId>({target.id()}));

    const auto duplicate = service.replaceTaskPredecessors(
        target.id(), {predecessor.id(), predecessor.id()});
    QCOMPARE(duplicate.error, TaskError::DependencyDuplicate);
    QCOMPARE(duplicate.context.conflictingTaskIds,
             QList<smartmate::model::TaskId>({predecessor.id()}));

    const QUuid missingId = QUuid::createUuid();
    const auto missing = service.replaceTaskPredecessors(target.id(), {missingId});
    QCOMPARE(missing.error, TaskError::DependencyEndpointNotFound);
    QCOMPARE(missing.context.conflictingTaskIds,
             QList<smartmate::model::TaskId>({missingId}));

    QCOMPARE(service.replaceTaskPredecessors(completed.id(), {}).error,
             TaskError::DependencyTargetNotEditable);
    const auto archivedPredecessor = service.replaceTaskPredecessors(
        target.id(), {archived.id()});
    QCOMPARE(archivedPredecessor.error,
             TaskError::DependencyPredecessorNotEligible);
    QCOMPARE(archivedPredecessor.context.conflictingTaskIds,
             QList<smartmate::model::TaskId>({archived.id()}));
    const auto cancelledPredecessor = service.replaceTaskPredecessors(
        target.id(), {cancelled.id()});
    QCOMPARE(cancelledPredecessor.error,
             TaskError::DependencyPredecessorNotEligible);
    QCOMPARE(cancelledPredecessor.context.conflictingTaskIds,
             QList<TaskId>{cancelled.id()});

    // 现有 A→B 上再把 B 设为 A 的前置会形成 B→A→B 的闭合环。
    const auto cycle = service.replaceTaskPredecessors(
        predecessor.id(), {target.id()});
    QCOMPARE(cycle.error, TaskError::DependencyCycle);
    QVERIFY(cycle.context.cyclePath.size() >= 3);
    QCOMPARE(cycle.context.cyclePath.constFirst(),
             cycle.context.cyclePath.constLast());
    QCOMPARE(dependencyRepository.replaceCount(), 1);

    const auto cleared = service.replaceTaskPredecessors(target.id(), {});
    QVERIFY(cleared.ok());
    QVERIFY(cleared.value->isEmpty());
    QVERIFY(dependencyRepository.dependencies().isEmpty());
    QCOMPARE(dependencyRepository.replaceCount(), 2);
    QCOMPARE(changedSpy.count(), 2);

    // 已存在的归档或取消前置可以继续保留或移除，但不能作为新前置加入。
    FakeTaskDependencyRepository legacyDependencies{
        {{archived.id(), target.id()}, {cancelled.id(), target.id()}}};
    FakeTaskCreationRepository legacyCreation{repository, legacyDependencies};
    FakeTaskBatchTransitionRepository legacyBatchTransition{repository};
    TaskService legacyService{repository, legacyDependencies, legacyCreation,
                              legacyBatchTransition, deletionRepository,
                              categoryRepository};
    QSignalSpy legacyChangedSpy{&legacyService, &TaskService::dependenciesChanged};
    const auto retainArchived = legacyService.replaceTaskPredecessors(
        target.id(), {archived.id(), cancelled.id(), predecessor.id()});
    QVERIFY(retainArchived.ok());
    QCOMPARE(retainArchived.value->size(), 3);
    const auto removeArchived = legacyService.replaceTaskPredecessors(
        target.id(), {predecessor.id()});
    QVERIFY(removeArchived.ok());
    QCOMPARE(removeArchived.value->size(), 1);
    QCOMPARE(removeArchived.value->constFirst().predecessorId, predecessor.id());
    QCOMPARE(legacyChangedSpy.count(), 2);
}

void TaskServiceTest::enforcesDependencyStatusConsistency()
{
    const Task predecessor = storedTask(TaskStatus::Todo, std::nullopt,
                                        QStringLiteral("Unfinished predecessor"));
    const Task target = storedTask(TaskStatus::Todo, std::nullopt,
                                   QStringLiteral("Blocked target"));
    FakeTaskRepository repository{{predecessor, target}};
    FakeTaskDependencyRepository dependencyRepository{
        {{predecessor.id(), target.id()}}};
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};

    const auto startBlocked = service.startTask(target.id());
    QCOMPARE(startBlocked.error, TaskError::TaskBlocked);
    QCOMPARE(startBlocked.context.blockingTaskIds,
             QList<smartmate::model::TaskId>({predecessor.id()}));
    QCOMPARE(startBlocked.context.conflictingTaskIds,
             QList<smartmate::model::TaskId>({target.id()}));

    QCOMPARE(service.completeTask(target.id()).error,
             TaskError::InvalidTaskTransition);

    const Task completedPredecessor = storedTask(
        TaskStatus::Done, std::nullopt, QStringLiteral("Completed predecessor"));
    const Task activeSuccessor = storedTask(
        TaskStatus::InProgress, std::nullopt, QStringLiteral("Active successor"));
    FakeTaskRepository activeRepository{{completedPredecessor, activeSuccessor}};
    FakeTaskDependencyRepository activeDependencies{
        {{completedPredecessor.id(), activeSuccessor.id()}}};
    FakeTaskCreationRepository activeCreation{activeRepository,
                                              activeDependencies};
    FakeTaskBatchTransitionRepository activeBatchTransition{activeRepository};
    TaskService activeService{activeRepository, activeDependencies,
                              activeCreation, activeBatchTransition,
                              deletionRepository, categoryRepository};

    const auto invalidateActive = activeService.redoTask(completedPredecessor.id());
    QCOMPARE(invalidateActive.error, TaskError::DependencyStateConflict);
    QCOMPARE(invalidateActive.context.blockingTaskIds,
             QList<smartmate::model::TaskId>({completedPredecessor.id()}));
    QCOMPARE(invalidateActive.context.conflictingTaskIds,
             QList<smartmate::model::TaskId>({activeSuccessor.id()}));
    QCOMPARE(activeRepository.findById(completedPredecessor.id())->status(),
             TaskStatus::Done);

    const Task archivedCompletedSuccessor = storedTask(
        TaskStatus::Archived, TaskStatus::Done,
        QStringLiteral("Archived completed successor"));
    FakeTaskRepository archivedSuccessorRepository{
        {completedPredecessor, archivedCompletedSuccessor}};
    FakeTaskDependencyRepository archivedSuccessorDependencies{
        {{completedPredecessor.id(), archivedCompletedSuccessor.id()}}};
    FakeTaskCreationRepository archivedSuccessorCreation{
        archivedSuccessorRepository, archivedSuccessorDependencies};
    FakeTaskBatchTransitionRepository archivedSuccessorBatchTransition{
        archivedSuccessorRepository};
    TaskService archivedSuccessorService{
        archivedSuccessorRepository, archivedSuccessorDependencies,
        archivedSuccessorCreation, archivedSuccessorBatchTransition,
        deletionRepository, categoryRepository};
    QCOMPARE(archivedSuccessorService.redoTask(completedPredecessor.id()).error,
             TaskError::DependencyStateConflict);

    // Archived-before-Done 前置仍满足关系，因此目标可以开始。
    const Task archivedCompletedPredecessor = storedTask(
        TaskStatus::Archived, TaskStatus::Done,
        QStringLiteral("Archived completed predecessor"));
    const Task readyTarget = storedTask(TaskStatus::Todo);
    FakeTaskRepository readyRepository{{archivedCompletedPredecessor, readyTarget}};
    FakeTaskDependencyRepository readyDependencies{
        {{archivedCompletedPredecessor.id(), readyTarget.id()}}};
    FakeTaskCreationRepository readyCreation{readyRepository, readyDependencies};
    FakeTaskBatchTransitionRepository readyBatchTransition{readyRepository};
    TaskService readyService{readyRepository, readyDependencies, readyCreation,
                             readyBatchTransition, deletionRepository,
                             categoryRepository};
    QVERIFY(readyService.startTask(readyTarget.id()).ok());

    const Task archivedActiveTarget = storedTask(
        TaskStatus::Archived, TaskStatus::InProgress,
        QStringLiteral("Archived active target"));
    FakeTaskRepository restoreRepository{{predecessor, archivedActiveTarget}};
    FakeTaskDependencyRepository restoreDependencies{
        {{predecessor.id(), archivedActiveTarget.id()}}};
    FakeTaskCreationRepository restoreCreation{restoreRepository,
                                               restoreDependencies};
    FakeTaskBatchTransitionRepository restoreBatchTransition{restoreRepository};
    TaskService restoreService{restoreRepository, restoreDependencies,
                               restoreCreation, restoreBatchTransition,
                               deletionRepository, categoryRepository};
    const auto restoreBlocked = restoreService.restoreTask(archivedActiveTarget.id());
    QVERIFY(restoreBlocked.ok());
    QCOMPARE(restoreBlocked.value->status(), TaskStatus::Todo);
    QCOMPARE(restoreRepository.findById(archivedActiveTarget.id())->status(),
             TaskStatus::Todo);
}

void TaskServiceTest::mapsDependencyRepositoryFailures()
{
    const Task predecessor = storedTask(TaskStatus::Todo);
    const Task target = storedTask(TaskStatus::Todo);
    FakeTaskRepository repository{{predecessor, target}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};
    QSignalSpy changedSpy{&service, &TaskService::dependenciesChanged};

    dependencyRepository.setReadFailure(true);
    QCOMPARE(service.listDependencies().error, TaskError::PersistenceFailure);
    QCOMPARE(service.listRecommendedTasks().error, TaskError::PersistenceFailure);
    QCOMPARE(service.taskGraphSnapshot().error, TaskError::PersistenceFailure);

    dependencyRepository.setReadFailure(false);
    dependencyRepository.setWriteFailure(true);
    const auto writeFailure = service.replaceTaskPredecessors(
        target.id(), {predecessor.id()});
    QCOMPARE(writeFailure.error, TaskError::PersistenceFailure);
    QVERIFY(!writeFailure.detail.isEmpty());
    QCOMPARE(changedSpy.count(), 0);
}

void TaskServiceTest::buildsGraphSnapshotWithArchivedClosure()
{
    const Task archivedCompleted = storedTask(
        TaskStatus::Archived, TaskStatus::Done,
        QStringLiteral("Archived completed predecessor"));
    const Task root = storedTask(TaskStatus::Todo, std::nullopt,
                                 QStringLiteral("Active root"));
    const Task connectedArchived = storedTask(
        TaskStatus::Archived, TaskStatus::Todo,
        QStringLiteral("Connected archived successor"));
    const Task isolatedActive = storedTask(
        TaskStatus::Cancelled, std::nullopt,
        QStringLiteral("Isolated active task"));
    const Task pureArchived = storedTask(
        TaskStatus::Archived, TaskStatus::Todo,
        QStringLiteral("Pure archived component"));
    FakeTaskRepository repository{{pureArchived, connectedArchived, root,
                                   isolatedActive, archivedCompleted}};
    FakeTaskDependencyRepository dependencyRepository{
        {{root.id(), connectedArchived.id()},
         {archivedCompleted.id(), root.id()}}};
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    const TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};

    const auto result = service.taskGraphSnapshot();

    QVERIFY(result.ok());
    QCOMPARE(result.value->nodes.size(), 4);
    QCOMPARE(result.value->edges.size(), 2);
    const auto findNode = [&result](const smartmate::model::TaskId &taskId) {
        return std::find_if(result.value->nodes.cbegin(), result.value->nodes.cend(),
                            [&taskId](const smartmate::model::TaskGraphNode &node) {
            return node.task.id() == taskId;
        });
    };
    const auto archivedNode = findNode(archivedCompleted.id());
    const auto rootNode = findNode(root.id());
    const auto connectedNode = findNode(connectedArchived.id());
    const auto isolatedNode = findNode(isolatedActive.id());
    QVERIFY(archivedNode != result.value->nodes.cend());
    QVERIFY(rootNode != result.value->nodes.cend());
    QVERIFY(connectedNode != result.value->nodes.cend());
    QVERIFY(isolatedNode != result.value->nodes.cend());
    QVERIFY(findNode(pureArchived.id()) == result.value->nodes.cend());
    QCOMPARE(archivedNode->dependencyLevel, 0);
    QCOMPARE(rootNode->dependencyLevel, 1);
    QCOMPARE(connectedNode->dependencyLevel, 2);
    QCOMPARE(isolatedNode->dependencyLevel, 0);
    QCOMPARE(rootNode->predecessorClosureIds,
             QList<smartmate::model::TaskId>({archivedCompleted.id()}));
    QCOMPARE(rootNode->successorClosureIds,
             QList<smartmate::model::TaskId>({connectedArchived.id()}));
    QCOMPARE(connectedNode->predecessorClosureIds.size(), 2);
    QVERIFY(connectedNode->predecessorClosureIds.contains(archivedCompleted.id()));
    QVERIFY(connectedNode->predecessorClosureIds.contains(root.id()));

    const auto satisfiedEdge = std::find_if(
        result.value->edges.cbegin(), result.value->edges.cend(),
        [&archivedCompleted](const smartmate::model::TaskGraphEdge &edge) {
            return edge.dependency.predecessorId == archivedCompleted.id();
        });
    const auto unsatisfiedEdge = std::find_if(
        result.value->edges.cbegin(), result.value->edges.cend(),
        [&root](const smartmate::model::TaskGraphEdge &edge) {
            return edge.dependency.predecessorId == root.id();
        });
    QVERIFY(satisfiedEdge != result.value->edges.cend());
    QVERIFY(unsatisfiedEdge != result.value->edges.cend());
    QCOMPARE(satisfiedEdge->resolution,
             smartmate::model::TaskDependencyResolution::Satisfied);
    QCOMPARE(unsatisfiedEdge->resolution,
             smartmate::model::TaskDependencyResolution::Pending);
}

void TaskServiceTest::buildsDependencyEditContextInModel()
{
    const Task target = storedTask(TaskStatus::Todo, std::nullopt,
                                   QStringLiteral("Target"));
    const Task active = storedTask(TaskStatus::InProgress, std::nullopt,
                                   QStringLiteral("Active"));
    const Task existingCancelled = storedTask(
        TaskStatus::Cancelled, std::nullopt, QStringLiteral("Existing cancelled"));
    const Task existingArchived = storedTask(
        TaskStatus::Archived, TaskStatus::Done, QStringLiteral("Existing archived"));
    const Task hiddenCancelled = storedTask(
        TaskStatus::Cancelled, std::nullopt, QStringLiteral("Hidden cancelled"));
    FakeTaskRepository repository{{hiddenCancelled, existingArchived, target,
                                   active, existingCancelled}};
    FakeTaskDependencyRepository dependencyRepository{
        {{existingCancelled.id(), target.id()},
         {existingArchived.id(), target.id()}}};
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    const TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};

    const auto result = service.taskDependencyEditContext(target.id());

    QVERIFY(result.ok());
    QCOMPARE(result.value->targetTask, target);
    QCOMPARE(result.value->taskTitles.size(), 5);
    QCOMPARE(result.value->candidates.size(), 3);
    const auto candidateFor = [&result](const TaskId &id) {
        return std::find_if(
            result.value->candidates.cbegin(), result.value->candidates.cend(),
            [&id](const smartmate::model::TaskDependencyCandidate &candidate) {
                return candidate.task.id() == id;
            });
    };
    const auto activeCandidate = candidateFor(active.id());
    const auto cancelledCandidate = candidateFor(existingCancelled.id());
    const auto archivedCandidate = candidateFor(existingArchived.id());
    QVERIFY(activeCandidate != result.value->candidates.cend());
    QVERIFY(cancelledCandidate != result.value->candidates.cend());
    QVERIFY(archivedCandidate != result.value->candidates.cend());
    QVERIFY(!activeCandidate->selected);
    QVERIFY(activeCandidate->selectable);
    QVERIFY(cancelledCandidate->selected);
    QVERIFY(cancelledCandidate->selectable);
    QVERIFY(archivedCandidate->selected);
    QVERIFY(archivedCandidate->selectable);
    QVERIFY(candidateFor(hiddenCancelled.id()) == result.value->candidates.cend());
    QVERIFY(candidateFor(target.id()) == result.value->candidates.cend());
}

void TaskServiceTest::rejectsDependencyEditContextForNonTodoTarget()
{
    const Task target = storedTask(TaskStatus::Done);
    FakeTaskRepository repository{{target}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    const TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};

    const auto result = service.taskDependencyEditContext(target.id());

    QCOMPARE(result.error, TaskError::DependencyTargetNotEditable);
    QCOMPARE(result.context.conflictingTaskIds, QList<TaskId>{target.id()});
}

void TaskServiceTest::keepsPlanAndGraphCommandAvailabilityConsistent()
{
    const Task predecessor = storedTask(TaskStatus::Done);
    const Task target = storedTask(TaskStatus::Todo);
    FakeTaskRepository repository{{target, predecessor}};
    FakeTaskDependencyRepository dependencyRepository{
        {{predecessor.id(), target.id()}}};
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchTransitionRepository{repository};
    const TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionRepository, deletionRepository,
                        categoryRepository};

    const auto plan = service.listRecommendedTasks();
    const auto graph = service.taskGraphSnapshot();

    QVERIFY(plan.ok());
    QVERIFY(graph.ok());
    for (const auto &node : graph.value->nodes) {
        const auto planned = std::find_if(
            plan.value->cbegin(), plan.value->cend(), [&node](const auto &item) {
                return item.task.id() == node.task.id();
            });
        QVERIFY(planned != plan.value->cend());
        QCOMPARE(node.availability, planned->availability);
    }
}

void TaskServiceTest::recordsImmutableActivityEventForEverySuccessfulTransition()
{
    const QDateTime deadline = timestamp().addDays(2);
    const TaskCategory category{QUuid::createUuid(),
                                QStringLiteral("学习"),
                                TaskCategoryColor::Violet,
                                timestamp(),
                                timestamp()};
    const Task primary{QUuid::createUuid(),
                       QStringLiteral("事件主任务"),
                       {},
                       TaskPriority::Urgent,
                       TaskStatus::InProgress,
                       std::nullopt,
                       deadline,
                       45,
                       timestamp(),
                       timestamp(),
                       category.id};
    const Task cancelled = storedTask(TaskStatus::Todo);
    FakeTaskRepository repository{{primary, cancelled}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository transitionRepository{repository};
    FakeTaskDeletionRepository deletion{repository, dependencyRepository};
    FakeTaskCategoryRepository categories{{category}};
    TaskService service{repository, dependencyRepository, creationRepository,
                        transitionRepository, deletion, categories};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};

    QVERIFY(service.completeTask(primary.id()).ok());
    QVERIFY(service.redoTask(primary.id()).ok());
    QVERIFY(service.startTask(primary.id()).ok());
    QVERIFY(service.completeTask(primary.id()).ok());
    QVERIFY(service.archiveTask(primary.id()).ok());
    QVERIFY(service.restoreTask(primary.id()).ok());
    QVERIFY(service.cancelTask(cancelled.id()).ok());

    QCOMPARE(changedSpy.count(), 7);
    QCOMPARE(transitionRepository.events().size(), 7);
    QCOMPARE(std::count_if(transitionRepository.events().cbegin(),
                           transitionRepository.events().cend(),
                           [](const auto &event) {
                               return event.transition == TaskTransition::Complete;
                           }),
             2);
    const auto &first = transitionRepository.events().first();
    QCOMPARE(first.taskId, primary.id());
    QCOMPARE(first.fromStatus, TaskStatus::InProgress);
    QCOMPARE(first.toStatus, TaskStatus::Done);
    QCOMPARE(first.deadlineSnapshotUtc, std::optional<QDateTime>{deadline});
    QCOMPARE(first.estimatedMinutesSnapshot, std::optional<int>{45});
    QCOMPARE(first.prioritySnapshot, TaskPriority::Urgent);
    QCOMPARE(first.categoryIdSnapshot,
             std::optional<TaskCategoryId>{category.id});
    QCOMPARE(first.categoryNameSnapshot,
             std::optional<QString>{category.name});
    QCOMPARE(first.categoryColorSnapshot,
             std::optional<TaskCategoryColor>{category.color});
}

QTEST_APPLESS_MAIN(TaskServiceTest)

#include "tst_TaskService.moc"
