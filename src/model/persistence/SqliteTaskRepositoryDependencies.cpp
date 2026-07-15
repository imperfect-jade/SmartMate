#include "persistence/SqliteTaskRepository.h"

#include "persistence/internal/SqliteTaskRepositorySupport.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace smartmate::model::persistence {

using namespace sqlite_task_repository_detail;

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
        // “删除全部旧入边 + 插入完整新集合”是一个持久化命令，不能让绑定观察到中间状态。
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
        // commit 成功后才正常返回；dependenciesChanged() 由 Service 在事务外统一发送。
    } catch (...) {
        database.rollback();
        throw;
    }
}

} // namespace smartmate::model::persistence

