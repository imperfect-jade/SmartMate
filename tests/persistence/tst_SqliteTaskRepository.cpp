#include "persistence/SqliteTaskRepository.h"
#include "persistence/TaskSqlCodec.h"

#include "domain/TaskDependency.h"
#include "domain/TaskCategory.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>
#include <QUuid>

using smartmate::model::RepositoryException;
using smartmate::model::Task;
using smartmate::model::TaskCategory;
using smartmate::model::TaskCategoryColor;
using smartmate::model::TaskDependency;
using smartmate::model::TaskId;
using smartmate::model::TaskPriority;
using smartmate::model::TaskStateChange;
using smartmate::model::TaskStatus;
using smartmate::model::TaskTransition;
using smartmate::model::TaskTransitionWrite;
using smartmate::model::persistence::SqliteTaskRepository;

namespace {

const QDateTime kCreatedAt =
    QDateTime::fromMSecsSinceEpoch(1'752'000'000'123LL, QTimeZone::UTC);
const QDateTime kUpdatedAt =
    QDateTime::fromMSecsSinceEpoch(1'752'000'123'456LL, QTimeZone::UTC);

Task makeTask(
    const QUuid &id,
    QString title = QStringLiteral("编写 SQLite 测试"),
    TaskPriority priority = TaskPriority::Normal,
    TaskStatus status = TaskStatus::Todo,
    std::optional<TaskStatus> statusBeforeArchive = std::nullopt,
    std::optional<QDateTime> deadline = std::nullopt,
    std::optional<int> estimatedMinutes = std::nullopt,
    QDateTime updatedAt = kUpdatedAt,
    std::optional<QUuid> categoryId = std::nullopt)
{
    return Task(
        id,
        std::move(title),
        QStringLiteral("覆盖中文、标点与 emoji：✅"),
        priority,
        status,
        statusBeforeArchive,
        deadline,
        estimatedMinutes,
        kCreatedAt,
        updatedAt,
        categoryId);
}

TaskTransitionWrite transitionWrite(TaskStateChange change,
                                    const TaskTransition transition)
{
    smartmate::model::TaskActivityEvent event;
    event.eventId = QUuid::createUuid();
    event.taskId = change.taskId;
    event.transition = transition;
    event.fromStatus = change.expectedStatus;
    event.toStatus = change.targetStatus;
    event.occurredAtUtc = change.updatedAtUtc;
    event.prioritySnapshot = TaskPriority::Normal;
    return {std::move(change), std::move(event)};
}

void compareTasks(const Task &actual, const Task &expected)
{
    QCOMPARE(actual.id(), expected.id());
    QCOMPARE(actual.title(), expected.title());
    QCOMPARE(actual.description(), expected.description());
    QCOMPARE(actual.priority(), expected.priority());
    QCOMPARE(actual.status(), expected.status());
    QVERIFY(actual.statusBeforeArchive() == expected.statusBeforeArchive());
    QVERIFY(actual.deadline() == expected.deadline());
    QVERIFY(actual.estimatedMinutes() == expected.estimatedMinutes());
    QVERIFY(actual.categoryId() == expected.categoryId());
    QCOMPARE(actual.createdAtUtc(), expected.createdAtUtc());
    QCOMPARE(actual.updatedAtUtc(), expected.updatedAtUtc());
}

void compareCategories(const TaskCategory &actual, const TaskCategory &expected)
{
    QCOMPARE(actual.id, expected.id);
    QCOMPARE(actual.name, expected.name);
    QCOMPARE(actual.color, expected.color);
    QCOMPARE(actual.createdAtUtc, expected.createdAtUtc);
    QCOMPARE(actual.updatedAtUtc, expected.updatedAtUtc);
}

[[nodiscard]] QString uniqueConnectionName(const QString &prefix)
{
    return QStringLiteral("%1_%2")
        .arg(prefix, QUuid::createUuid().toString(QUuid::WithoutBraces));
}

void createVersionOneDatabase(const QString &databasePath, const Task &task)
{
    const QString connectionName = uniqueConnectionName(QStringLiteral("create_v1"));
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        QVERIFY2(database.open(), qPrintable(database.lastError().text()));

        QSqlQuery schemaQuery(database);
        QVERIFY2(schemaQuery.exec(QStringLiteral(
                     "CREATE TABLE tasks ("
                     "id TEXT PRIMARY KEY NOT NULL, "
                     "title TEXT NOT NULL, "
                     "description TEXT NOT NULL, "
                     "priority TEXT NOT NULL, "
                     "status TEXT NOT NULL, "
                     "status_before_archive TEXT NULL, "
                     "deadline_utc_ms INTEGER NULL, "
                     "estimated_minutes INTEGER NULL, "
                     "created_at_utc_ms INTEGER NOT NULL, "
                     "updated_at_utc_ms INTEGER NOT NULL)")),
                 qPrintable(schemaQuery.lastError().text()));

        QSqlQuery insertQuery(database);
        insertQuery.prepare(QStringLiteral(
            "INSERT INTO tasks ("
            "id, title, description, priority, status, status_before_archive, "
            "deadline_utc_ms, estimated_minutes, created_at_utc_ms, updated_at_utc_ms"
            ") VALUES ("
            ":id, :title, :description, :priority, :status, NULL, "
            ":deadline, :estimate, :created, :updated)"));
        insertQuery.bindValue(QStringLiteral(":id"),
                              task.id().toString(QUuid::WithoutBraces));
        insertQuery.bindValue(QStringLiteral(":title"), task.title());
        insertQuery.bindValue(QStringLiteral(":description"), task.description());
        insertQuery.bindValue(QStringLiteral(":priority"), QStringLiteral("high"));
        insertQuery.bindValue(QStringLiteral(":status"), QStringLiteral("done"));
        insertQuery.bindValue(QStringLiteral(":deadline"),
                              task.deadline()->toMSecsSinceEpoch());
        insertQuery.bindValue(QStringLiteral(":estimate"), *task.estimatedMinutes());
        insertQuery.bindValue(QStringLiteral(":created"),
                              task.createdAtUtc().toMSecsSinceEpoch());
        insertQuery.bindValue(QStringLiteral(":updated"),
                              task.updatedAtUtc().toMSecsSinceEpoch());
        QVERIFY2(insertQuery.exec(), qPrintable(insertQuery.lastError().text()));

        QSqlQuery versionQuery(database);
        QVERIFY2(versionQuery.exec(QStringLiteral("PRAGMA user_version = 1")),
                 qPrintable(versionQuery.lastError().text()));
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void createVersionTwoDatabase(const QString &databasePath,
                              const Task &predecessor,
                              const Task &successor)
{
    const QString connectionName = uniqueConnectionName(QStringLiteral("create_v2"));
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        QVERIFY2(database.open(), qPrintable(database.lastError().text()));
        QSqlQuery query(database);
        QVERIFY2(query.exec(QStringLiteral(
                     "CREATE TABLE tasks ("
                     "id TEXT PRIMARY KEY NOT NULL, title TEXT NOT NULL, "
                     "description TEXT NOT NULL, priority TEXT NOT NULL, "
                     "status TEXT NOT NULL, status_before_archive TEXT NULL, "
                     "deadline_utc_ms INTEGER NULL, estimated_minutes INTEGER NULL, "
                     "created_at_utc_ms INTEGER NOT NULL, updated_at_utc_ms INTEGER NOT NULL)")),
                 qPrintable(query.lastError().text()));
        QVERIFY2(query.exec(QStringLiteral(
                     "CREATE TABLE task_dependencies ("
                     "predecessor_id TEXT NOT NULL, successor_id TEXT NOT NULL, "
                     "PRIMARY KEY (predecessor_id, successor_id), "
                     "CHECK (predecessor_id <> successor_id), "
                     "FOREIGN KEY (predecessor_id) REFERENCES tasks(id) ON DELETE RESTRICT, "
                     "FOREIGN KEY (successor_id) REFERENCES tasks(id) ON DELETE RESTRICT)")),
                 qPrintable(query.lastError().text()));

        QSqlQuery insertTask(database);
        insertTask.prepare(QStringLiteral(
            "INSERT INTO tasks ("
            "id, title, description, priority, status, status_before_archive, "
            "deadline_utc_ms, estimated_minutes, created_at_utc_ms, updated_at_utc_ms"
            ") VALUES ("
            ":id, :title, :description, :priority, :status, NULL, "
            "NULL, NULL, :created, :updated)"));
        for (const Task *task : {&predecessor, &successor}) {
            insertTask.bindValue(QStringLiteral(":id"),
                                 task->id().toString(QUuid::WithoutBraces));
            insertTask.bindValue(QStringLiteral(":title"), task->title());
            insertTask.bindValue(QStringLiteral(":description"), task->description());
            insertTask.bindValue(QStringLiteral(":priority"), QStringLiteral("normal"));
            insertTask.bindValue(QStringLiteral(":status"),
                                 task == &predecessor ? QStringLiteral("done")
                                                      : QStringLiteral("todo"));
            insertTask.bindValue(QStringLiteral(":created"),
                                 task->createdAtUtc().toMSecsSinceEpoch());
            insertTask.bindValue(QStringLiteral(":updated"),
                                 task->updatedAtUtc().toMSecsSinceEpoch());
            QVERIFY2(insertTask.exec(), qPrintable(insertTask.lastError().text()));
            insertTask.finish();
        }

        QSqlQuery edgeQuery(database);
        edgeQuery.prepare(QStringLiteral(
            "INSERT INTO task_dependencies (predecessor_id, successor_id) "
            "VALUES (:predecessor, :successor)"));
        edgeQuery.bindValue(QStringLiteral(":predecessor"),
                            predecessor.id().toString(QUuid::WithoutBraces));
        edgeQuery.bindValue(QStringLiteral(":successor"),
                            successor.id().toString(QUuid::WithoutBraces));
        QVERIFY2(edgeQuery.exec(), qPrintable(edgeQuery.lastError().text()));
        QVERIFY2(query.exec(QStringLiteral("PRAGMA user_version = 2")),
                 qPrintable(query.lastError().text()));
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

[[nodiscard]] bool dependencyWriteThrows(
    SqliteTaskRepository &repository,
    const QUuid &successorId,
    const QList<QUuid> &predecessorIds)
{
    try {
        repository.replacePredecessors(successorId, predecessorIds);
    } catch (const RepositoryException &) {
        return true;
    }
    return false;
}

[[nodiscard]] bool atomicCreationThrows(
    SqliteTaskRepository &repository,
    const Task &task,
    const QList<QUuid> &predecessorIds)
{
    try {
        repository.insertTaskWithPredecessors(task, predecessorIds);
    } catch (const RepositoryException &) {
        return true;
    }
    return false;
}

[[nodiscard]] bool permanentDeletionThrows(
    SqliteTaskRepository &repository,
    const QList<QUuid> &taskIds)
{
    try {
        (void) repository.deleteArchivedTasksWithDependencies(taskIds);
    } catch (const RepositoryException &) {
        return true;
    }
    return false;
}

} // namespace

// 验证领域值与SQLite的双向映射、重复初始化安全性和数据库级约束。
class SqliteTaskRepositoryTest final : public QObject {
    Q_OBJECT

private slots:
    void initializesSchemaIdempotently();
    void migratesVersionOneWithoutChangingTaskData();
    void migratesVersionTwoWithoutChangingTasksOrDependencies();
    void rollsBackFailedVersionOneMigration();
    void rejectsFutureSchemaVersion();
    void stableSqlCodecsRoundTripEveryValue();
    void stableSqlCodecsRejectInvalidValues();
    void roundTripsTaskCategoryAndUnicodeNameKey();
    void categoryForeignKeyRejectsMissingCategory();
    void deletesCategoryAndUnassignsTasksWithoutChangingDependencies();
    void categoryDeletionRollsBackAtomically();
    void roundTripsCompleteUnicodeTask();
    void roundTripsNullOptionals();
    void storesOmittedDescriptionAsEmptyText();
    void persistsAcrossRepositoryReopen();
    void updatesArchivedTaskAndOriginalStatus();
    void rejectsSecondInProgressTask();
    void returnsFalseWhenUpdatingMissingTask();
    void roundTripsDependenciesAcrossRepositoryReopen();
    void enforcesDependencyIdentityForeignKeysAndCycles();
    void restrictsDeletingTasksReferencedByDependencies();
    void replacePredecessorsRollsBackAtomically();
    void atomicallyCreatesTaskWithPredecessorsAcrossReopen();
    void atomicCreationRollsBackTaskAfterMidwayEdgeFailure();
    void atomicCreationRollsBackOnTaskOrSelfDependencyConstraint();
    void batchStateChangesCommitAcrossReopen();
    void batchStateChangesRollBackOnConflictOrSqlFailure();
    void activityEventsRoundTripAndCascadeOnPermanentDelete();
    void permanentlyDeletesArchivedTaskAndAllDependenciesAcrossReopen();
    void rejectsPermanentDeletionOfActiveOrMissingTaskAtomically();
    void permanentDeletionRollsBackAfterTaskDeleteFailure();
};

void SqliteTaskRepositoryTest::initializesSchemaIdempotently()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("tasks.db"));

    {
        SqliteTaskRepository repository(databasePath);
        QVERIFY(repository.findAll().isEmpty());
    }
    {
        SqliteTaskRepository repository(databasePath);
        QVERIFY(repository.findAll().isEmpty());
    }

    const QString connectionName =
        QStringLiteral("schema_verification_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        QVERIFY2(database.open(), qPrintable(database.lastError().text()));

        QSqlQuery versionQuery(database);
        QVERIFY(versionQuery.exec(QStringLiteral("PRAGMA user_version")));
        QVERIFY(versionQuery.next());
        QCOMPARE(versionQuery.value(0).toInt(), 4);

        QSqlQuery indexQuery(database);
        QVERIFY(indexQuery.exec(QStringLiteral(
            "SELECT COUNT(*) FROM sqlite_master "
            "WHERE type = 'index' AND name IN ("
            "'idx_tasks_status', 'idx_tasks_deadline', 'idx_tasks_single_in_progress', "
            "'idx_tasks_category_id')")));
        QVERIFY(indexQuery.next());
        QCOMPARE(indexQuery.value(0).toInt(), 4);

        QSqlQuery categorySchemaQuery(database);
        QVERIFY(categorySchemaQuery.exec(QStringLiteral(
            "SELECT COUNT(*) FROM sqlite_master WHERE "
            "(type = 'table' AND name = 'task_categories') OR "
            "(type = 'index' AND name = 'idx_tasks_category_id')")));
        QVERIFY(categorySchemaQuery.next());
        QCOMPARE(categorySchemaQuery.value(0).toInt(), 2);

        QSqlQuery dependencySchemaQuery(database);
        QVERIFY(dependencySchemaQuery.exec(QStringLiteral(
            "SELECT COUNT(*) FROM sqlite_master WHERE "
            "(type = 'table' AND name = 'task_dependencies') OR "
            "(type = 'index' AND name = 'idx_task_dependencies_successor') OR "
            "(type = 'trigger' AND name = 'trg_task_dependencies_prevent_cycle')")));
        QVERIFY(dependencySchemaQuery.next());
        QCOMPARE(dependencySchemaQuery.value(0).toInt(), 3);

        QSqlQuery activitySchemaQuery(database);
        QVERIFY(activitySchemaQuery.exec(QStringLiteral(
            "SELECT COUNT(*) FROM sqlite_master WHERE "
            "(type = 'table' AND name = 'task_activity_events') OR "
            "(type = 'index' AND name IN ("
            "'idx_task_activity_events_occurred', "
            "'idx_task_activity_events_transition_occurred', "
            "'idx_task_activity_events_task_occurred'))")));
        QVERIFY(activitySchemaQuery.next());
        QCOMPARE(activitySchemaQuery.value(0).toInt(), 4);

        QSqlQuery dependencyIndexQuery(database);
        QVERIFY(dependencyIndexQuery.exec(QStringLiteral(
            "PRAGMA index_info(idx_task_dependencies_successor)")));
        QStringList dependencyIndexColumns;
        while (dependencyIndexQuery.next()) {
            dependencyIndexColumns.append(
                dependencyIndexQuery.value(2).toString());
        }
        QCOMPARE(dependencyIndexColumns,
                 QStringList({QStringLiteral("successor_id"),
                              QStringLiteral("predecessor_id")}));

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void SqliteTaskRepositoryTest::stableSqlCodecsRoundTripEveryValue()
{
    using namespace smartmate::model::persistence::detail;

    const QList<QPair<TaskPriority, QString>> priorities{
        {TaskPriority::Low, QStringLiteral("low")},
        {TaskPriority::Normal, QStringLiteral("normal")},
        {TaskPriority::High, QStringLiteral("high")},
        {TaskPriority::Urgent, QStringLiteral("urgent")},
    };
    for (const auto &[value, text] : priorities) {
        QCOMPARE(taskPriorityToSqlText(value), text);
        QCOMPARE(taskPriorityFromSqlText(text), value);
    }

    const QList<QPair<TaskStatus, QString>> statuses{
        {TaskStatus::Todo, QStringLiteral("todo")},
        {TaskStatus::InProgress, QStringLiteral("in_progress")},
        {TaskStatus::Done, QStringLiteral("done")},
        {TaskStatus::Cancelled, QStringLiteral("cancelled")},
        {TaskStatus::Archived, QStringLiteral("archived")},
    };
    for (const auto &[value, text] : statuses) {
        QCOMPARE(taskStatusToSqlText(value), text);
        QCOMPARE(taskStatusFromSqlText(text), value);
    }

    const QList<QPair<TaskCategoryColor, QString>> colors{
        {TaskCategoryColor::Blue, QStringLiteral("blue")},
        {TaskCategoryColor::Teal, QStringLiteral("teal")},
        {TaskCategoryColor::Green, QStringLiteral("green")},
        {TaskCategoryColor::Amber, QStringLiteral("amber")},
        {TaskCategoryColor::Orange, QStringLiteral("orange")},
        {TaskCategoryColor::Rose, QStringLiteral("rose")},
        {TaskCategoryColor::Violet, QStringLiteral("violet")},
        {TaskCategoryColor::Slate, QStringLiteral("slate")},
    };
    for (const auto &[value, text] : colors) {
        QCOMPARE(taskCategoryColorToSqlText(value), text);
        QCOMPARE(taskCategoryColorFromSqlText(text), value);
    }

    const QList<QPair<TaskTransition, QString>> transitions{
        {TaskTransition::Start, QStringLiteral("start")},
        {TaskTransition::Cancel, QStringLiteral("cancel")},
        {TaskTransition::Complete, QStringLiteral("complete")},
        {TaskTransition::Redo, QStringLiteral("redo")},
        {TaskTransition::Archive, QStringLiteral("archive")},
        {TaskTransition::Restore, QStringLiteral("restore")},
    };
    for (const auto &[value, text] : transitions) {
        QCOMPARE(taskTransitionToSqlText(value), text);
        QCOMPARE(taskTransitionFromSqlText(text), value);
    }
}

void SqliteTaskRepositoryTest::stableSqlCodecsRejectInvalidValues()
{
    using namespace smartmate::model::persistence::detail;

    QVERIFY_EXCEPTION_THROWN(
        (void) taskPriorityToSqlText(static_cast<TaskPriority>(999)),
        RepositoryException);
    QVERIFY_EXCEPTION_THROWN(
        (void) taskPriorityFromSqlText(QStringLiteral("unknown")),
        RepositoryException);
    QVERIFY_EXCEPTION_THROWN(
        (void) taskStatusToSqlText(static_cast<TaskStatus>(999)),
        RepositoryException);
    QVERIFY_EXCEPTION_THROWN(
        (void) taskStatusFromSqlText(QStringLiteral("unknown")),
        RepositoryException);
    QVERIFY_EXCEPTION_THROWN(
        (void) taskCategoryColorToSqlText(static_cast<TaskCategoryColor>(999)),
        RepositoryException);
    QVERIFY_EXCEPTION_THROWN(
        (void) taskCategoryColorFromSqlText(QStringLiteral("unknown")),
        RepositoryException);
    QVERIFY_EXCEPTION_THROWN(
        (void) taskTransitionToSqlText(static_cast<TaskTransition>(999)),
        RepositoryException);
    QVERIFY_EXCEPTION_THROWN(
        (void) taskTransitionFromSqlText(QStringLiteral("unknown")),
        RepositoryException);
}

void SqliteTaskRepositoryTest::migratesVersionOneWithoutChangingTaskData()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("tasks.db"));
    const Task expected = makeTask(
        QUuid::createUuid(),
        QStringLiteral("保留 v1 中文任务 ✅"),
        TaskPriority::High,
        TaskStatus::Done,
        std::nullopt,
        QDateTime::fromMSecsSinceEpoch(1'752'086'400'789LL, QTimeZone::UTC),
        180);
    createVersionOneDatabase(databasePath, expected);

    {
        SqliteTaskRepository repository(databasePath);
        const auto migrated = repository.findById(expected.id());
        QVERIFY(migrated.has_value());
        compareTasks(*migrated, expected);
        QVERIFY(repository.findAllDependencies().isEmpty());
        QVERIFY(repository.findEventsByOccurredAt(
            kCreatedAt.addYears(-10), kUpdatedAt.addYears(10)).isEmpty());
    }

    const QString connectionName = uniqueConnectionName(QStringLiteral("verify_v2"));
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        QVERIFY2(database.open(), qPrintable(database.lastError().text()));

        QSqlQuery versionQuery(database);
        QVERIFY(versionQuery.exec(QStringLiteral("PRAGMA user_version")));
        QVERIFY(versionQuery.next());
        QCOMPARE(versionQuery.value(0).toInt(), 4);

        QSqlQuery foreignKeyQuery(database);
        QVERIFY(foreignKeyQuery.exec(QStringLiteral(
            "PRAGMA foreign_key_list(task_dependencies)")));
        int foreignKeyCount = 0;
        while (foreignKeyQuery.next()) {
            ++foreignKeyCount;
            QCOMPARE(foreignKeyQuery.value(2).toString(), QStringLiteral("tasks"));
            QCOMPARE(foreignKeyQuery.value(6).toString(), QStringLiteral("RESTRICT"));
        }
        QCOMPARE(foreignKeyCount, 2);
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void SqliteTaskRepositoryTest::migratesVersionTwoWithoutChangingTasksOrDependencies()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("tasks.db"));
    const Task predecessor = makeTask(
        QUuid::createUuid(), QStringLiteral("v2 前置"), TaskPriority::Normal,
        TaskStatus::Done);
    const Task successor = makeTask(
        QUuid::createUuid(), QStringLiteral("v2 后继"), TaskPriority::Normal,
        TaskStatus::Todo);
    createVersionTwoDatabase(databasePath, predecessor, successor);

    {
        SqliteTaskRepository repository(databasePath);
        const auto migratedPredecessor = repository.findById(predecessor.id());
        const auto migratedSuccessor = repository.findById(successor.id());
        QVERIFY(migratedPredecessor.has_value());
        QVERIFY(migratedSuccessor.has_value());
        QVERIFY(!migratedPredecessor->categoryId().has_value());
        QVERIFY(!migratedSuccessor->categoryId().has_value());
        QCOMPARE(repository.findAllDependencies(),
                 QList<TaskDependency>({{predecessor.id(), successor.id()}}));
        QVERIFY(repository.findAllCategories().isEmpty());
    }

    const QString connectionName = uniqueConnectionName(QStringLiteral("verify_v3"));
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 4);
        QVERIFY(query.exec(QStringLiteral("PRAGMA table_info(tasks)")));
        bool categoryColumnFound = false;
        while (query.next()) {
            categoryColumnFound = categoryColumnFound
                || query.value(1).toString() == QStringLiteral("category_id");
        }
        QVERIFY(categoryColumnFound);
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void SqliteTaskRepositoryTest::rollsBackFailedVersionOneMigration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("tasks.db"));
    const Task expected = makeTask(
        QUuid::createUuid(), QStringLiteral("迁移失败也不能丢失"),
        TaskPriority::High, TaskStatus::Done, std::nullopt,
        QDateTime::fromMSecsSinceEpoch(1'752'086'400'789LL, QTimeZone::UTC), 30);
    createVersionOneDatabase(databasePath, expected);

    const QString sabotageConnection =
        uniqueConnectionName(QStringLiteral("break_migration"));
    {
        auto database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), sabotageConnection);
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE task_dependencies (unexpected TEXT)")));
        database.close();
    }
    QSqlDatabase::removeDatabase(sabotageConnection);

    bool exceptionThrown = false;
    try {
        SqliteTaskRepository repository(databasePath);
    } catch (const RepositoryException &) {
        exceptionThrown = true;
    }
    QVERIFY(exceptionThrown);

    const QString verifyConnection =
        uniqueConnectionName(QStringLiteral("verify_rollback"));
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                  verifyConnection);
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        QSqlQuery versionQuery(database);
        QVERIFY(versionQuery.exec(QStringLiteral("PRAGMA user_version")));
        QVERIFY(versionQuery.next());
        QCOMPARE(versionQuery.value(0).toInt(), 1);

        QSqlQuery taskQuery(database);
        taskQuery.prepare(QStringLiteral("SELECT title FROM tasks WHERE id = :id"));
        taskQuery.bindValue(QStringLiteral(":id"),
                            expected.id().toString(QUuid::WithoutBraces));
        QVERIFY(taskQuery.exec());
        QVERIFY(taskQuery.next());
        QCOMPARE(taskQuery.value(0).toString(), expected.title());
        database.close();
    }
    QSqlDatabase::removeDatabase(verifyConnection);
}

void SqliteTaskRepositoryTest::rejectsFutureSchemaVersion()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("future.db"));
    const QString connectionName = uniqueConnectionName(QStringLiteral("future_schema"));
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version = 5")));
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    bool exceptionThrown = false;
    try {
        SqliteTaskRepository repository(databasePath);
    } catch (const RepositoryException &) {
        exceptionThrown = true;
    }
    QVERIFY(exceptionThrown);
}

void SqliteTaskRepositoryTest::roundTripsTaskCategoryAndUnicodeNameKey()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("tasks.db"));
    const TaskCategory expected{
        QUuid::createUuid(),
        QStringLiteral("Ｗｏｒｋ"),
        TaskCategoryColor::Violet,
        kCreatedAt,
        kUpdatedAt};
    const Task assignedTask = makeTask(
        QUuid::createUuid(), QStringLiteral("重启后保留类别"), TaskPriority::Normal,
        TaskStatus::Todo, std::nullopt, std::nullopt, std::nullopt,
        kUpdatedAt, expected.id);

    {
        SqliteTaskRepository repository(databasePath);
        repository.insertCategory(expected);
        const auto stored = repository.findCategoryById(expected.id);
        QVERIFY(stored.has_value());
        compareCategories(*stored, expected);

        // NFKC与大小写折叠后的唯一键相同，数据库必须拒绝绕过Service的重复写入。
        bool duplicateRejected = false;
        try {
            repository.insertCategory(TaskCategory{
                QUuid::createUuid(), QStringLiteral("work"),
                TaskCategoryColor::Blue, kCreatedAt, kUpdatedAt});
        } catch (const RepositoryException &) {
            duplicateRejected = true;
        }
        QVERIFY(duplicateRejected);

        TaskCategory renamed = expected;
        renamed.name = QStringLiteral("工作");
        renamed.color = TaskCategoryColor::Teal;
        renamed.updatedAtUtc = kUpdatedAt.addSecs(30);
        QVERIFY(repository.updateCategory(renamed));
        const auto updated = repository.findCategoryById(expected.id);
        QVERIFY(updated.has_value());
        compareCategories(*updated, renamed);

        // 50个兼容连字在NFKC后会扩展到150字符；name合法时name_key不能误设50上限。
        repository.insertCategory(TaskCategory{
            QUuid::createUuid(), QString(50, QChar(0xFB03)),
            TaskCategoryColor::Amber, kCreatedAt, kUpdatedAt});
        repository.insert(assignedTask);
        QCOMPARE(repository.findAllCategories().size(), 2);
    }

    {
        SqliteTaskRepository repository(databasePath);
        QCOMPARE(repository.findAllCategories().size(), 2);
        QVERIFY(repository.findCategoryById(expected.id).has_value());
        const auto reloadedTask = repository.findById(assignedTask.id());
        QVERIFY(reloadedTask.has_value());
        QVERIFY(reloadedTask->categoryId() == std::optional<QUuid>(expected.id));
        QVERIFY(!repository.updateCategory(TaskCategory{
            QUuid::createUuid(), QStringLiteral("不存在"),
            TaskCategoryColor::Slate, kCreatedAt, kUpdatedAt}));
    }
}

void SqliteTaskRepositoryTest::categoryForeignKeyRejectsMissingCategory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SqliteTaskRepository repository(directory.filePath(QStringLiteral("tasks.db")));
    const QUuid missingCategoryId = QUuid::createUuid();
    const Task invalid = makeTask(
        QUuid::createUuid(), QStringLiteral("悬空类别任务"), TaskPriority::Normal,
        TaskStatus::Todo, std::nullopt, std::nullopt, std::nullopt,
        kUpdatedAt, missingCategoryId);

    bool insertRejected = false;
    try {
        repository.insert(invalid);
    } catch (const RepositoryException &) {
        insertRejected = true;
    }
    QVERIFY(insertRejected);
    QVERIFY(!repository.findById(invalid.id()).has_value());

    const TaskCategory category{
        QUuid::createUuid(), QStringLiteral("学习"), TaskCategoryColor::Green,
        kCreatedAt, kUpdatedAt};
    repository.insertCategory(category);
    const Task valid = makeTask(
        invalid.id(), invalid.title(), TaskPriority::Normal, TaskStatus::Todo,
        std::nullopt, std::nullopt, std::nullopt, kUpdatedAt, category.id);
    repository.insert(valid);
    const auto stored = repository.findById(valid.id());
    QVERIFY(stored.has_value());
    QVERIFY(stored->categoryId() == std::optional<QUuid>(category.id));
}

void SqliteTaskRepositoryTest::deletesCategoryAndUnassignsTasksWithoutChangingDependencies()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("tasks.db"));
    const TaskCategory category{
        QUuid::createUuid(), QStringLiteral("项目"), TaskCategoryColor::Orange,
        kCreatedAt, kUpdatedAt};
    const QDateTime deletionTime = kUpdatedAt.addSecs(90);
    const Task predecessor = makeTask(
        QUuid::createUuid(), QStringLiteral("前置"), TaskPriority::Normal,
        TaskStatus::Done, std::nullopt, std::nullopt, std::nullopt,
        kUpdatedAt, category.id);
    const Task successor = makeTask(
        QUuid::createUuid(), QStringLiteral("后继"), TaskPriority::Normal,
        TaskStatus::Todo, std::nullopt, std::nullopt, std::nullopt,
        kUpdatedAt, category.id);
    const Task archived = makeTask(
        QUuid::createUuid(), QStringLiteral("归档"), TaskPriority::Normal,
        TaskStatus::Archived, TaskStatus::Cancelled, std::nullopt, std::nullopt,
        kUpdatedAt, category.id);

    {
        SqliteTaskRepository repository(databasePath);
        repository.insertCategory(category);
        repository.insert(predecessor);
        repository.insert(successor);
        repository.insert(archived);
        repository.replacePredecessors(successor.id(), {predecessor.id()});
        const QList<TaskDependency> dependenciesBefore =
            repository.findAllDependencies();

        const auto outcome = repository.deleteCategoryAndUnassignTasks(
            category.id, deletionTime);
        QVERIFY(outcome.categoryDeleted);
        QCOMPARE(outcome.unassignedTaskCount, 3);
        QVERIFY(!repository.findCategoryById(category.id).has_value());
        QCOMPARE(repository.findAllDependencies(), dependenciesBefore);

        for (const TaskId &id : {predecessor.id(), successor.id(), archived.id()}) {
            const auto task = repository.findById(id);
            QVERIFY(task.has_value());
            QVERIFY(!task->categoryId().has_value());
            QCOMPARE(task->updatedAtUtc(), deletionTime);
        }
        QCOMPARE(repository.findById(predecessor.id())->status(), TaskStatus::Done);
        QCOMPARE(repository.findById(successor.id())->status(), TaskStatus::Todo);
        QCOMPARE(repository.findById(archived.id())->status(), TaskStatus::Archived);

        const auto missingOutcome = repository.deleteCategoryAndUnassignTasks(
            QUuid::createUuid(), deletionTime.addSecs(1));
        QVERIFY(!missingOutcome.categoryDeleted);
        QCOMPARE(missingOutcome.unassignedTaskCount, 0);
    }

    {
        SqliteTaskRepository repository(databasePath);
        QVERIFY(!repository.findCategoryById(category.id).has_value());
        QCOMPARE(repository.findAllDependencies(),
                 QList<TaskDependency>({{predecessor.id(), successor.id()}}));
        QVERIFY(!repository.findById(successor.id())->categoryId().has_value());
    }
}

void SqliteTaskRepositoryTest::categoryDeletionRollsBackAtomically()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("tasks.db"));
    SqliteTaskRepository repository(databasePath);
    const TaskCategory category{
        QUuid::createUuid(), QStringLiteral("不可删除"), TaskCategoryColor::Rose,
        kCreatedAt, kUpdatedAt};
    const Task task = makeTask(
        QUuid::createUuid(), QStringLiteral("必须回滚归属"), TaskPriority::Normal,
        TaskStatus::Todo, std::nullopt, std::nullopt, std::nullopt,
        kUpdatedAt, category.id);
    repository.insertCategory(category);
    repository.insert(task);

    const QString connectionName = uniqueConnectionName(QStringLiteral("category_sabotage"));
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        QSqlQuery query(database);
        QVERIFY2(query.exec(QStringLiteral(
                     "CREATE TRIGGER reject_category_delete "
                     "BEFORE DELETE ON task_categories BEGIN "
                     "SELECT RAISE(ABORT, 'test category delete failure'); END")),
                 qPrintable(query.lastError().text()));
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    bool deletionRejected = false;
    try {
        (void) repository.deleteCategoryAndUnassignTasks(
            category.id, kUpdatedAt.addSecs(300));
    } catch (const RepositoryException &) {
        deletionRejected = true;
    }
    QVERIFY(deletionRejected);
    QVERIFY(repository.findCategoryById(category.id).has_value());
    const auto storedTask = repository.findById(task.id());
    QVERIFY(storedTask.has_value());
    QVERIFY(storedTask->categoryId() == std::optional<QUuid>(category.id));
    QCOMPARE(storedTask->updatedAtUtc(), task.updatedAtUtc());
}

void SqliteTaskRepositoryTest::roundTripsCompleteUnicodeTask()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SqliteTaskRepository repository(directory.filePath(QStringLiteral("tasks.db")));

    const auto deadline =
        QDateTime::fromMSecsSinceEpoch(1'752'086'400'789LL, QTimeZone::UTC);
    const Task expected = makeTask(
        QUuid::createUuid(),
        QStringLiteral("完成课程大作业 🧭"),
        TaskPriority::Urgent,
        TaskStatus::Done,
        std::nullopt,
        deadline,
        180);

    repository.insert(expected);

    const auto actual = repository.findById(expected.id());
    QVERIFY(actual.has_value());
    compareTasks(*actual, expected);
}

void SqliteTaskRepositoryTest::roundTripsNullOptionals()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SqliteTaskRepository repository(directory.filePath(QStringLiteral("tasks.db")));

    const Task expected = makeTask(QUuid::createUuid());
    repository.insert(expected);

    const auto actual = repository.findById(expected.id());
    QVERIFY(actual.has_value());
    QVERIFY(!actual->statusBeforeArchive().has_value());
    QVERIFY(!actual->deadline().has_value());
    QVERIFY(!actual->estimatedMinutes().has_value());
    compareTasks(*actual, expected);
}

void SqliteTaskRepositoryTest::storesOmittedDescriptionAsEmptyText()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SqliteTaskRepository repository(directory.filePath(QStringLiteral("tasks.db")));

    // 新建表单未填写可选描述时会产生 null QString；持久化层必须把它映射为
    // 空文本，而不是违反 description NOT NULL 约束的 SQL NULL。
    const Task taskWithoutDescription{
        QUuid::createUuid(),
        QStringLiteral("只填写标题的任务"),
        QString{},
        TaskPriority::Normal,
        TaskStatus::Todo,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        kCreatedAt,
        kUpdatedAt};

    repository.insert(taskWithoutDescription);

    const auto stored = repository.findById(taskWithoutDescription.id());
    QVERIFY(stored.has_value());
    QVERIFY(stored->description().isEmpty());
    QVERIFY(!stored->description().isNull());
}

void SqliteTaskRepositoryTest::persistsAcrossRepositoryReopen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("tasks.db"));
    const Task expected = makeTask(
        QUuid::createUuid(),
        QStringLiteral("关闭后仍需存在"),
        TaskPriority::High,
        TaskStatus::Cancelled);

    {
        SqliteTaskRepository repository(databasePath);
        repository.insert(expected);
    }
    {
        SqliteTaskRepository repository(databasePath);
        const auto actual = repository.findById(expected.id());
        QVERIFY(actual.has_value());
        compareTasks(*actual, expected);
    }
}

void SqliteTaskRepositoryTest::updatesArchivedTaskAndOriginalStatus()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SqliteTaskRepository repository(directory.filePath(QStringLiteral("tasks.db")));

    const auto id = QUuid::createUuid();
    repository.insert(makeTask(
        id,
        QStringLiteral("正在处理的任务"),
        TaskPriority::High,
        TaskStatus::InProgress));

    const Task archived = makeTask(
        id,
        QStringLiteral("已归档的任务"),
        TaskPriority::High,
        TaskStatus::Archived,
        TaskStatus::InProgress,
        std::nullopt,
        90,
        kUpdatedAt.addSecs(60));
    QVERIFY(repository.update(archived));

    const auto actual = repository.findById(id);
    QVERIFY(actual.has_value());
    compareTasks(*actual, archived);
}

void SqliteTaskRepositoryTest::rejectsSecondInProgressTask()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SqliteTaskRepository repository(directory.filePath(QStringLiteral("tasks.db")));

    repository.insert(makeTask(
        QUuid::createUuid(),
        QStringLiteral("第一个进行中任务"),
        TaskPriority::Normal,
        TaskStatus::InProgress));

    bool exceptionThrown = false;
    try {
        repository.insert(makeTask(
            QUuid::createUuid(),
            QStringLiteral("第二个进行中任务"),
            TaskPriority::Normal,
            TaskStatus::InProgress));
    } catch (const RepositoryException &) {
        exceptionThrown = true;
    }

    QVERIFY(exceptionThrown);
    QCOMPARE(repository.findAll().size(), 1);
}

void SqliteTaskRepositoryTest::returnsFalseWhenUpdatingMissingTask()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SqliteTaskRepository repository(directory.filePath(QStringLiteral("tasks.db")));

    QVERIFY(!repository.update(makeTask(QUuid::createUuid())));
}

void SqliteTaskRepositoryTest::roundTripsDependenciesAcrossRepositoryReopen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("tasks.db"));
    const QUuid first = QUuid::createUuid();
    const QUuid second = QUuid::createUuid();
    const QUuid successor = QUuid::createUuid();

    {
        SqliteTaskRepository repository(databasePath);
        repository.insert(makeTask(first, QStringLiteral("前置一")));
        repository.insert(makeTask(second, QStringLiteral("前置二")));
        repository.insert(makeTask(successor, QStringLiteral("后继")));
        repository.replacePredecessors(successor, {second, first});
    }
    {
        SqliteTaskRepository repository(databasePath);
        QList<TaskDependency> dependencies = repository.findAllDependencies();
        QCOMPARE(dependencies.size(), 2);
        QVERIFY(dependencies.contains(TaskDependency{first, successor}));
        QVERIFY(dependencies.contains(TaskDependency{second, successor}));

        repository.replacePredecessors(successor, {second});
        QCOMPARE(repository.findAllDependencies(),
                 QList<TaskDependency>({TaskDependency{second, successor}}));
        repository.replacePredecessors(successor, {});
        QVERIFY(repository.findAllDependencies().isEmpty());
    }
}

void SqliteTaskRepositoryTest::enforcesDependencyIdentityForeignKeysAndCycles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SqliteTaskRepository repository(directory.filePath(QStringLiteral("tasks.db")));
    const QUuid first = QUuid::createUuid();
    const QUuid second = QUuid::createUuid();
    const QUuid third = QUuid::createUuid();
    const QUuid missing = QUuid::createUuid();
    repository.insert(makeTask(first));
    repository.insert(makeTask(second));
    repository.insert(makeTask(third));

    QVERIFY(dependencyWriteThrows(repository, first, {first}));
    QVERIFY(dependencyWriteThrows(repository, second, {missing}));
    QVERIFY(dependencyWriteThrows(repository, missing, {first}));
    QVERIFY(dependencyWriteThrows(repository, second, {first, first}));
    QVERIFY(repository.findAllDependencies().isEmpty());

    repository.replacePredecessors(second, {first});
    repository.replacePredecessors(third, {second});
    QVERIFY(dependencyWriteThrows(repository, first, {third}));
    QCOMPARE(repository.findAllDependencies().size(), 2);
    QVERIFY(repository.findAllDependencies().contains(TaskDependency{first, second}));
    QVERIFY(repository.findAllDependencies().contains(TaskDependency{second, third}));
}

void SqliteTaskRepositoryTest::restrictsDeletingTasksReferencedByDependencies()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("tasks.db"));
    SqliteTaskRepository repository(databasePath);
    const QUuid predecessor = QUuid::createUuid();
    const QUuid successor = QUuid::createUuid();
    repository.insert(makeTask(predecessor));
    repository.insert(makeTask(successor));
    repository.replacePredecessors(successor, {predecessor});

    const QString connectionName =
        uniqueConnectionName(QStringLiteral("verify_restrict"));
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        QSqlQuery pragmaQuery(database);
        QVERIFY(pragmaQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON")));

        QSqlQuery deleteQuery(database);
        deleteQuery.prepare(QStringLiteral("DELETE FROM tasks WHERE id = :id"));
        deleteQuery.bindValue(
            QStringLiteral(":id"),
            predecessor.toString(QUuid::WithoutBraces));
        QVERIFY(!deleteQuery.exec());
        QVERIFY(deleteQuery.lastError().isValid());
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    QVERIFY(repository.findById(predecessor).has_value());
    QVERIFY(repository.findById(successor).has_value());
    QCOMPARE(repository.findAllDependencies(),
             QList<TaskDependency>({TaskDependency{predecessor, successor}}));
}

void SqliteTaskRepositoryTest::replacePredecessorsRollsBackAtomically()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SqliteTaskRepository repository(directory.filePath(QStringLiteral("tasks.db")));
    const QUuid original = QUuid::createUuid();
    const QUuid replacement = QUuid::createUuid();
    const QUuid successor = QUuid::createUuid();
    const QUuid missing = QUuid::createUuid();
    repository.insert(makeTask(original));
    repository.insert(makeTask(replacement));
    repository.insert(makeTask(successor));
    repository.replacePredecessors(successor, {original});

    QVERIFY(dependencyWriteThrows(repository, successor, {replacement, missing}));
    QCOMPARE(repository.findAllDependencies(),
             QList<TaskDependency>({TaskDependency{original, successor}}));
}

void SqliteTaskRepositoryTest::atomicallyCreatesTaskWithPredecessorsAcrossReopen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("tasks.db"));
    const QUuid first = QUuid::createUuid();
    const QUuid second = QUuid::createUuid();
    const Task successor = makeTask(
        QUuid::createUuid(), QStringLiteral("原子创建的后继任务"));

    {
        SqliteTaskRepository repository(databasePath);
        repository.insert(makeTask(first, QStringLiteral("需求确认"),
                                   TaskPriority::High, TaskStatus::Done));
        repository.insert(makeTask(second, QStringLiteral("接口设计")));
        repository.insertTaskWithPredecessors(successor, {second, first});

        const auto stored = repository.findById(successor.id());
        QVERIFY(stored.has_value());
        compareTasks(*stored, successor);
        const QList<TaskDependency> dependencies =
            repository.findAllDependencies();
        QCOMPARE(dependencies.size(), 2);
        QVERIFY(dependencies.contains(TaskDependency{first, successor.id()}));
        QVERIFY(dependencies.contains(TaskDependency{second, successor.id()}));
    }

    // 重开数据库验证事务提交结果确实落盘，而不是只存在于连接缓存中。
    {
        SqliteTaskRepository repository(databasePath);
        QVERIFY(repository.findById(successor.id()).has_value());
        const QList<TaskDependency> dependencies =
            repository.findAllDependencies();
        QCOMPARE(dependencies.size(), 2);
        QVERIFY(dependencies.contains(TaskDependency{first, successor.id()}));
        QVERIFY(dependencies.contains(TaskDependency{second, successor.id()}));
    }
}

void SqliteTaskRepositoryTest::atomicCreationRollsBackTaskAfterMidwayEdgeFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SqliteTaskRepository repository(directory.filePath(QStringLiteral("tasks.db")));
    const QUuid validPredecessor = QUuid::createUuid();
    const QUuid missingPredecessor = QUuid::createUuid();
    const Task successor = makeTask(
        QUuid::createUuid(), QStringLiteral("不能留下半成品"));
    repository.insert(makeTask(validPredecessor, QStringLiteral("有效前置")));

    // 第一条边已可成功执行，第二条边触发外键失败；整个事务必须连同任务一起撤销。
    QVERIFY(atomicCreationThrows(
        repository, successor, {validPredecessor, missingPredecessor}));
    QVERIFY(!repository.findById(successor.id()).has_value());
    QVERIFY(repository.findById(validPredecessor).has_value());
    QVERIFY(repository.findAllDependencies().isEmpty());
}

void SqliteTaskRepositoryTest::atomicCreationRollsBackOnTaskOrSelfDependencyConstraint()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SqliteTaskRepository repository(directory.filePath(QStringLiteral("tasks.db")));
    const Task existing = makeTask(
        QUuid::createUuid(), QStringLiteral("已存在任务"));
    repository.insert(existing);

    const Task duplicateId = makeTask(
        existing.id(), QStringLiteral("重复ID不能覆盖原任务"));
    QVERIFY(atomicCreationThrows(repository, duplicateId, {}));
    const auto unchanged = repository.findById(existing.id());
    QVERIFY(unchanged.has_value());
    compareTasks(*unchanged, existing);

    const Task selfDependent = makeTask(
        QUuid::createUuid(), QStringLiteral("自依赖必须回滚"));
    QVERIFY(atomicCreationThrows(
        repository, selfDependent, {selfDependent.id()}));
    QVERIFY(!repository.findById(selfDependent.id()).has_value());
    QVERIFY(repository.findAllDependencies().isEmpty());
}

void SqliteTaskRepositoryTest::batchStateChangesCommitAcrossReopen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("tasks.db"));
    const QUuid doneId = QUuid::createUuid();
    const QUuid cancelledId = QUuid::createUuid();
    const QDateTime batchUpdatedAt = kUpdatedAt.addSecs(90);
    const QDateTime restoredAt = kUpdatedAt.addSecs(180);

    {
        SqliteTaskRepository repository(databasePath);
        repository.insert(makeTask(doneId, QStringLiteral("批量归档完成任务"),
                                   TaskPriority::Normal, TaskStatus::Done));
        repository.insert(makeTask(cancelledId, QStringLiteral("批量归档取消任务"),
                                   TaskPriority::Normal, TaskStatus::Cancelled));

        const auto result = repository.applyTransitionsAtomically({
            transitionWrite(
                TaskStateChange{doneId, TaskStatus::Done, TaskStatus::Archived,
                                TaskStatus::Done, batchUpdatedAt},
                TaskTransition::Archive),
            transitionWrite(
                TaskStateChange{cancelledId, TaskStatus::Cancelled,
                                TaskStatus::Archived, TaskStatus::Cancelled,
                                batchUpdatedAt},
                TaskTransition::Archive),
        });
        QCOMPARE(result.updatedTaskCount, 2);
        QCOMPARE(result.insertedEventCount, 2);
        QVERIFY(result.conflictingTaskIds.isEmpty());
    }

    // 重开数据库验证批量事务的状态、归档前状态与统一更新时间已共同提交。
    {
        SqliteTaskRepository repository(databasePath);
        const auto done = repository.findById(doneId);
        const auto cancelled = repository.findById(cancelledId);
        QVERIFY(done.has_value());
        QVERIFY(cancelled.has_value());
        QCOMPARE(done->status(), TaskStatus::Archived);
        QCOMPARE(done->statusBeforeArchive(),
                 std::optional<TaskStatus>{TaskStatus::Done});
        QCOMPARE(done->updatedAtUtc(), batchUpdatedAt);
        QCOMPARE(cancelled->status(), TaskStatus::Archived);
        QCOMPARE(cancelled->statusBeforeArchive(),
                 std::optional<TaskStatus>{TaskStatus::Cancelled});
        QCOMPARE(cancelled->updatedAtUtc(), batchUpdatedAt);

        const auto restored = repository.applyTransitionsAtomically({
            transitionWrite(
                TaskStateChange{doneId, TaskStatus::Archived, TaskStatus::Done,
                                std::nullopt, restoredAt},
                TaskTransition::Restore),
            transitionWrite(
                TaskStateChange{cancelledId, TaskStatus::Archived,
                                TaskStatus::Cancelled, std::nullopt, restoredAt},
                TaskTransition::Restore),
        });
        QCOMPARE(restored.updatedTaskCount, 2);
        QVERIFY(restored.conflictingTaskIds.isEmpty());
    }

    {
        SqliteTaskRepository repository(databasePath);
        const auto done = repository.findById(doneId);
        const auto cancelled = repository.findById(cancelledId);
        QVERIFY(done.has_value());
        QVERIFY(cancelled.has_value());
        QCOMPARE(done->status(), TaskStatus::Done);
        QVERIFY(!done->statusBeforeArchive().has_value());
        QCOMPARE(done->updatedAtUtc(), restoredAt);
        QCOMPARE(cancelled->status(), TaskStatus::Cancelled);
        QVERIFY(!cancelled->statusBeforeArchive().has_value());
        QCOMPARE(cancelled->updatedAtUtc(), restoredAt);
    }
}

void SqliteTaskRepositoryTest::batchStateChangesRollBackOnConflictOrSqlFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("tasks.db"));
    const QUuid firstId = QUuid::createUuid();
    const QUuid conflictId = QUuid::createUuid();
    const QUuid triggerId = QUuid::createUuid();
    SqliteTaskRepository repository(databasePath);
    repository.insert(makeTask(firstId, QStringLiteral("事务首项"),
                               TaskPriority::Normal, TaskStatus::Done));
    repository.insert(makeTask(conflictId, QStringLiteral("状态竞争项"),
                               TaskPriority::Normal, TaskStatus::Todo));
    repository.insert(makeTask(triggerId, QStringLiteral("SQL失败项"),
                               TaskPriority::Normal, TaskStatus::Done));

    const auto conflictResult = repository.applyTransitionsAtomically({
        transitionWrite(
            TaskStateChange{firstId, TaskStatus::Done, TaskStatus::Archived,
                            TaskStatus::Done, kUpdatedAt.addSecs(60)},
            TaskTransition::Archive),
        transitionWrite(
            TaskStateChange{conflictId, TaskStatus::Done, TaskStatus::Archived,
                            TaskStatus::Done, kUpdatedAt.addSecs(60)},
            TaskTransition::Archive),
    });
    QCOMPARE(conflictResult.updatedTaskCount, 0);
    QCOMPARE(conflictResult.conflictingTaskIds, QList<QUuid>{conflictId});
    QCOMPARE(repository.findById(firstId)->status(), TaskStatus::Done);
    QCOMPARE(repository.findById(conflictId)->status(), TaskStatus::Todo);
    QVERIFY(repository.findEventsByOccurredAt(
        kUpdatedAt, kUpdatedAt.addSecs(90)).isEmpty());

    const QString connectionName =
        uniqueConnectionName(QStringLiteral("batch_update_failure_trigger"));
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                  connectionName);
        database.setDatabaseName(databasePath);
        QVERIFY2(database.open(), qPrintable(database.lastError().text()));
        QSqlQuery triggerQuery(database);
        const QString statement = QStringLiteral(
            "CREATE TRIGGER fail_batch_task_update "
            "BEFORE UPDATE ON tasks WHEN OLD.id = '%1' "
            "BEGIN SELECT RAISE(ABORT, 'simulated batch update failure'); END")
                                      .arg(triggerId.toString(QUuid::WithoutBraces));
        QVERIFY2(triggerQuery.exec(statement),
                 qPrintable(triggerQuery.lastError().text()));
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    bool threw = false;
    try {
        (void) repository.applyTransitionsAtomically({
            transitionWrite(
                TaskStateChange{firstId, TaskStatus::Done, TaskStatus::Archived,
                                TaskStatus::Done, kUpdatedAt.addSecs(120)},
                TaskTransition::Archive),
            transitionWrite(
                TaskStateChange{triggerId, TaskStatus::Done, TaskStatus::Archived,
                                TaskStatus::Done, kUpdatedAt.addSecs(120)},
                TaskTransition::Archive),
        });
    } catch (const RepositoryException &) {
        threw = true;
    }
    QVERIFY(threw);
    QCOMPARE(repository.findById(firstId)->status(), TaskStatus::Done);
    QCOMPARE(repository.findById(triggerId)->status(), TaskStatus::Done);
    QVERIFY(repository.findEventsByOccurredAt(
        kUpdatedAt, kUpdatedAt.addSecs(180)).isEmpty());
}

void SqliteTaskRepositoryTest::activityEventsRoundTripAndCascadeOnPermanentDelete()
{
    SqliteTaskRepository repository(QStringLiteral(":memory:"));
    const TaskCategory category{QUuid::createUuid(),
                                QStringLiteral("历史类别"),
                                TaskCategoryColor::Violet,
                                kCreatedAt,
                                kUpdatedAt};
    repository.insertCategory(category);
    const TaskId taskId = QUuid::createUuid();
    const QDateTime deadline = kUpdatedAt.addSecs(30);
    repository.insert(makeTask(taskId,
                               QStringLiteral("事件任务"),
                               TaskPriority::Urgent,
                               TaskStatus::InProgress,
                               std::nullopt,
                               deadline,
                               75,
                               kUpdatedAt,
                               category.id));

    TaskTransitionWrite completed = transitionWrite(
        {taskId, TaskStatus::InProgress, TaskStatus::Done,
         std::nullopt, kUpdatedAt.addSecs(10)},
        TaskTransition::Complete);
    completed.event.deadlineSnapshotUtc = deadline;
    completed.event.estimatedMinutesSnapshot = 75;
    completed.event.prioritySnapshot = TaskPriority::Urgent;
    completed.event.categoryIdSnapshot = category.id;
    completed.event.categoryNameSnapshot = category.name;
    completed.event.categoryColorSnapshot = category.color;
    const auto completeResult = repository.applyTransitionsAtomically({completed});
    QCOMPARE(completeResult.updatedTaskCount, 1);
    QCOMPARE(completeResult.insertedEventCount, 1);

    TaskTransitionWrite archived = transitionWrite(
        {taskId, TaskStatus::Done, TaskStatus::Archived,
         TaskStatus::Done, kUpdatedAt.addSecs(20)},
        TaskTransition::Archive);
    const auto archiveResult = repository.applyTransitionsAtomically({archived});
    QCOMPARE(archiveResult.updatedTaskCount, 1);
    QCOMPARE(archiveResult.insertedEventCount, 1);

    const auto events = repository.findEventsByOccurredAt(
        kUpdatedAt, kUpdatedAt.addSecs(30));
    QCOMPARE(events.size(), 2);
    QCOMPARE(events.first().transition, TaskTransition::Complete);
    QCOMPARE(events.first().deadlineSnapshotUtc,
             std::optional<QDateTime>{deadline});
    QCOMPARE(events.first().estimatedMinutesSnapshot,
             std::optional<int>{75});
    QCOMPARE(events.first().prioritySnapshot, TaskPriority::Urgent);
    QCOMPARE(events.first().categoryNameSnapshot,
             std::optional<QString>{category.name});
    QCOMPARE(repository.findLatestCompletionBefore(kUpdatedAt.addSecs(30)),
             std::optional{events.first()});

    const auto deletion = repository.deleteArchivedTasksWithDependencies({taskId});
    QCOMPARE(deletion.deletedTaskCount, 1);
    QVERIFY(repository.findEventsByOccurredAt(
        kUpdatedAt, kUpdatedAt.addSecs(30)).isEmpty());
    QVERIFY(!repository.findLatestCompletionBefore(
        kUpdatedAt.addSecs(30)).has_value());
}

void SqliteTaskRepositoryTest::permanentlyDeletesArchivedTaskAndAllDependenciesAcrossReopen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("tasks.db"));
    const QUuid predecessor = QUuid::createUuid();
    const QUuid archivedId = QUuid::createUuid();
    const QUuid secondArchivedId = QUuid::createUuid();
    const QUuid successor = QUuid::createUuid();

    {
        SqliteTaskRepository repository(databasePath);
        repository.insert(makeTask(predecessor, QStringLiteral("保留前置"),
                                   TaskPriority::Normal, TaskStatus::Done));
        repository.insert(makeTask(archivedId, QStringLiteral("永久删除目标"),
                                   TaskPriority::Normal, TaskStatus::Archived,
                                   TaskStatus::Done));
        repository.insert(makeTask(secondArchivedId,
                                   QStringLiteral("共享边的第二个删除目标"),
                                   TaskPriority::Normal, TaskStatus::Archived,
                                   TaskStatus::Cancelled));
        repository.insert(makeTask(successor, QStringLiteral("保留后继")));
        repository.replacePredecessors(archivedId, {predecessor});
        repository.replacePredecessors(secondArchivedId, {archivedId});
        repository.replacePredecessors(successor,
                                       {secondArchivedId, predecessor});

        const auto result =
            repository.deleteArchivedTasksWithDependencies(
                {archivedId, secondArchivedId});
        QCOMPARE(result.deletedTaskCount, 2);
        QCOMPARE(result.removedDependencyCount, 3);
        QVERIFY(result.conflictingTaskIds.isEmpty());
        QVERIFY(!repository.findById(archivedId).has_value());
        QVERIFY(!repository.findById(secondArchivedId).has_value());
        QVERIFY(repository.findById(predecessor).has_value());
        QVERIFY(repository.findById(successor).has_value());
        QCOMPARE(repository.findAllDependencies(),
                 QList<TaskDependency>({TaskDependency{predecessor, successor}}));
    }

    // 重开数据库确认任务与关联边已经提交，而无关任务和无关边仍完整保留。
    {
        SqliteTaskRepository repository(databasePath);
        QVERIFY(!repository.findById(archivedId).has_value());
        QVERIFY(!repository.findById(secondArchivedId).has_value());
        QVERIFY(repository.findById(predecessor).has_value());
        QVERIFY(repository.findById(successor).has_value());
        QCOMPARE(repository.findAllDependencies(),
                 QList<TaskDependency>({TaskDependency{predecessor, successor}}));
    }
}

void SqliteTaskRepositoryTest::rejectsPermanentDeletionOfActiveOrMissingTaskAtomically()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SqliteTaskRepository repository(directory.filePath(QStringLiteral("tasks.db")));
    const QUuid predecessor = QUuid::createUuid();
    const QUuid archivedId = QUuid::createUuid();
    const QUuid activeId = QUuid::createUuid();
    repository.insert(makeTask(predecessor));
    repository.insert(makeTask(archivedId, QStringLiteral("应随整批回滚的归档任务"),
                               TaskPriority::Normal, TaskStatus::Archived,
                               TaskStatus::Done));
    repository.insert(makeTask(activeId, QStringLiteral("仍是活动任务")));
    repository.replacePredecessors(archivedId, {predecessor});
    repository.replacePredecessors(activeId, {archivedId});

    const auto activeResult =
        repository.deleteArchivedTasksWithDependencies({archivedId, activeId});
    QCOMPARE(activeResult.deletedTaskCount, 0);
    QCOMPARE(activeResult.removedDependencyCount, 0);
    QCOMPARE(activeResult.conflictingTaskIds, QList<QUuid>{activeId});
    QVERIFY(repository.findById(archivedId).has_value());
    QVERIFY(repository.findById(activeId).has_value());
    const auto dependenciesAfterActiveConflict = repository.findAllDependencies();
    QCOMPARE(dependenciesAfterActiveConflict.size(), 2);
    QVERIFY(dependenciesAfterActiveConflict.contains(
        TaskDependency{predecessor, archivedId}));
    QVERIFY(dependenciesAfterActiveConflict.contains(
        TaskDependency{archivedId, activeId}));

    const QUuid missingId = QUuid::createUuid();
    const auto missingResult = repository.deleteArchivedTasksWithDependencies(
        {missingId});
    QCOMPARE(missingResult.deletedTaskCount, 0);
    QCOMPARE(missingResult.removedDependencyCount, 0);
    QCOMPARE(missingResult.conflictingTaskIds, QList<QUuid>{missingId});
    const auto dependenciesAfterMissingConflict = repository.findAllDependencies();
    QCOMPARE(dependenciesAfterMissingConflict.size(), 2);
    QVERIFY(dependenciesAfterMissingConflict.contains(
        TaskDependency{predecessor, archivedId}));
    QVERIFY(dependenciesAfterMissingConflict.contains(
        TaskDependency{archivedId, activeId}));
}

void SqliteTaskRepositoryTest::permanentDeletionRollsBackAfterTaskDeleteFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("tasks.db"));
    const QUuid predecessor = QUuid::createUuid();
    const QUuid firstArchivedId = QUuid::createUuid();
    const QUuid archivedId = QUuid::createUuid();
    SqliteTaskRepository repository(databasePath);
    repository.insert(makeTask(predecessor));
    repository.insert(makeTask(firstArchivedId, QStringLiteral("触发失败前已删除项"),
                               TaskPriority::Normal, TaskStatus::Archived,
                               TaskStatus::Done));
    repository.insert(makeTask(archivedId, QStringLiteral("触发回滚的归档任务"),
                               TaskPriority::Normal, TaskStatus::Archived,
                               TaskStatus::Cancelled));
    repository.replacePredecessors(firstArchivedId, {predecessor});
    repository.replacePredecessors(archivedId, {firstArchivedId});

    const QString connectionName =
        uniqueConnectionName(QStringLiteral("delete_failure_trigger"));
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                  connectionName);
        database.setDatabaseName(databasePath);
        QVERIFY2(database.open(), qPrintable(database.lastError().text()));
        QSqlQuery triggerQuery(database);
        const QString statement = QStringLiteral(
            "CREATE TRIGGER fail_archived_task_delete "
            "BEFORE DELETE ON tasks WHEN OLD.id = '%1' "
            "BEGIN SELECT RAISE(ABORT, 'simulated task delete failure'); END")
                                      .arg(archivedId.toString(QUuid::WithoutBraces));
        QVERIFY2(triggerQuery.exec(statement),
                 qPrintable(triggerQuery.lastError().text()));
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    // 依赖DELETE已经执行后任务DELETE触发失败；事务必须恢复两张表。
    QVERIFY(permanentDeletionThrows(repository, {firstArchivedId, archivedId}));
    QVERIFY(repository.findById(firstArchivedId).has_value());
    QVERIFY(repository.findById(archivedId).has_value());
    QVERIFY(repository.findById(predecessor).has_value());
    const auto dependencies = repository.findAllDependencies();
    QCOMPARE(dependencies.size(), 2);
    QVERIFY(dependencies.contains(TaskDependency{predecessor, firstArchivedId}));
    QVERIFY(dependencies.contains(TaskDependency{firstArchivedId, archivedId}));
}

QTEST_MAIN(SqliteTaskRepositoryTest)

#include "tst_SqliteTaskRepository.moc"
