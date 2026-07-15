#include "persistence/SqliteTaskRepository.h"

#include "persistence/TaskSqlCodec.h"
#include "persistence/internal/SqliteTaskRepositorySupport.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <limits>
#include <utility>

namespace smartmate::model::persistence {

using namespace sqlite_task_repository_detail;

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
        // Persistence 不发送通知；Service 确认任务和全部边已提交后再通知任务/依赖投影。
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
                              detail::taskStatusToSqlText(change.targetStatus));
            if (change.statusBeforeArchive.has_value()) {
                updateQuery.bindValue(
                    QStringLiteral(":status_before_archive"),
                                  detail::taskStatusToSqlText(
                                      *change.statusBeforeArchive));
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
                              detail::taskStatusToSqlText(change.expectedStatus));
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
        // 原子提交完成后返回计数；Service 再发送一次 tasksChanged()，避免逐项刷新绑定。
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
        // 先删除依赖边以满足外键 RESTRICT；任一任务条件删除失败时事务会恢复全部边。
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
        // 返回去重后的实际删边数；Service 据此决定是否发送 dependenciesChanged()。
        return {deletedTaskCount, static_cast<int>(removedDependencyCount), {}};
    } catch (...) {
        database.rollback();
        throw;
    }
}

} // namespace smartmate::model::persistence

