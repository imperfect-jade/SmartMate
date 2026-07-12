#include "persistence/SqliteTaskRepository.h"

#include "domain/TaskDependency.h"

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
using smartmate::model::TaskDependency;
using smartmate::model::TaskPriority;
using smartmate::model::TaskStateChange;
using smartmate::model::TaskStatus;
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
    QDateTime updatedAt = kUpdatedAt)
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
        updatedAt);
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
    QCOMPARE(actual.createdAtUtc(), expected.createdAtUtc());
    QCOMPARE(actual.updatedAtUtc(), expected.updatedAtUtc());
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
    void rollsBackFailedVersionOneMigration();
    void rejectsFutureSchemaVersion();
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
        QCOMPARE(versionQuery.value(0).toInt(), 2);

        QSqlQuery indexQuery(database);
        QVERIFY(indexQuery.exec(QStringLiteral(
            "SELECT COUNT(*) FROM sqlite_master "
            "WHERE type = 'index' AND name IN ("
            "'idx_tasks_status', 'idx_tasks_deadline', 'idx_tasks_single_in_progress')")));
        QVERIFY(indexQuery.next());
        QCOMPARE(indexQuery.value(0).toInt(), 3);

        QSqlQuery dependencySchemaQuery(database);
        QVERIFY(dependencySchemaQuery.exec(QStringLiteral(
            "SELECT COUNT(*) FROM sqlite_master WHERE "
            "(type = 'table' AND name = 'task_dependencies') OR "
            "(type = 'index' AND name = 'idx_task_dependencies_successor') OR "
            "(type = 'trigger' AND name = 'trg_task_dependencies_prevent_cycle')")));
        QVERIFY(dependencySchemaQuery.next());
        QCOMPARE(dependencySchemaQuery.value(0).toInt(), 3);

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
    }

    const QString connectionName = uniqueConnectionName(QStringLiteral("verify_v2"));
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        QVERIFY2(database.open(), qPrintable(database.lastError().text()));

        QSqlQuery versionQuery(database);
        QVERIFY(versionQuery.exec(QStringLiteral("PRAGMA user_version")));
        QVERIFY(versionQuery.next());
        QCOMPARE(versionQuery.value(0).toInt(), 2);

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
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version = 3")));
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

        const auto result = repository.updateTaskStatesAtomically({
            TaskStateChange{doneId, TaskStatus::Done, TaskStatus::Archived,
                            TaskStatus::Done, batchUpdatedAt},
            TaskStateChange{cancelledId, TaskStatus::Cancelled,
                            TaskStatus::Archived, TaskStatus::Cancelled,
                            batchUpdatedAt},
        });
        QCOMPARE(result.updatedTaskCount, 2);
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

        const auto restored = repository.updateTaskStatesAtomically({
            TaskStateChange{doneId, TaskStatus::Archived, TaskStatus::Done,
                            std::nullopt, restoredAt},
            TaskStateChange{cancelledId, TaskStatus::Archived,
                            TaskStatus::Cancelled, std::nullopt, restoredAt},
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

    const auto conflictResult = repository.updateTaskStatesAtomically({
        TaskStateChange{firstId, TaskStatus::Done, TaskStatus::Archived,
                        TaskStatus::Done, kUpdatedAt.addSecs(60)},
        TaskStateChange{conflictId, TaskStatus::Done, TaskStatus::Archived,
                        TaskStatus::Done, kUpdatedAt.addSecs(60)},
    });
    QCOMPARE(conflictResult.updatedTaskCount, 0);
    QCOMPARE(conflictResult.conflictingTaskIds, QList<QUuid>{conflictId});
    QCOMPARE(repository.findById(firstId)->status(), TaskStatus::Done);
    QCOMPARE(repository.findById(conflictId)->status(), TaskStatus::Todo);

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
        (void) repository.updateTaskStatesAtomically({
            TaskStateChange{firstId, TaskStatus::Done, TaskStatus::Archived,
                            TaskStatus::Done, kUpdatedAt.addSecs(120)},
            TaskStateChange{triggerId, TaskStatus::Done, TaskStatus::Archived,
                            TaskStatus::Done, kUpdatedAt.addSecs(120)},
        });
    } catch (const RepositoryException &) {
        threw = true;
    }
    QVERIFY(threw);
    QCOMPARE(repository.findById(firstId)->status(), TaskStatus::Done);
    QCOMPARE(repository.findById(triggerId)->status(), TaskStatus::Done);
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
