#include "persistence/SqliteTaskRepository.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimeZone>
#include <QVariant>

#include <limits>
#include <utility>

namespace smartmate::model::persistence {
namespace {

// SELECT列顺序必须与taskFromQuery()中的索引映射保持一致。
constexpr auto kSelectColumns =
    "id, title, description, priority, status, status_before_archive, "
    "deadline_utc_ms, estimated_minutes, created_at_utc_ms, updated_at_utc_ms, "
    "category_id";

// 类别列顺序同样属于持久化协议，必须与categoryFromQuery()同步修改。
constexpr auto kSelectCategoryColumns =
    "id, name, color, created_at_utc_ms, updated_at_utc_ms";

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

std::optional<TaskCategoryId> optionalCategoryIdFromStorage(const QVariant &value)
{
    if (value.isNull()) {
        return std::nullopt;
    }

    const TaskCategoryId id = TaskCategoryId::fromString(value.toString());
    if (id.isNull()) {
        throwPersistenceError(QStringLiteral("SQLite contains an invalid category id"));
    }
    return id;
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
        QDateTime::fromMSecsSinceEpoch(query.value(9).toLongLong(), QTimeZone::UTC),
        optionalCategoryIdFromStorage(query.value(10)));
}

TaskCategory categoryFromQuery(const QSqlQuery &query)
{
    const TaskCategoryId id = TaskCategoryId::fromString(query.value(0).toString());
    if (id.isNull()) {
        throwPersistenceError(QStringLiteral("SQLite contains an invalid category id"));
    }

    const auto color = taskCategoryColorFromStorageText(query.value(2).toString());
    if (!color.has_value()) {
        throwPersistenceError(
            QStringLiteral("Unknown task category color in SQLite: %1")
                .arg(query.value(2).toString()));
    }

    return TaskCategory{
        id,
        query.value(1).toString(),
        *color,
        QDateTime::fromMSecsSinceEpoch(query.value(3).toLongLong(), QTimeZone::UTC),
        QDateTime::fromMSecsSinceEpoch(query.value(4).toLongLong(), QTimeZone::UTC)};
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

    if (task.categoryId().has_value()) {
        query.bindValue(
            QStringLiteral(":category_id"),
            task.categoryId()->toString(QUuid::WithoutBraces));
    } else {
        query.bindValue(QStringLiteral(":category_id"), QVariant());
    }
}

void bindCategory(QSqlQuery &query, const TaskCategory &category)
{
    query.bindValue(
        QStringLiteral(":id"), category.id.toString(QUuid::WithoutBraces));
    query.bindValue(QStringLiteral(":name"), category.name);
    // name_key与Model校验共用同一Unicode规范化函数，数据库唯一索引只负责并发兜底。
    query.bindValue(QStringLiteral(":name_key"), taskCategoryNameKey(category.name));
    query.bindValue(QStringLiteral(":color"),
                    taskCategoryColorToStorageText(category.color));
    query.bindValue(
        QStringLiteral(":created_at_utc_ms"),
        category.createdAtUtc.toUTC().toMSecsSinceEpoch());
    query.bindValue(
        QStringLiteral(":updated_at_utc_ms"),
        category.updatedAtUtc.toUTC().toMSecsSinceEpoch());
}

bool tableHasColumn(QSqlDatabase &database,
                    const QString &tableName,
                    const QString &columnName)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(tableName))) {
        throwDatabaseError(QStringLiteral("Cannot inspect SQLite table columns"),
                           query.lastError());
    }
    while (query.next()) {
        if (query.value(1).toString() == columnName) {
            return true;
        }
    }
    return false;
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
    if (schemaVersion > 3) {
        throwPersistenceError(
            QStringLiteral("SQLite schema version %1 is newer than supported version 3")
                .arg(schemaVersion));
    }

    // DDL与版本号同事务提交，配合IF NOT EXISTS保证重复启动安全。
    if (!database.transaction()) {
        throwDatabaseError(
            QStringLiteral("Cannot start SQLite schema transaction"), database.lastError());
    }

    try {
        // 类别实体先于tasks创建，使新数据库可以立即建立category_id外键；旧数据库
        // 则在同一事务内补列，迁移失败不会留下半个v3 Schema。
        executeStatement(
            database,
            QStringLiteral(
                "CREATE TABLE IF NOT EXISTS task_categories ("
                "id TEXT PRIMARY KEY NOT NULL, "
                "name TEXT NOT NULL, "
                "name_key TEXT NOT NULL UNIQUE, "
                "color TEXT NOT NULL CHECK (color IN ("
                "'blue', 'teal', 'green', 'amber', 'orange', 'rose', 'violet', 'slate')), "
                "created_at_utc_ms INTEGER NOT NULL, "
                "updated_at_utc_ms INTEGER NOT NULL, "
                "CHECK (length(trim(name)) BETWEEN 1 AND 50), "
                "CHECK (length(name_key) >= 1)"
                ")"));
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
                "category_id TEXT NULL REFERENCES task_categories(id) ON DELETE SET NULL, "
                "CHECK (length(trim(title)) BETWEEN 1 AND 200), "
                "CHECK (length(description) <= 5000), "
                "CHECK ((status = 'archived' AND status_before_archive IS NOT NULL) OR "
                "       (status <> 'archived' AND status_before_archive IS NULL))"
                ")"));

        if (!tableHasColumn(database,
                            QStringLiteral("tasks"),
                            QStringLiteral("category_id"))) {
            executeStatement(
                database,
                QStringLiteral(
                    "ALTER TABLE tasks ADD COLUMN category_id TEXT NULL "
                    "REFERENCES task_categories(id) ON DELETE SET NULL"));
        }
        executeStatement(
            database,
            QStringLiteral(
                "CREATE INDEX IF NOT EXISTS idx_tasks_status ON tasks(status)"));
        executeStatement(
            database,
            QStringLiteral(
                "CREATE INDEX IF NOT EXISTS idx_tasks_deadline ON tasks(deadline_utc_ms)"));
        executeStatement(
            database,
            QStringLiteral(
                "CREATE INDEX IF NOT EXISTS idx_tasks_category_id ON tasks(category_id)"));
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

        if (schemaVersion < 3) {
            executeStatement(database, QStringLiteral("PRAGMA user_version = 3"));
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
        "deadline_utc_ms, estimated_minutes, created_at_utc_ms, updated_at_utc_ms, "
        "category_id"
        ") VALUES ("
        ":id, :title, :description, :priority, :status, :status_before_archive, "
        ":deadline_utc_ms, :estimated_minutes, :created_at_utc_ms, :updated_at_utc_ms, "
        ":category_id"
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
        "updated_at_utc_ms = :updated_at_utc_ms, "
        "category_id = :category_id "
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
            "deadline_utc_ms, estimated_minutes, created_at_utc_ms, updated_at_utc_ms, "
            "category_id"
            ") VALUES ("
            ":id, :title, :description, :priority, :status, :status_before_archive, "
            ":deadline_utc_ms, :estimated_minutes, :created_at_utc_ms, :updated_at_utc_ms, "
            ":category_id"
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

TaskBatchWriteResult SqliteTaskRepository::updateTaskStatesAtomically(
    const QList<TaskStateChange> &changes)
{
    auto database = QSqlDatabase::database(m_connectionName, false);
    if (!database.transaction()) {
        throwDatabaseError(
            QStringLiteral("Cannot start batch task transition transaction"),
            database.lastError());
    }

    try {
        // expected_status 是 Service 预检与真正写入之间的并发防线；一个任务冲突时，
        // 先收集全部冲突 ID，再回滚此前已执行的更新，绝不产生部分成功。
        QSqlQuery updateQuery(database);
        updateQuery.prepare(QStringLiteral(
            "UPDATE tasks SET "
            "status = :target_status, "
            "status_before_archive = :status_before_archive, "
            "updated_at_utc_ms = :updated_at_utc_ms "
            "WHERE id = :task_id AND status = :expected_status"));

        QList<TaskId> conflictingTaskIds;
        for (const TaskStateChange &change : changes) {
            updateQuery.bindValue(QStringLiteral(":target_status"),
                                  statusToStorage(change.targetStatus));
            if (change.statusBeforeArchive.has_value()) {
                updateQuery.bindValue(
                    QStringLiteral(":status_before_archive"),
                    statusToStorage(*change.statusBeforeArchive));
            } else {
                updateQuery.bindValue(QStringLiteral(":status_before_archive"),
                                      QVariant());
            }
            updateQuery.bindValue(
                QStringLiteral(":updated_at_utc_ms"),
                change.updatedAtUtc.toUTC().toMSecsSinceEpoch());
            updateQuery.bindValue(
                QStringLiteral(":task_id"),
                change.taskId.toString(QUuid::WithoutBraces));
            updateQuery.bindValue(QStringLiteral(":expected_status"),
                                  statusToStorage(change.expectedStatus));
            if (!updateQuery.exec()) {
                throwDatabaseError(QStringLiteral("Cannot update batch task state"),
                                   updateQuery.lastError());
            }
            if (updateQuery.numRowsAffected() != 1) {
                conflictingTaskIds.append(change.taskId);
            }
            updateQuery.finish();
        }

        if (!conflictingTaskIds.isEmpty()) {
            if (!database.rollback()) {
                throwDatabaseError(
                    QStringLiteral("Cannot roll back conflicting batch task transition"),
                    database.lastError());
            }
            return {0, std::move(conflictingTaskIds)};
        }

        if (!database.commit()) {
            throwDatabaseError(
                QStringLiteral("Cannot commit batch task transition transaction"),
                database.lastError());
        }
        return {static_cast<int>(changes.size()), {}};
    } catch (...) {
        database.rollback();
        throw;
    }
}

TaskDeletionWriteResult
SqliteTaskRepository::deleteArchivedTasksWithDependencies(
    const QList<TaskId> &taskIds)
{
    auto database = QSqlDatabase::database(m_connectionName, false);
    if (!database.transaction()) {
        throwDatabaseError(
            QStringLiteral("Cannot start permanent task deletion transaction"),
            database.lastError());
    }

    try {
        // 循环复用预编译语句既避免 SQLite 参数上限，也使选中任务之间的共享边只在
        // 第一次命中时计数。若后续状态条件冲突，整个事务会恢复全部已删除边。
        QSqlQuery dependencyQuery(database);
        dependencyQuery.prepare(QStringLiteral(
            "DELETE FROM task_dependencies "
            "WHERE predecessor_id = :task_id OR successor_id = :task_id"));
        qint64 removedDependencyCount = 0;
        for (const TaskId &taskId : taskIds) {
            dependencyQuery.bindValue(
                QStringLiteral(":task_id"),
                taskId.toString(QUuid::WithoutBraces));
            if (!dependencyQuery.exec()) {
                throwDatabaseError(
                    QStringLiteral("Cannot delete batch task dependencies"),
                    dependencyQuery.lastError());
            }
            const qint64 removedForTask = dependencyQuery.numRowsAffected();
            if (removedForTask < 0
                || removedDependencyCount
                    > std::numeric_limits<int>::max() - removedForTask) {
                throwPersistenceError(QStringLiteral(
                    "SQLite returned an invalid removed dependency count"));
            }
            removedDependencyCount += removedForTask;
            dependencyQuery.finish();
        }

        QSqlQuery taskQuery(database);
        taskQuery.prepare(QStringLiteral(
            "DELETE FROM tasks WHERE id = :task_id AND status = 'archived'"));
        QList<TaskId> conflictingTaskIds;
        int deletedTaskCount = 0;
        for (const TaskId &taskId : taskIds) {
            taskQuery.bindValue(
                QStringLiteral(":task_id"),
                taskId.toString(QUuid::WithoutBraces));
            if (!taskQuery.exec()) {
                throwDatabaseError(
                    QStringLiteral("Cannot permanently delete batch task"),
                    taskQuery.lastError());
            }
            if (taskQuery.numRowsAffected() == 1) {
                ++deletedTaskCount;
            } else {
                conflictingTaskIds.append(taskId);
            }
            taskQuery.finish();
        }

        if (!conflictingTaskIds.isEmpty()) {
            if (!database.rollback()) {
                throwDatabaseError(
                    QStringLiteral("Cannot roll back conflicting permanent deletion"),
                    database.lastError());
            }
            return {0, 0, std::move(conflictingTaskIds)};
        }

        if (!database.commit()) {
            throwDatabaseError(
                QStringLiteral("Cannot commit permanent task deletion transaction"),
                database.lastError());
        }
        return {deletedTaskCount, static_cast<int>(removedDependencyCount), {}};
    } catch (...) {
        database.rollback();
        throw;
    }
}

QList<TaskCategory> SqliteTaskRepository::findAllCategories() const
{
    auto database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(database);
    const QString statement =
        QStringLiteral(
            "SELECT %1 FROM task_categories ORDER BY name_key ASC, id ASC")
            .arg(QLatin1StringView(kSelectCategoryColumns));
    if (!query.exec(statement)) {
        throwDatabaseError(QStringLiteral("Cannot list task categories"),
                           query.lastError());
    }

    QList<TaskCategory> categories;
    while (query.next()) {
        categories.append(categoryFromQuery(query));
    }
    return categories;
}

std::optional<TaskCategory> SqliteTaskRepository::findCategoryById(
    const TaskCategoryId &id) const
{
    auto database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(database);
    query.prepare(
        QStringLiteral("SELECT %1 FROM task_categories WHERE id = :id")
            .arg(QLatin1StringView(kSelectCategoryColumns)));
    query.bindValue(QStringLiteral(":id"), id.toString(QUuid::WithoutBraces));
    if (!query.exec()) {
        throwDatabaseError(QStringLiteral("Cannot find task category"),
                           query.lastError());
    }
    if (!query.next()) {
        return std::nullopt;
    }
    return categoryFromQuery(query);
}

void SqliteTaskRepository::insertCategory(const TaskCategory &category)
{
    auto database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO task_categories ("
        "id, name, name_key, color, created_at_utc_ms, updated_at_utc_ms"
        ") VALUES ("
        ":id, :name, :name_key, :color, :created_at_utc_ms, :updated_at_utc_ms"
        ")"));
    bindCategory(query, category);
    if (!query.exec()) {
        throwDatabaseError(QStringLiteral("Cannot insert task category"),
                           query.lastError());
    }
}

bool SqliteTaskRepository::updateCategory(const TaskCategory &category)
{
    auto database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE task_categories SET "
        "name = :name, "
        "name_key = :name_key, "
        "color = :color, "
        "created_at_utc_ms = :created_at_utc_ms, "
        "updated_at_utc_ms = :updated_at_utc_ms "
        "WHERE id = :id"));
    bindCategory(query, category);
    if (!query.exec()) {
        throwDatabaseError(QStringLiteral("Cannot update task category"),
                           query.lastError());
    }
    return query.numRowsAffected() > 0;
}

CategoryDeletionWriteResult
SqliteTaskRepository::deleteCategoryAndUnassignTasks(
    const TaskCategoryId &id,
    const QDateTime &updatedAtUtc)
{
    auto database = QSqlDatabase::database(m_connectionName, false);
    if (!database.transaction()) {
        throwDatabaseError(
            QStringLiteral("Cannot start task category deletion transaction"),
            database.lastError());
    }

    try {
        // 类别删除是唯一允许跨任务状态解除归属的管理操作。先解除外键再删除类别，
        // 并统一更新时间；依赖表不在事务语句中，因此关系必然保持原样。
        QSqlQuery unassignQuery(database);
        unassignQuery.prepare(QStringLiteral(
            "UPDATE tasks SET "
            "category_id = NULL, updated_at_utc_ms = :updated_at_utc_ms "
            "WHERE category_id = :category_id"));
        unassignQuery.bindValue(
            QStringLiteral(":updated_at_utc_ms"),
            updatedAtUtc.toUTC().toMSecsSinceEpoch());
        unassignQuery.bindValue(
            QStringLiteral(":category_id"), id.toString(QUuid::WithoutBraces));
        if (!unassignQuery.exec()) {
            throwDatabaseError(QStringLiteral("Cannot unassign task category"),
                               unassignQuery.lastError());
        }
        const qint64 unassignedTaskCount = unassignQuery.numRowsAffected();
        if (unassignedTaskCount < 0
            || unassignedTaskCount > std::numeric_limits<int>::max()) {
            throwPersistenceError(
                QStringLiteral("SQLite returned an invalid unassigned task count"));
        }
        unassignQuery.finish();

        QSqlQuery deleteQuery(database);
        deleteQuery.prepare(
            QStringLiteral("DELETE FROM task_categories WHERE id = :id"));
        deleteQuery.bindValue(QStringLiteral(":id"),
                              id.toString(QUuid::WithoutBraces));
        if (!deleteQuery.exec()) {
            throwDatabaseError(QStringLiteral("Cannot delete task category"),
                               deleteQuery.lastError());
        }
        const bool categoryDeleted = deleteQuery.numRowsAffected() == 1;
        deleteQuery.finish();

        // 类别缺失时正常情况下不会命中任何任务；仍回滚而非提交，明确保证零写入。
        if (!categoryDeleted) {
            if (!database.rollback()) {
                throwDatabaseError(
                    QStringLiteral("Cannot roll back missing task category deletion"),
                    database.lastError());
            }
            return {};
        }

        if (!database.commit()) {
            throwDatabaseError(
                QStringLiteral("Cannot commit task category deletion transaction"),
                database.lastError());
        }
        return {true, static_cast<int>(unassignedTaskCount)};
    } catch (...) {
        database.rollback();
        throw;
    }
}

} // namespace smartmate::model::persistence
