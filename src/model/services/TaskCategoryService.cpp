#include "services/TaskCategoryService.h"

#include "repositories/RepositoryException.h"

#include <QDateTime>

#include <algorithm>
#include <exception>
#include <utility>

namespace smartmate::model {
namespace {

constexpr qsizetype maximumCategoryNameLength = 50;

[[nodiscard]] QString stableCategoryId(const TaskCategoryId &id)
{
    return id.toString(QUuid::WithoutBraces);
}

[[nodiscard]] std::optional<TaskCategoryError> validateDraft(
    const TaskCategoryDraft &draft)
{
    const QString trimmedName = draft.name.trimmed();
    if (trimmedName.isEmpty()) {
        return TaskCategoryError::EmptyName;
    }
    // QString::size()统计UTF-16 code unit，会把补充平面字符计为两个；
    // 领域“字符”边界按Unicode code point计算，并与SQLite length()保持一致。
    if (trimmedName.toUcs4().size() > maximumCategoryNameLength) {
        return TaskCategoryError::NameTooLong;
    }
    if (!isValidTaskCategoryColor(draft.color)) {
        return TaskCategoryError::InvalidColor;
    }
    return std::nullopt;
}

[[nodiscard]] QString validationDetail(const TaskCategoryError error)
{
    switch (error) {
    case TaskCategoryError::EmptyName:
        return QStringLiteral("Task category name must not be empty.");
    case TaskCategoryError::NameTooLong:
        return QStringLiteral("Task category name exceeds 50 characters.");
    case TaskCategoryError::InvalidColor:
        return QStringLiteral("Task category color is invalid.");
    default:
        return QStringLiteral("Task category validation failed.");
    }
}

[[nodiscard]] bool hasDuplicateName(const QList<TaskCategory> &categories,
                                    const QString &nameKey,
                                    const std::optional<TaskCategoryId> &excludedId)
{
    return std::any_of(categories.cbegin(), categories.cend(),
                       [&nameKey, &excludedId](const TaskCategory &category) {
        return (!excludedId.has_value() || category.id != *excludedId)
            && taskCategoryNameKey(category.name) == nameKey;
    });
}

void sortCategories(QList<TaskCategory> &categories)
{
    std::sort(categories.begin(), categories.end(),
              [](const TaskCategory &left, const TaskCategory &right) {
        const QString leftKey = taskCategoryNameKey(left.name);
        const QString rightKey = taskCategoryNameKey(right.name);
        if (leftKey != rightKey) {
            return leftKey < rightKey;
        }
        return stableCategoryId(left.id) < stableCategoryId(right.id);
    });
}

template<typename Result>
[[nodiscard]] Result persistenceFailure(const RepositoryException &exception)
{
    return Result::failure(TaskCategoryError::PersistenceFailure,
                           QString::fromUtf8(exception.what()));
}

template<typename Result>
[[nodiscard]] Result unexpectedPersistenceFailure()
{
    return Result::failure(TaskCategoryError::PersistenceFailure,
                           QStringLiteral("Unexpected task category repository failure."));
}

} // namespace

TaskCategoryService::TaskCategoryService(ITaskCategoryRepository &repository,
                                         QObject *parent)
    : QObject(parent)
    , m_repository(repository)
{
}

TaskCategoryListResult TaskCategoryService::listCategories() const
{
    try {
        QList<TaskCategory> categories = m_repository.findAllCategories();
        sortCategories(categories);
        return TaskCategoryListResult::success(std::move(categories));
    } catch (const RepositoryException &exception) {
        return persistenceFailure<TaskCategoryListResult>(exception);
    } catch (...) {
        return unexpectedPersistenceFailure<TaskCategoryListResult>();
    }
}

TaskCategoryResult TaskCategoryService::createCategory(
    const TaskCategoryDraft &draft)
{
    if (const auto error = validateDraft(draft)) {
        return TaskCategoryResult::failure(*error, validationDetail(*error));
    }

    try {
        const QList<TaskCategory> categories = m_repository.findAllCategories();
        const QString name = draft.name.trimmed();
        if (hasDuplicateName(categories, taskCategoryNameKey(name), std::nullopt)) {
            return TaskCategoryResult::failure(
                TaskCategoryError::DuplicateName,
                QStringLiteral("Task category name already exists."));
        }

        TaskCategoryId id;
        do {
            id = QUuid::createUuid();
        } while (std::any_of(categories.cbegin(), categories.cend(),
                             [&id](const TaskCategory &category) {
            return category.id == id;
        }));
        const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
        TaskCategory category{id, name, draft.color, nowUtc, nowUtc};
        m_repository.insertCategory(category);
        emit categoriesChanged();
        return TaskCategoryResult::success(std::move(category));
    } catch (const RepositoryException &exception) {
        return persistenceFailure<TaskCategoryResult>(exception);
    } catch (...) {
        return unexpectedPersistenceFailure<TaskCategoryResult>();
    }
}

TaskCategoryResult TaskCategoryService::updateCategory(
    const TaskCategoryId &id,
    const TaskCategoryDraft &draft)
{
    if (const auto error = validateDraft(draft)) {
        return TaskCategoryResult::failure(*error, validationDetail(*error));
    }

    try {
        const std::optional<TaskCategory> current =
            m_repository.findCategoryById(id);
        if (!current.has_value()) {
            return TaskCategoryResult::failure(
                TaskCategoryError::NotFound,
                QStringLiteral("Task category was not found."));
        }

        const QList<TaskCategory> categories = m_repository.findAllCategories();
        const QString name = draft.name.trimmed();
        if (hasDuplicateName(categories, taskCategoryNameKey(name), id)) {
            return TaskCategoryResult::failure(
                TaskCategoryError::DuplicateName,
                QStringLiteral("Task category name already exists."));
        }
        if (current->name == name && current->color == draft.color) {
            return TaskCategoryResult::success(*current);
        }

        TaskCategory updated{id,
                             name,
                             draft.color,
                             current->createdAtUtc,
                             QDateTime::currentDateTimeUtc()};
        if (!m_repository.updateCategory(updated)) {
            return TaskCategoryResult::failure(
                TaskCategoryError::NotFound,
                QStringLiteral("Task category disappeared during update."));
        }
        emit categoriesChanged();
        return TaskCategoryResult::success(std::move(updated));
    } catch (const RepositoryException &exception) {
        return persistenceFailure<TaskCategoryResult>(exception);
    } catch (...) {
        return unexpectedPersistenceFailure<TaskCategoryResult>();
    }
}

TaskCategoryDeletionResult TaskCategoryService::deleteCategory(
    const TaskCategoryId &id)
{
    try {
        const std::optional<TaskCategory> current =
            m_repository.findCategoryById(id);
        if (!current.has_value()) {
            return TaskCategoryDeletionResult::failure(
                TaskCategoryError::NotFound,
                QStringLiteral("Task category was not found."));
        }

        const CategoryDeletionWriteResult writeResult =
            m_repository.deleteCategoryAndUnassignTasks(
                id, QDateTime::currentDateTimeUtc());
        if (!writeResult.categoryDeleted) {
            return TaskCategoryDeletionResult::failure(
                TaskCategoryError::NotFound,
                QStringLiteral("Task category disappeared during deletion."));
        }
        emit categoriesChanged();
        if (writeResult.unassignedTaskCount > 0) {
            emit taskCategoryAssignmentsChanged();
        }
        return TaskCategoryDeletionResult::success(
            {*current, writeResult.unassignedTaskCount});
    } catch (const RepositoryException &exception) {
        return persistenceFailure<TaskCategoryDeletionResult>(exception);
    } catch (...) {
        return unexpectedPersistenceFailure<TaskCategoryDeletionResult>();
    }
}

} // namespace smartmate::model
