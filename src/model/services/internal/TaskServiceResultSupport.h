#pragma once

#include "dependencies/TaskDependencyGraph.h"
#include "services/TaskResult.h"

namespace smartmate::model {
class RepositoryException;

namespace task_service_detail {

/// 将 Repository 异常统一转换为稳定的 TaskService 结果，禁止异常越过 Model 边界。
[[nodiscard]] TaskResult persistenceFailure(
    const RepositoryException &exception);
[[nodiscard]] TaskResult unexpectedPersistenceFailure();
[[nodiscard]] TaskBatchResult batchPersistenceFailure(
    const RepositoryException &exception);
[[nodiscard]] TaskBatchResult unexpectedBatchPersistenceFailure();
[[nodiscard]] TaskListResult persistenceListFailure(
    const RepositoryException &exception);
[[nodiscard]] TaskListResult unexpectedPersistenceListFailure();
[[nodiscard]] TaskPlanResult persistencePlanFailure(
    const RepositoryException &exception);
[[nodiscard]] TaskPlanResult unexpectedPersistencePlanFailure();
[[nodiscard]] TaskGraphResult persistenceGraphFailure(
    const RepositoryException &exception);
[[nodiscard]] TaskGraphResult unexpectedPersistenceGraphFailure();
[[nodiscard]] TaskDependencyListResult dependencyPersistenceFailure(
    const RepositoryException &exception);
[[nodiscard]] TaskDependencyListResult
unexpectedDependencyPersistenceFailure();

/// 将依赖图内部错误收窄为 TaskService 对外使用的稳定错误契约。
[[nodiscard]] TaskError graphError(DependencyGraphError error) noexcept;
[[nodiscard]] TaskErrorContext graphContext(
    const DependencyGraphValidation &validation);

template<typename T>
[[nodiscard]] ServiceResult<T> graphFailure(
    const DependencyGraphValidation &validation)
{
    return ServiceResult<T>::failure(
        graphError(validation.error),
        QStringLiteral("Task dependency graph validation failed."),
        graphContext(validation));
}

} // namespace task_service_detail
} // namespace smartmate::model
