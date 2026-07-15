#include "persistence/internal/SqliteTaskRepositorySupport.h"

#include "persistence/TaskSqlCodec.h"
#include "repositories/RepositoryException.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimeZone>
#include <QVariant>

#include <optional>

namespace smartmate::model::persistence::sqlite_task_repository_detail {
namespace {

// SELECT 列顺序是持久化协议，必须与下方查询行索引同步修改。
constexpr char kTaskSelectColumns[] =
    "id, title, description, priority, status, status_before_archive, "
    "deadline_utc_ms, estimated_minutes, created_at_utc_ms, updated_at_utc_ms, "
    "category_id";
constexpr char kCategorySelectColumns[] =
    "id, name, color, created_at_utc_ms, updated_at_utc_ms";

std::optional<TaskStatus> optionalStatusFromStorage(const QVariant &value)
{
    return value.isNull()
        ? std::nullopt
        : std::optional<TaskStatus>{detail::taskStatusFromSqlText(value.toString())};
}

std::optional<QDateTime> optionalDateTimeFromStorage(const QVariant &value)
{
    return value.isNull()
        ? std::nullopt
        : std::optional<QDateTime>{QDateTime::fromMSecsSinceEpoch(
              value.toLongLong(), QTimeZone::UTC)};
}

std::optional<int> optionalIntegerFromStorage(const QVariant &value)
{
    return value.isNull() ? std::nullopt
                          : std::optional<int>{value.toInt()};
}

std::optional<TaskCategoryId> optionalCategoryIdFromStorage(
    const QVariant &value)
{
    if (value.isNull()) return std::nullopt;
    const TaskCategoryId id = TaskCategoryId::fromString(value.toString());
    if (id.isNull()) {
        throwPersistenceError(
            QStringLiteral("SQLite contains an invalid category id"));
    }
    return id;
}

} // namespace

QLatin1StringView taskSelectColumns() noexcept
{
    return QLatin1StringView{kTaskSelectColumns};
}

QLatin1StringView categorySelectColumns() noexcept
{
    return QLatin1StringView{kCategorySelectColumns};
}

[[noreturn]] void throwDatabaseError(const QString &operation,
                                     const QSqlError &error)
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
        throwDatabaseError(QStringLiteral("SQLite statement failed"),
                           query.lastError());
    }
}

Task taskFromQuery(const QSqlQuery &query)
{
    // 非法存量数据立即报告，禁止在 Persistence 边界静默修正领域快照。
    const TaskId id = TaskId::fromString(query.value(0).toString());
    if (id.isNull()) {
        throwPersistenceError(QStringLiteral("SQLite contains an invalid task id"));
    }
    return Task(
        id,
        query.value(1).toString(),
        query.value(2).toString(),
        detail::taskPriorityFromSqlText(query.value(3).toString()),
        detail::taskStatusFromSqlText(query.value(4).toString()),
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
        throwPersistenceError(
            QStringLiteral("SQLite contains an invalid category id"));
    }
    return TaskCategory{
        id,
        query.value(1).toString(),
        detail::taskCategoryColorFromSqlText(query.value(2).toString()),
        QDateTime::fromMSecsSinceEpoch(query.value(3).toLongLong(), QTimeZone::UTC),
        QDateTime::fromMSecsSinceEpoch(query.value(4).toLongLong(), QTimeZone::UTC)};
}

void bindTask(QSqlQuery &query, const Task &task)
{
    query.bindValue(QStringLiteral(":id"),
                    task.id().toString(QUuid::WithoutBraces));
    query.bindValue(QStringLiteral(":title"), task.title());
    // 领域中的“未填写描述”保存为空文本，避免 null QString 触发 NOT NULL。
    query.bindValue(QStringLiteral(":description"),
                    task.description().isNull() ? QStringLiteral("")
                                                : task.description());
    query.bindValue(QStringLiteral(":priority"),
                    detail::taskPriorityToSqlText(task.priority()));
    query.bindValue(QStringLiteral(":status"),
                    detail::taskStatusToSqlText(task.status()));
    query.bindValue(QStringLiteral(":status_before_archive"),
                    task.statusBeforeArchive().has_value()
                        ? QVariant{detail::taskStatusToSqlText(
                              *task.statusBeforeArchive())}
                        : QVariant{});
    query.bindValue(QStringLiteral(":deadline_utc_ms"),
                    task.deadline().has_value()
                        ? QVariant{task.deadline()->toUTC().toMSecsSinceEpoch()}
                        : QVariant{});
    query.bindValue(QStringLiteral(":estimated_minutes"),
                    task.estimatedMinutes().has_value()
                        ? QVariant{*task.estimatedMinutes()}
                        : QVariant{});
    query.bindValue(QStringLiteral(":created_at_utc_ms"),
                    task.createdAtUtc().toUTC().toMSecsSinceEpoch());
    query.bindValue(QStringLiteral(":updated_at_utc_ms"),
                    task.updatedAtUtc().toUTC().toMSecsSinceEpoch());
    query.bindValue(QStringLiteral(":category_id"),
                    task.categoryId().has_value()
                        ? QVariant{task.categoryId()->toString(QUuid::WithoutBraces)}
                        : QVariant{});
}

void bindCategory(QSqlQuery &query, const TaskCategory &category)
{
    query.bindValue(QStringLiteral(":id"),
                    category.id.toString(QUuid::WithoutBraces));
    query.bindValue(QStringLiteral(":name"), category.name);
    query.bindValue(QStringLiteral(":name_key"),
                    taskCategoryNameKey(category.name));
    query.bindValue(QStringLiteral(":color"),
                    detail::taskCategoryColorToSqlText(category.color));
    query.bindValue(QStringLiteral(":created_at_utc_ms"),
                    category.createdAtUtc.toUTC().toMSecsSinceEpoch());
    query.bindValue(QStringLiteral(":updated_at_utc_ms"),
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
        if (query.value(1).toString() == columnName) return true;
    }
    return false;
}

} // namespace smartmate::model::persistence::sqlite_task_repository_detail
