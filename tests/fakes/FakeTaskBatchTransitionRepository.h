#pragma once

#include "fakes/FakeTaskRepository.h"

#include "repositories/ITaskBatchTransitionRepository.h"

#include <QList>

#include <algorithm>
#include <optional>
#include <utility>

namespace smartmate::tests {

/// Service测试用原子批量状态端口；先验证全部expectedStatus，再一次性发布内存快照。
class FakeTaskBatchTransitionRepository final
    : public model::ITaskBatchTransitionRepository {
public:
    explicit FakeTaskBatchTransitionRepository(FakeTaskRepository &repository)
        : m_repository(repository)
    {
    }

    [[nodiscard]] model::TaskBatchWriteResult updateTaskStatesAtomically(
        const QList<model::TaskStateChange> &changes) override
    {
        ++m_callCount;
        m_lastChanges = changes;
        if (m_failWrites) {
            throw model::RepositoryException("Fake atomic batch transition failure.");
        }
        if (m_forcedResult.has_value()) {
            return *m_forcedResult;
        }

        QList<model::TaskId> conflicts;
        QList<model::Task> replacement = m_repository.m_tasks;
        for (const model::TaskStateChange &change : changes) {
            const auto iterator = std::find_if(
                replacement.begin(), replacement.end(), [&change](const model::Task &task) {
                    return task.id() == change.taskId;
                });
            if (iterator == replacement.end()
                || iterator->status() != change.expectedStatus) {
                conflicts.append(change.taskId);
                continue;
            }
            *iterator = model::Task{iterator->id(),
                                    iterator->title(),
                                    iterator->description(),
                                    iterator->priority(),
                                    change.targetStatus,
                                    change.statusBeforeArchive,
                                    iterator->deadline(),
                                    iterator->estimatedMinutes(),
                                    iterator->createdAtUtc(),
                                    change.updatedAtUtc,
                                    iterator->categoryId()};
        }
        if (!conflicts.isEmpty()) {
            return {0, std::move(conflicts)};
        }
        m_repository.m_tasks = std::move(replacement);
        m_repository.m_updateCount += changes.size();
        return {static_cast<int>(changes.size()), {}};
    }

    void setResult(model::TaskBatchWriteResult result)
    {
        m_forcedResult = std::move(result);
    }

    void setWriteFailure(const bool enabled) noexcept
    {
        m_failWrites = enabled;
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

    [[nodiscard]] const QList<model::TaskStateChange> &lastChanges() const noexcept
    {
        return m_lastChanges;
    }

private:
    FakeTaskRepository &m_repository;
    std::optional<model::TaskBatchWriteResult> m_forcedResult;
    QList<model::TaskStateChange> m_lastChanges;
    int m_callCount{0};
    bool m_failWrites{false};
};

} // namespace smartmate::tests
