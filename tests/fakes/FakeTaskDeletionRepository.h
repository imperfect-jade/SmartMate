#pragma once

#include "fakes/FakeTaskDependencyRepository.h"
#include "fakes/FakeTaskRepository.h"

#include "repositories/ITaskDeletionRepository.h"

#include <QList>

#include <algorithm>
#include <optional>
#include <utility>

namespace smartmate::tests {

/// Service测试使用的永久删除端口，可控制事务结果、故障并记录稳定TaskId调用。
class FakeTaskDeletionRepository final
    : public model::ITaskDeletionRepository {
public:
    FakeTaskDeletionRepository() = default;

    FakeTaskDeletionRepository(FakeTaskRepository &taskRepository,
                               FakeTaskDependencyRepository &dependencyRepository)
        : m_taskRepository(&taskRepository)
        , m_dependencyRepository(&dependencyRepository)
    {
    }

    [[nodiscard]] model::TaskDeletionWriteResult
    deleteArchivedTasksWithDependencies(
        const QList<model::TaskId> &taskIds) override
    {
        if (m_failWrites) {
            throw model::RepositoryException("Fake permanent deletion failure.");
        }
        m_deletedTaskIds.append(taskIds);
        if (m_missingConflictOnNextWrite && m_taskRepository != nullptr) {
            m_missingConflictOnNextWrite = false;
            // 模拟另一实例在Service预检后先行删除目标，供错误语义重读测试使用。
            for (const model::TaskId &taskId : taskIds) {
                m_taskRepository->removeTaskForAtomicRollback(taskId);
            }
            return {0, 0, taskIds};
        }
        if (m_forcedResult.has_value()) {
            return *m_forcedResult;
        }
        if (m_taskRepository == nullptr || m_dependencyRepository == nullptr) {
            return {static_cast<int>(taskIds.size()), 0, {}};
        }

        QList<model::TaskId> conflicts;
        for (const model::TaskId &taskId : taskIds) {
            const auto taskIterator = std::find_if(
                m_taskRepository->m_tasks.cbegin(),
                m_taskRepository->m_tasks.cend(),
                [&taskId](const model::Task &task) { return task.id() == taskId; });
            if (taskIterator == m_taskRepository->m_tasks.cend()
                || taskIterator->status() != model::TaskStatus::Archived) {
                conflicts.append(taskId);
            }
        }
        if (!conflicts.isEmpty()) {
            return {0, 0, std::move(conflicts)};
        }

        QList<model::TaskDependency> retainedDependencies;
        retainedDependencies.reserve(m_dependencyRepository->m_dependencies.size());
        int removedDependencyCount = 0;
        for (const model::TaskDependency &dependency
             : std::as_const(m_dependencyRepository->m_dependencies)) {
            const bool touchesDeletedTask = std::any_of(
                taskIds.cbegin(), taskIds.cend(), [&dependency](const model::TaskId &taskId) {
                    return dependency.predecessorId == taskId
                        || dependency.successorId == taskId;
                });
            if (touchesDeletedTask) {
                ++removedDependencyCount;
            } else {
                retainedDependencies.append(dependency);
            }
        }

        // 两个容器只在所有资格检查完成后一起替换，模拟真实Repository的事务发布点。
        m_taskRepository->m_tasks.erase(
            std::remove_if(m_taskRepository->m_tasks.begin(),
                           m_taskRepository->m_tasks.end(),
                           [&taskIds](const model::Task &task) {
                               return taskIds.contains(task.id());
                           }),
            m_taskRepository->m_tasks.end());
        m_dependencyRepository->m_dependencies = std::move(retainedDependencies);
        return {static_cast<int>(taskIds.size()), removedDependencyCount, {}};
    }

    void setResult(const model::TaskDeletionWriteResult result) noexcept
    {
        m_forcedResult = result;
    }

    void setWriteFailure(const bool enabled) noexcept
    {
        m_failWrites = enabled;
    }

    void setMissingConflictOnNextWrite(const bool enabled) noexcept
    {
        m_missingConflictOnNextWrite = enabled;
    }

    [[nodiscard]] const QList<model::TaskId> &deletedTaskIds() const noexcept
    {
        return m_deletedTaskIds;
    }

private:
    FakeTaskRepository *m_taskRepository{nullptr};
    FakeTaskDependencyRepository *m_dependencyRepository{nullptr};
    std::optional<model::TaskDeletionWriteResult> m_forcedResult;
    QList<model::TaskId> m_deletedTaskIds;
    bool m_failWrites{false};
    bool m_missingConflictOnNextWrite{false};
};

} // namespace smartmate::tests
