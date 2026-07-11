#include "persistence/SqliteTaskRepository.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimeZone>
#include <QVariant>

#include <utility>

namespace smartmate::model::persistence {
namespace {

// SELECT列顺序必须与taskFromQuery()中的索引映射保持一致。
constexpr auto kSelectColumns =
    "id, title, description, priority, status, status_before_archive, "
    "deadline_utc_ms, estimated_minutes, created_at_utc_ms, updated_at_utc_ms";

[[noreturn]] void throwDatabaseError(const QString &operation, const QSqlError &error)
{
    throw RepositoryException(
        QStringLiteral("%1: %2").arg(operation, error.text()));
}

[[noreturn]] void throwPersistenceError(const QString &message)
{
    throw RepositoryException(message);
}

void executeStatement(QSqlDatabase &database, const QString &statement)
{
    QSqlQuery query(database);
    if (!query.exec(statement)) {
        throwDatabaseError(QStringLiteral("SQLite statement failed"), query.lastError());
    }
}

// 持久化使用稳定英文值，避免枚举顺序或中文界面文案变化破坏已有数据库。
QString priorityToStorage(TaskPriority priority)
{
    switch (priority) {
    case TaskPriority::Low:
        return QStringLiteral("low");
    case TaskPriority::Normal:
        return QStringLiteral("normal");
    case TaskPriority::High:
        return QStringLiteral("high");
    case TaskPriority::Urgent:
        return QStringLiteral("urgent");
    }

    throwPersistenceError(QStringLiteral("Cannot store an unknown task priority"));
}

TaskPriority priorityFromStorage(const QString &value)
{
    if (value == QStringLiteral("low")) {
        return TaskPriority::Low;
    }
    if (value == QStringLiteral("normal")) {
        return TaskPriority::Normal;
    }
    if (value == QStringLiteral("high")) {
        return TaskPriority::High;
    }
    if (value == QStringLiteral("urgent")) {
        return TaskPriority::Urgent;
    }

    throwPersistenceError(
        QStringLiteral("Unknown task priority in SQLite: %1").arg(value));
}

QString statusToStorage(TaskStatus status)
{
    switch (status) {
    case TaskStatus::Todo:
        return QStringLiteral("todo");
    case TaskStatus::InProgress:
        return QStringLiteral("in_progress");
    case TaskStatus::Done:
        return QStringLiteral("done");
    case TaskStatus::Cancelled:
        return QStringLiteral("cancelled");
    case TaskStatus::Archived:
        return QStringLiteral("archived");
    }

    throwPersistenceError(QStringLiteral("Cannot store an unknown task status"));
}

TaskStatus statusFromStorage(const QString &value)
{
    if (value == QStringLiteral("todo")) {
        return TaskStatus::Todo;
    }
    if (value == QStringLiteral("in_progress")) {
        return TaskStatus::InProgress;
    }
    if (value == QStringLiteral("done")) {
        return TaskStatus::Done;
    }
    if (value == QStringLiteral("cancelled")) {
        return TaskStatus::Cancelled;
    }
    if (value == QStringLiteral("archived")) {
        return TaskStatus::Archived;
    }

    throwPersistenceError(
        QStringLiteral("Unknown task status in SQLite: %1").arg(value));
}

std::optional<TaskStatus> optionalStatusFromStorage(const QVariant &value)
{
    if (value.isNull()) {
        return std::nullopt;
    }
    return statusFromStorage(value.toString());
}

std::optional<QDateTime> optionalDateTimeFromStorage(const QVariant &value)
{
    if (value.isNull()) {
        return std::nullopt;
    }
    return QDateTime::fromMSecsSinceEpoch(value.toLongLong(), QTimeZone::UTC);
}

std::optional<int> optionalIntegerFromStorage(const QVariant &value)
{
    if (value.isNull()) {
        return std::nullopt;
    }
    return value.toInt();
}

Task taskFromQuery(const QSqlQuery &query)
{
    const auto id = TaskId::fromString(query.value(0).toString());
    if (id.isNull()) {
        throwPersistenceError(QStringLiteral("SQLite contains an invalid task id"));
    }

    return Task(
        id,
        query.value(1).toString(),
        query.value(2).toString(),
        priorityFromStorage(query.value(3).toString()),
        statusFromStorage(query.value(4).toString()),
        optionalStatusFromStorage(query.value(5)),
        optionalDateTimeFromStorage(query.value(6)),
        optionalIntegerFromStorage(query.value(7)),
        QDateTime::fromMSecsSinceEpoch(query.value(8).toLongLong(), QTimeZone::UTC),
        QDateTime::fromMSecsSinceEpoch(query.value(9).toLongLong(), QTimeZone::UTC));
}

// 外部文本只通过参数绑定写入；可选值映射为SQL NULL，时间统一保存为UTC毫秒。
void bindTask(QSqlQuery &query, const Task &task)
{
    query.bindValue(
        QStringLiteral(":id"), task.id().toString(QUuid::WithoutBraces));
    query.bindValue(QStringLiteral(":title"), task.title());
    // Qt 会把 null QString 绑定成 SQL NULL；领域中的“未填写描述”应写成
    // 空文本，否则会错误触发 description NOT NULL 约束。
    query.bindValue(QStringLiteral(":description"),
                    task.description().isNull() ? QStringLiteral("")
                                                : task.description());
    query.bindValue(QStringLiteral(":priority"), priorityToStorage(task.priority()));
    query.bindValue(QStringLiteral(":status"), statusToStorage(task.status()));

    if (task.statusBeforeArchive().has_value()) {
        query.bindValue(
            QStringLiteral(":status_before_archive"),
            statusToStorage(*task.statusBeforeArchive()));
    } else {
        query.bindValue(QStringLiteral(":status_before_archive"), QVariant());
    }

    if (task.deadline().has_value()) {
        query.bindValue(
            QStringLiteral(":deadline_utc_ms"),
            task.deadline()->toUTC().toMSecsSinceEpoch());
    } else {
        query.bindValue(QStringLiteral(":deadline_utc_ms"), QVariant());
    }

    if (task.estimatedMinutes().has_value()) {
        query.bindValue(
            QStringLiteral(":estimated_minutes"), *task.estimatedMinutes());
    } else {
        query.bindValue(QStringLiteral(":estimated_minutes"), QVariant());
    }

    query.bindValue(
        QStringLiteral(":created_at_utc_ms"),
        task.createdAtUtc().toUTC().toMSecsSinceEpoch());
    query.bindValue(
        QStringLiteral(":updated_at_utc_ms"),
        task.updatedAtUtc().toUTC().toMSecsSinceEpoch());
}

} // namespace

// Qt SQL按进程级连接名管理资源，UUID可避免多个Repository及并行测试互相覆盖。
SqliteTaskRepository::SqliteTaskRepository(QString databasePath)
    : m_connectionName(
          QStringLiteral("smartmate_tasks_%1")
              .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    databasePath = databasePath.trimmed();
    if (databasePath.isEmpty()) {
        throwPersistenceError(QStringLiteral("SQLite database path must not be empty"));
    }

    const bool inMemory = databasePath == QStringLiteral(":memory:");
    if (!inMemory) {
        const QFileInfo databaseFile(databasePath);
        QDir parentDirectory = databaseFile.absoluteDir();
        if (!parentDirectory.exists() && !parentDirectory.mkpath(QStringLiteral("."))) {
            throwPersistenceError(
                QStringLiteral("Cannot create SQLite database directory: %1")
                    .arg(parentDirectory.absolutePath()));
        }
        databasePath = databaseFile.absoluteFilePath();
    }

    auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    database.setDatabaseName(databasePath);
    if (!database.open()) {
        const QString error = database.lastError().text();
        // removeDatabase前必须先释放本地句柄，否则Qt仍会认为连接正在使用。
        database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
        throwPersistenceError(
            QStringLiteral("Cannot open SQLite task database: %1").arg(error));
    }

    try {
        configureConnection(inMemory);
        initializeSchema();
    } catch (...) {
        database.close();
        // 初始化失败也遵守同一释放顺序，避免遗留失效的命名连接。
        database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
        throw;
    }
}

SqliteTaskRepository::~SqliteTaskRepository()
{
    // 内层作用域确保QSqlDatabase句柄先析构，再从Qt连接注册表中移除名称。
    {
        auto database = QSqlDatabase::database(m_connectionName, false);
        if (database.isValid()) {
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

void SqliteTaskRepository::configureConnection(bool inMemory)
{
    auto database = QSqlDatabase::database(m_connectionName, false);
    if (!database.isValid() || !database.isOpen()) {
        throwPersistenceError(QStringLiteral("SQLite task connection is not open"));
    }

    // 外键为后续关系表兜底，busy timeout缓解短暂锁冲突；WAL仅适用于文件数据库。
    executeStatement(database, QStringLiteral("PRAGMA foreign_keys = ON"));
    executeStatement(database, QStringLiteral("PRAGMA busy_timeout = 5000"));
    if (!inMemory) {
        executeStatement(database, QStringLiteral("PRAGMA journal_mode = WAL"));
    }
}

void SqliteTaskRepository::initializeSchema()
{
    auto database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery versionQuery(database);
    if (!versionQuery.exec(QStringLiteral("PRAGMA user_version")) || !versionQuery.next()) {
        throwDatabaseError(
            QStringLiteral("Cannot read SQLite schema version"), versionQuery.lastError());
    }

    // user_version是迁移入口；旧程序拒绝更高版本，避免误写新Schema。
    const int schemaVersion = versionQuery.value(0).toInt();
    versionQuery.finish();
    if (schemaVersion > 2) {
        throwPersistenceError(
            QStringLiteral("SQLite schema version %1 is newer than supported version 2")
                .arg(schemaVersion));
    }

    // DDL与版本号同事务提交，配合IF NOT EXISTS保证重复启动安全。
    if (!database.transaction()) {
        throwDatabaseError(
            QStringLiteral("Cannot start SQLite schema transaction"), database.lastError());
    }

    try {
        executeStatement(
            database,
            QStringLiteral(
                "CREATE TABLE IF NOT EXISTS tasks ("
                "id TEXT PRIMARY KEY NOT NULL, "
                "title TEXT NOT NULL, "
                "description TEXT NOT NULL, "
                "priority TEXT NOT NULL CHECK (priority IN ('low', 'normal', 'high', 'urgent')), "
                "status TEXT NOT NULL CHECK (status IN ('todo', 'in_progress', 'done', 'cancelled', 'archived')), "
                "status_before_archive TEXT NULL CHECK (status_before_archive IS NULL OR status_before_archive IN ('todo', 'in_progress', 'done', 'cancelled')), "
                "deadline_utc_ms INTEGER NULL, "
                "estimated_minutes INTEGER NULL CHECK (estimated_minutes IS NULL OR estimated_minutes BETWEEN 1 AND 525600), "
                "created_at_utc_ms INTEGER NOT NULL, "
                "updated_at_utc_ms INTEGER NOT NULL, "
                "CHECK (length(trim(title)) BETWEEN 1 AND 200), "
                "CHECK (length(description) <= 5000), "
                "CHECK ((status = 'archived' AND status_before_archive IS NOT NULL) OR "
                "       (status <> 'archived' AND status_before_archive IS NULL))"
                ")"));
        executeStatement(
            database,
            QStringLiteral(
                "CREATE INDEX IF NOT EXISTS idx_tasks_status ON tasks(status)"));
        executeStatement(
            database,
            QStringLiteral(
                "CREATE INDEX IF NOT EXISTS idx_tasks_deadline ON tasks(deadline_utc_ms)"));
        // Service先给出友好业务错误，此索引再防止并发或绕过Service破坏单进行中约束。
        executeStatement(
            database,
            QStringLiteral(
                "CREATE UNIQUE INDEX IF NOT EXISTS idx_tasks_single_in_progress "
                "ON tasks(status) WHERE status = 'in_progress'"));

        // v2新增Finish-to-Start关系。复合主键防止重复边，CHECK和外键负责
        // 自依赖与悬空端点；业务层仍提供更友好的结构化错误。
        executeStatement(
            database,
            QStringLiteral(
                "CREATE TABLE IF NOT EXISTS task_dependencies ("
                "predecessor_id TEXT NOT NULL, "
                "successor_id TEXT NOT NULL, "
                "PRIMARY KEY (predecessor_id, successor_id), "
                "CHECK (predecessor_id <> successor_id), "
                "FOREIGN KEY (predecessor_id) REFERENCES tasks(id) ON DELETE RESTRICT, "
                "FOREIGN KEY (successor_id) REFERENCES tasks(id) ON DELETE RESTRICT"
                ")"));
        executeStatement(
            database,
            QStringLiteral(
                "CREATE INDEX IF NOT EXISTS idx_task_dependencies_successor "
                "ON task_dependencies(successor_id, predecessor_id)"));

        // SQLite不能用普通约束表达有向无环图；递归CTE从新边的后继出发，
        // 若能到达其前驱，插入该边就会闭合一个环并被中止。
        executeStatement(
            database,
            QStringLiteral(
                "CREATE TRIGGER IF NOT EXISTS trg_task_dependencies_prevent_cycle "
                "BEFORE INSERT ON task_dependencies "
                "FOR EACH ROW "
                "WHEN EXISTS ("
                "  WITH RECURSIVE reachable(task_id) AS ("
                "    SELECT successor_id FROM task_dependencies "
                "      WHERE predecessor_id = NEW.successor_id "
                "    UNION "
                "    SELECT dependency.successor_id "
                "      FROM task_dependencies AS dependency "
                "      JOIN reachable "
                "        ON dependency.predecessor_id = reachable.task_id"
                "  ) "
                "  SELECT 1 FROM reachable WHERE task_id = NEW.predecessor_id"
                ") "
                "BEGIN "
                "  SELECT RAISE(ABORT, 'task dependency would create a cycle'); "
                "END"));

        if (schemaVersion < 2) {
            executeStatement(database, QStringLiteral("PRAGMA user_version = 2"));
        }

        if (!database.commit()) {
            throwDatabaseError(
                QStringLiteral("Cannot commit SQLite schema transaction"),
                database.lastError());
        }
    } catch (...) {
        database.rollback();
        throw;
    }
}

QList<Task> SqliteTaskRepository::findAll() const
{
    auto database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(database);
    const QString statement =
        QStringLiteral("SELECT %1 FROM tasks ORDER BY updated_at_utc_ms DESC, id ASC")
            .arg(QLatin1StringView(kSelectColumns));
    if (!query.exec(statement)) {
        throwDatabaseError(QStringLiteral("Cannot list tasks"), query.lastError());
    }

    QList<Task> tasks;
    while (query.next()) {
        tasks.append(taskFromQuery(query));
    }
    return tasks;
}

std::optional<Task> SqliteTaskRepository::findById(const TaskId &id) const
{
    auto database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(database);
    query.prepare(
        QStringLiteral("SELECT %1 FROM tasks WHERE id = :id")
            .arg(QLatin1StringView(kSelectColumns)));
    query.bindValue(QStringLiteral(":id"), id.toString(QUuid::WithoutBraces));
    if (!query.exec()) {
        throwDatabaseError(QStringLiteral("Cannot find task"), query.lastError());
    }

    if (!query.next()) {
        return std::nullopt;
    }
    return taskFromQuery(query);
}

void SqliteTaskRepository::insert(const Task &task)
{
    auto database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO tasks ("
        "id, title, description, priority, status, status_before_archive, "
        "deadline_utc_ms, estimated_minutes, created_at_utc_ms, updated_at_utc_ms"
        ") VALUES ("
        ":id, :title, :description, :priority, :status, :status_before_archive, "
        ":deadline_utc_ms, :estimated_minutes, :created_at_utc_ms, :updated_at_utc_ms"
        ")"));
    bindTask(query, task);
    if (!query.exec()) {
        throwDatabaseError(QStringLiteral("Cannot insert task"), query.lastError());
    }
}

bool SqliteTaskRepository::update(const Task &task)
{
    auto database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE tasks SET "
        "title = :title, "
        "description = :description, "
        "priority = :priority, "
        "status = :status, "
        "status_before_archive = :status_before_archive, "
        "deadline_utc_ms = :deadline_utc_ms, "
        "estimated_minutes = :estimated_minutes, "
        "created_at_utc_ms = :created_at_utc_ms, "
        "updated_at_utc_ms = :updated_at_utc_ms "
        "WHERE id = :id"));
    bindTask(query, task);
    if (!query.exec()) {
        throwDatabaseError(QStringLiteral("Cannot update task"), query.lastError());
    }
    return query.numRowsAffected() > 0;
}

QList<TaskDependency> SqliteTaskRepository::findAllDependencies() const
{
    auto database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(
            "SELECT predecessor_id, successor_id "
            "FROM task_dependencies "
            "ORDER BY successor_id ASC, predecessor_id ASC"))) {
        throwDatabaseError(QStringLiteral("Cannot list task dependencies"),
                           query.lastError());
    }

    QList<TaskDependency> dependencies;
    while (query.next()) {
        const TaskId predecessorId = TaskId::fromString(query.value(0).toString());
        const TaskId successorId = TaskId::fromString(query.value(1).toString());
        if (predecessorId.isNull() || successorId.isNull()) {
            throwPersistenceError(
                QStringLiteral("SQLite contains an invalid task dependency id"));
        }
        dependencies.append(TaskDependency{predecessorId, successorId});
    }
    return dependencies;
}

void SqliteTaskRepository::replacePredecessors(
    const TaskId &successorId,
    const QList<TaskId> &predecessorIds)
{
    auto database = QSqlDatabase::database(m_connectionName, false);
    if (!database.transaction()) {
        throwDatabaseError(
            QStringLiteral("Cannot start task dependency transaction"),
            database.lastError());
    }

    try {
        QSqlQuery deleteQuery(database);
        deleteQuery.prepare(QStringLiteral(
            "DELETE FROM task_dependencies WHERE successor_id = :successor_id"));
        deleteQuery.bindValue(
            QStringLiteral(":successor_id"),
            successorId.toString(QUuid::WithoutBraces));
        if (!deleteQuery.exec()) {
            throwDatabaseError(QStringLiteral("Cannot replace task predecessors"),
                               deleteQuery.lastError());
        }
        deleteQuery.finish();

        QSqlQuery insertQuery(database);
        insertQuery.prepare(QStringLiteral(
            "INSERT INTO task_dependencies (predecessor_id, successor_id) "
            "VALUES (:predecessor_id, :successor_id)"));
        for (const TaskId &predecessorId : predecessorIds) {
            insertQuery.bindValue(
                QStringLiteral(":predecessor_id"),
                predecessorId.toString(QUuid::WithoutBraces));
            insertQuery.bindValue(
                QStringLiteral(":successor_id"),
                successorId.toString(QUuid::WithoutBraces));
            if (!insertQuery.exec()) {
                throwDatabaseError(QStringLiteral("Cannot insert task dependency"),
                                   insertQuery.lastError());
            }
            insertQuery.finish();
        }

        if (!database.commit()) {
            throwDatabaseError(
                QStringLiteral("Cannot commit task dependency transaction"),
                database.lastError());
        }
    } catch (...) {
        database.rollback();
        throw;
    }
}

void SqliteTaskRepository::insertTaskWithPredecessors(
    const Task &task,
    const QList<TaskId> &predecessorIds)
{
    auto database = QSqlDatabase::database(m_connectionName, false);
    if (!database.transaction()) {
        throwDatabaseError(
            QStringLiteral("Cannot start atomic task creation transaction"),
            database.lastError());
    }

    try {
        // 任务和前置边属于一个创建命令。任务先写入以满足后继外键；任意一条边失败时，
        // catch中的rollback会同时撤销任务和已经成功写入的边，避免产生半成品任务。
        QSqlQuery taskQuery(database);
        taskQuery.prepare(QStringLiteral(
            "INSERT INTO tasks ("
            "id, title, description, priority, status, status_before_archive, "
            "deadline_utc_ms, estimated_minutes, created_at_utc_ms, updated_at_utc_ms"
            ") VALUES ("
            ":id, :title, :description, :priority, :status, :status_before_archive, "
            ":deadline_utc_ms, :estimated_minutes, :created_at_utc_ms, :updated_at_utc_ms"
            ")"));
        bindTask(taskQuery, task);
        if (!taskQuery.exec()) {
            throwDatabaseError(QStringLiteral("Cannot atomically insert task"),
                               taskQuery.lastError());
        }
        taskQuery.finish();

        QSqlQuery dependencyQuery(database);
        dependencyQuery.prepare(QStringLiteral(
            "INSERT INTO task_dependencies (predecessor_id, successor_id) "
            "VALUES (:predecessor_id, :successor_id)"));
        const QString successorId = task.id().toString(QUuid::WithoutBraces);
        for (const TaskId &predecessorId : predecessorIds) {
            dependencyQuery.bindValue(
                QStringLiteral(":predecessor_id"),
                predecessorId.toString(QUuid::WithoutBraces));
            dependencyQuery.bindValue(QStringLiteral(":successor_id"), successorId);
            if (!dependencyQuery.exec()) {
                throwDatabaseError(
                    QStringLiteral("Cannot atomically insert task dependency"),
                    dependencyQuery.lastError());
            }
            dependencyQuery.finish();
        }

        if (!database.commit()) {
            throwDatabaseError(
                QStringLiteral("Cannot commit atomic task creation transaction"),
                database.lastError());
        }
    } catch (...) {
        // commit失败也尝试回滚；SQLite会在连接继续使用前恢复到一致状态。
        database.rollback();
        throw;
    }
}

} // namespace smartmate::model::persistence
