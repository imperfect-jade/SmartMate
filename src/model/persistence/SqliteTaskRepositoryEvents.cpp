#include "persistence/SqliteTaskRepository.h"

#include "persistence/TaskSqlCodec.h"
#include "persistence/internal/SqliteTaskRepositorySupport.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimeZone>
#include <QVariant>

namespace smartmate::model::persistence {
namespace {

TaskActivityEvent eventFromQuery(const QSqlQuery &query)
{
    TaskActivityEvent event;
    event.eventId = QUuid{query.value(0).toString()};
    event.taskId = QUuid{query.value(1).toString()};
    event.transition = detail::taskTransitionFromSqlText(query.value(2).toString());
    event.fromStatus = detail::taskStatusFromSqlText(query.value(3).toString());
    event.toStatus = detail::taskStatusFromSqlText(query.value(4).toString());
    event.occurredAtUtc = QDateTime::fromMSecsSinceEpoch(
        query.value(5).toLongLong(), QTimeZone::UTC);
    if (!query.value(6).isNull()) {
        event.deadlineSnapshotUtc = QDateTime::fromMSecsSinceEpoch(
            query.value(6).toLongLong(), QTimeZone::UTC);
    }
    if (!query.value(7).isNull()) {
        event.estimatedMinutesSnapshot = query.value(7).toInt();
    }
    event.prioritySnapshot = detail::taskPriorityFromSqlText(
        query.value(8).toString());
    if (!query.value(9).isNull()) {
        event.categoryIdSnapshot = QUuid{query.value(9).toString()};
        event.categoryNameSnapshot = query.value(10).toString();
        event.categoryColorSnapshot = detail::taskCategoryColorFromSqlText(
            query.value(11).toString());
    }
    return event;
}

QString eventSelectColumns()
{
    return QStringLiteral(
        "SELECT id, task_id, transition, from_status, to_status, "
        "occurred_at_utc_ms, deadline_snapshot_utc_ms, "
        "estimated_minutes_snapshot, priority_snapshot, "
        "category_id_snapshot, category_name_snapshot, category_color_snapshot "
        "FROM task_activity_events ");
}

} // namespace

QList<TaskActivityEvent> SqliteTaskRepository::findEventsByOccurredAt(
    const QDateTime &startInclusiveUtc,
    const QDateTime &endExclusiveUtc) const
{
    auto database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(database);
    query.prepare(eventSelectColumns()
                  + QStringLiteral(
                      "WHERE occurred_at_utc_ms >= :start_ms "
                      "AND occurred_at_utc_ms < :end_ms "
                      "ORDER BY occurred_at_utc_ms, id"));
    query.bindValue(QStringLiteral(":start_ms"),
                    startInclusiveUtc.toUTC().toMSecsSinceEpoch());
    query.bindValue(QStringLiteral(":end_ms"),
                    endExclusiveUtc.toUTC().toMSecsSinceEpoch());
    if (!query.exec()) {
        sqlite_task_repository_detail::throwDatabaseError(
            QStringLiteral("Cannot query task activity events"), query.lastError());
    }

    QList<TaskActivityEvent> events;
    while (query.next()) {
        events.append(eventFromQuery(query));
    }
    return events;
}

std::optional<TaskActivityEvent> SqliteTaskRepository::findLatestCompletionBefore(
    const QDateTime &endExclusiveUtc) const
{
    auto database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(database);
    query.prepare(eventSelectColumns()
                  + QStringLiteral(
                      "WHERE transition = 'complete' "
                      "AND occurred_at_utc_ms < :end_ms "
                      "ORDER BY occurred_at_utc_ms DESC, id DESC LIMIT 1"));
    query.bindValue(QStringLiteral(":end_ms"),
                    endExclusiveUtc.toUTC().toMSecsSinceEpoch());
    if (!query.exec()) {
        sqlite_task_repository_detail::throwDatabaseError(
            QStringLiteral("Cannot query latest completion event"), query.lastError());
    }
    if (!query.next()) {
        return std::nullopt;
    }
    return eventFromQuery(query);
}

} // namespace smartmate::model::persistence
