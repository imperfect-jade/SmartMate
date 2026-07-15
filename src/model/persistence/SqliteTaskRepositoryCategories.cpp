#include "persistence/SqliteTaskRepository.h"

#include "persistence/internal/SqliteTaskRepositorySupport.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include <limits>

namespace smartmate::model::persistence {

using namespace sqlite_task_repository_detail;

QList<TaskCategory> SqliteTaskRepository::findAllCategories() const
{
    auto database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(database);
    const QString statement =
        // name_key 与稳定 ID 仅保证查询可复现；展示 Role 仍由类别 ViewModel 生成。
        QStringLiteral(
            "SELECT %1 FROM task_categories ORDER BY name_key ASC, id ASC")
            .arg(categorySelectColumns());
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
            .arg(categorySelectColumns()));
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
    // 唯一索引负责并发兜底；友好的 DuplicateName 错误仍由 Service 预检和映射。
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
        // 返回影响范围后由 TaskCategoryService 分别发布类别目录和任务归属失效通知。
        return {true, static_cast<int>(unassignedTaskCount)};
    } catch (...) {
        database.rollback();
        throw;
    }
}

} // namespace smartmate::model::persistence

