#include "persistence/SqliteTaskRepository.h"
#include "persistence/internal/SqliteTaskRepositorySupport.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace smartmate::model::persistence {
using namespace sqlite_task_repository_detail;

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
    if (schemaVersion > 4) {
        throwPersistenceError(
            QStringLiteral("SQLite schema version %1 is newer than supported version 4")
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
        // Service 先给出友好业务错误，此索引再防止并发或绕过 Service 破坏单进行中约束。
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

        // v4 以不可变事件作为历史统计事实；类别字段是完成时快照，不建立类别外键，
        // 从而保证类别重命名或删除不会改写过去。任务永久删除则通过 CASCADE 清理历史。
        executeStatement(
            database,
            QStringLiteral(
                "CREATE TABLE IF NOT EXISTS task_activity_events ("
                "id TEXT PRIMARY KEY NOT NULL, "
                "task_id TEXT NOT NULL REFERENCES tasks(id) ON DELETE CASCADE, "
                "transition TEXT NOT NULL CHECK (transition IN ("
                "'start', 'cancel', 'complete', 'redo', 'archive', 'restore')), "
                "from_status TEXT NOT NULL CHECK (from_status IN ("
                "'todo', 'in_progress', 'done', 'cancelled', 'archived')), "
                "to_status TEXT NOT NULL CHECK (to_status IN ("
                "'todo', 'in_progress', 'done', 'cancelled', 'archived')), "
                "occurred_at_utc_ms INTEGER NOT NULL, "
                "deadline_snapshot_utc_ms INTEGER NULL, "
                "estimated_minutes_snapshot INTEGER NULL CHECK ("
                "estimated_minutes_snapshot IS NULL OR "
                "estimated_minutes_snapshot BETWEEN 1 AND 525600), "
                "priority_snapshot TEXT NOT NULL CHECK (priority_snapshot IN ("
                "'low', 'normal', 'high', 'urgent')), "
                "category_id_snapshot TEXT NULL, "
                "category_name_snapshot TEXT NULL, "
                "category_color_snapshot TEXT NULL CHECK ("
                "category_color_snapshot IS NULL OR category_color_snapshot IN ("
                "'blue', 'teal', 'green', 'amber', 'orange', 'rose', 'violet', 'slate')), "
                "CHECK ((category_id_snapshot IS NULL "
                "        AND category_name_snapshot IS NULL "
                "        AND category_color_snapshot IS NULL) OR "
                "       (category_id_snapshot IS NOT NULL "
                "        AND length(trim(category_name_snapshot)) >= 1 "
                "        AND category_color_snapshot IS NOT NULL))"
                ")"));
        executeStatement(
            database,
            QStringLiteral(
                "CREATE INDEX IF NOT EXISTS idx_task_activity_events_occurred "
                "ON task_activity_events(occurred_at_utc_ms, id)"));
        executeStatement(
            database,
            QStringLiteral(
                "CREATE INDEX IF NOT EXISTS idx_task_activity_events_transition_occurred "
                "ON task_activity_events(transition, occurred_at_utc_ms, id)"));
        executeStatement(
            database,
            QStringLiteral(
                "CREATE INDEX IF NOT EXISTS idx_task_activity_events_task_occurred "
                "ON task_activity_events(task_id, occurred_at_utc_ms, id)"));

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

        if (schemaVersion < 4) {
            executeStatement(database, QStringLiteral("PRAGMA user_version = 4"));
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

} // namespace smartmate::model::persistence
