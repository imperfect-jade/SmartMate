#include "persistence/SqliteTaskRepository.h"

#include "persistence/internal/SqliteTaskRepositorySupport.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace smartmate::model::persistence {

using namespace sqlite_task_repository_detail;

QList<Task> SqliteTaskRepository::findAll() const
{
    auto database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(database);
    const QString statement =
        // 此 ORDER BY 只保证 Repository 结果确定；ViewModel 绑定顺序由 Model Planner 重算。
        QStringLiteral("SELECT %1 FROM tasks ORDER BY updated_at_utc_ms DESC, id ASC")
            .arg(taskSelectColumns());
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
            .arg(taskSelectColumns()));
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
    // 单表写端口不复制业务规则；调用方 Service 负责校验和成功后的 tasksChanged()。
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
    // 返回受影响行数供 Service 区分“写入成功”与“稳定 ID 已消失”，本层不发通知。
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

} // namespace smartmate::model::persistence

