#include "services/internal/TaskServiceResultSupport.h"

#include "repositories/RepositoryException.h"

namespace smartmate::model::task_service_detail {

TaskResult persistenceFailure(const RepositoryException &exception)
{
    return TaskResult::failure(TaskError::PersistenceFailure,
                               QString::fromUtf8(exception.what()));
}

TaskResult unexpectedPersistenceFailure()
{
    return TaskResult::failure(TaskError::PersistenceFailure,
                               QStringLiteral("Unexpected repository failure."));
}

TaskBatchResult batchPersistenceFailure(const RepositoryException &exception)
{
    return TaskBatchResult::failure(TaskError::PersistenceFailure,
                                    QString::fromUtf8(exception.what()));
}

TaskBatchResult unexpectedBatchPersistenceFailure()
{
    return TaskBatchResult::failure(
        TaskError::PersistenceFailure,
        QStringLiteral("Unexpected batch repository failure."));
}

TaskListResult persistenceListFailure(const RepositoryException &exception)
{
    return TaskListResult::failure(TaskError::PersistenceFailure,
                                   QString::fromUtf8(exception.what()));
}

TaskListResult unexpectedPersistenceListFailure()
{
    return TaskListResult::failure(TaskError::PersistenceFailure,
                                   QStringLiteral("Unexpected repository failure."));
}

TaskPlanResult persistencePlanFailure(const RepositoryException &exception)
{
    return TaskPlanResult::failure(TaskError::PersistenceFailure,
                                   QString::fromUtf8(exception.what()));
}

TaskPlanResult unexpectedPersistencePlanFailure()
{
    return TaskPlanResult::failure(TaskError::PersistenceFailure,
                                   QStringLiteral("Unexpected repository failure."));
}

TaskGraphResult persistenceGraphFailure(const RepositoryException &exception)
{
    return TaskGraphResult::failure(TaskError::PersistenceFailure,
                                    QString::fromUtf8(exception.what()));
}

TaskGraphResult unexpectedPersistenceGraphFailure()
{
    return TaskGraphResult::failure(
        TaskError::PersistenceFailure,
        QStringLiteral("Unexpected graph repository failure."));
}

TaskDependencyListResult dependencyPersistenceFailure(
    const RepositoryException &exception)
{
    return TaskDependencyListResult::failure(
        TaskError::PersistenceFailure, QString::fromUtf8(exception.what()));
}

TaskDependencyListResult unexpectedDependencyPersistenceFailure()
{
    return TaskDependencyListResult::failure(
        TaskError::PersistenceFailure,
        QStringLiteral("Unexpected dependency repository failure."));
}

TaskError graphError(const DependencyGraphError error) noexcept
{
    switch (error) {
    case DependencyGraphError::None:
        return TaskError::None;
    case DependencyGraphError::MissingTask:
        return TaskError::DependencyEndpointNotFound;
    case DependencyGraphError::SelfDependency:
        return TaskError::DependencySelfReference;
    case DependencyGraphError::DuplicateDependency:
        return TaskError::DependencyDuplicate;
    case DependencyGraphError::Cycle:
        return TaskError::DependencyCycle;
    }
    return TaskError::DependencyCycle;
}

TaskErrorContext graphContext(const DependencyGraphValidation &validation)
{
    return {{}, validation.conflictingTaskIds, validation.cyclePath};
}

} // namespace smartmate::model::task_service_detail
