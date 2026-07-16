#include "persistence/TaskSqlCodec.h"

#include "repositories/RepositoryException.h"

namespace smartmate::model::persistence::detail {

QString taskPriorityToSqlText(const TaskPriority priority)
{
    switch (priority) {
    case TaskPriority::Low: return QStringLiteral("low");
    case TaskPriority::Normal: return QStringLiteral("normal");
    case TaskPriority::High: return QStringLiteral("high");
    case TaskPriority::Urgent: return QStringLiteral("urgent");
    }
    throw RepositoryException(QStringLiteral("Cannot store an unknown task priority"));
}

TaskPriority taskPriorityFromSqlText(const QString &text)
{
    if (text == QStringLiteral("low")) return TaskPriority::Low;
    if (text == QStringLiteral("normal")) return TaskPriority::Normal;
    if (text == QStringLiteral("high")) return TaskPriority::High;
    if (text == QStringLiteral("urgent")) return TaskPriority::Urgent;
    throw RepositoryException(
        QStringLiteral("Unknown task priority in SQLite: %1").arg(text));
}

QString taskStatusToSqlText(const TaskStatus status)
{
    switch (status) {
    case TaskStatus::Todo: return QStringLiteral("todo");
    case TaskStatus::InProgress: return QStringLiteral("in_progress");
    case TaskStatus::Done: return QStringLiteral("done");
    case TaskStatus::Cancelled: return QStringLiteral("cancelled");
    case TaskStatus::Archived: return QStringLiteral("archived");
    }
    throw RepositoryException(QStringLiteral("Cannot store an unknown task status"));
}

TaskStatus taskStatusFromSqlText(const QString &text)
{
    if (text == QStringLiteral("todo")) return TaskStatus::Todo;
    if (text == QStringLiteral("in_progress")) return TaskStatus::InProgress;
    if (text == QStringLiteral("done")) return TaskStatus::Done;
    if (text == QStringLiteral("cancelled")) return TaskStatus::Cancelled;
    if (text == QStringLiteral("archived")) return TaskStatus::Archived;
    throw RepositoryException(
        QStringLiteral("Unknown task status in SQLite: %1").arg(text));
}

QString taskCategoryColorToSqlText(const TaskCategoryColor color)
{
    switch (color) {
    case TaskCategoryColor::Blue: return QStringLiteral("blue");
    case TaskCategoryColor::Teal: return QStringLiteral("teal");
    case TaskCategoryColor::Green: return QStringLiteral("green");
    case TaskCategoryColor::Amber: return QStringLiteral("amber");
    case TaskCategoryColor::Orange: return QStringLiteral("orange");
    case TaskCategoryColor::Rose: return QStringLiteral("rose");
    case TaskCategoryColor::Violet: return QStringLiteral("violet");
    case TaskCategoryColor::Slate: return QStringLiteral("slate");
    }
    throw RepositoryException(
        QStringLiteral("Cannot store an unknown task category color"));
}

TaskCategoryColor taskCategoryColorFromSqlText(const QString &text)
{
    if (text == QStringLiteral("blue")) return TaskCategoryColor::Blue;
    if (text == QStringLiteral("teal")) return TaskCategoryColor::Teal;
    if (text == QStringLiteral("green")) return TaskCategoryColor::Green;
    if (text == QStringLiteral("amber")) return TaskCategoryColor::Amber;
    if (text == QStringLiteral("orange")) return TaskCategoryColor::Orange;
    if (text == QStringLiteral("rose")) return TaskCategoryColor::Rose;
    if (text == QStringLiteral("violet")) return TaskCategoryColor::Violet;
    if (text == QStringLiteral("slate")) return TaskCategoryColor::Slate;
    throw RepositoryException(
        QStringLiteral("Unknown task category color in SQLite: %1").arg(text));
}

QString taskTransitionToSqlText(const TaskTransition transition)
{
    switch (transition) {
    case TaskTransition::Start: return QStringLiteral("start");
    case TaskTransition::Cancel: return QStringLiteral("cancel");
    case TaskTransition::Complete: return QStringLiteral("complete");
    case TaskTransition::Redo: return QStringLiteral("redo");
    case TaskTransition::Archive: return QStringLiteral("archive");
    case TaskTransition::Restore: return QStringLiteral("restore");
    }
    throw RepositoryException(QStringLiteral("Cannot store an unknown task transition"));
}

TaskTransition taskTransitionFromSqlText(const QString &text)
{
    if (text == QStringLiteral("start")) return TaskTransition::Start;
    if (text == QStringLiteral("cancel")) return TaskTransition::Cancel;
    if (text == QStringLiteral("complete")) return TaskTransition::Complete;
    if (text == QStringLiteral("redo")) return TaskTransition::Redo;
    if (text == QStringLiteral("archive")) return TaskTransition::Archive;
    if (text == QStringLiteral("restore")) return TaskTransition::Restore;
    throw RepositoryException(
        QStringLiteral("Unknown task transition in SQLite: %1").arg(text));
}

} // namespace smartmate::model::persistence::detail
