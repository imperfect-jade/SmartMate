#pragma once

#include "fakes/FakeTaskRepository.h"

#include "repositories/ITaskTransitionRepository.h"

#include <QList>

#include <algorithm>
#include <optional>
#include <utility>

namespace smartmate::tests {

/// Service测试用原子批量状态端口；先验证全部expectedStatus，再一次性发布内存快照。
class FakeTaskBatchTransitionRepository final
    : public model::ITaskTransitionRepository {
public:
    explicit FakeTaskBatchTransitionRepository(FakeTaskRepository &repository)
        : m_repository(repository)
    {
    }

    [[nodiscard]] model::TaskTransitionWriteResult applyTransitionsAtomically(
        const QList<model::TaskTransitionWrite> &writes) override
    {
        ++m_callCount;
        m_lastWrites = writes;
        m_repository.injectCompetingTaskAndThrow();
        m_repository.throwIfWriteFails();
        if (m_failWrites) {
            throw model::RepositoryException("Fake atomic batch transition failure.");
        }
        if (m_forcedResult.has_value()) {
            return *m_forcedResult;
        }

        QList<model::TaskId> conflicts;
        QList<model::Task> replacement = m_repository.m_tasks;
        for (const model::TaskTransitionWrite &write : writes) {
            const model::TaskStateChange &change = write.stateChange;
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
            return {0, 0, std::move(conflicts)};
        }
        m_repository.m_tasks = std::move(replacement);
        m_repository.m_updateCount += writes.size();
        for (const model::TaskTransitionWrite &write : writes) {
            m_events.append(write.event);
        }
        return {static_cast<int>(writes.size()),
                static_cast<int>(writes.size()), {}};
    }

    void setResult(model::TaskTransitionWriteResult result)
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
        m_lastChanges.clear();
        for (const model::TaskTransitionWrite &write : m_lastWrites) {
            m_lastChanges.append(write.stateChange);
        }
        return m_lastChanges;
    }

    [[nodiscard]] const QList<model::TaskTransitionWrite> &lastWrites() const noexcept
    {
        return m_lastWrites;
    }

    [[nodiscard]] const QList<model::TaskActivityEvent> &events() const noexcept
    {
        return m_events;
    }

private:
    FakeTaskRepository &m_repository;
    std::optional<model::TaskTransitionWriteResult> m_forcedResult;
    QList<model::TaskTransitionWrite> m_lastWrites;
    QList<model::TaskActivityEvent> m_events;
    mutable QList<model::TaskStateChange> m_lastChanges;
    int m_callCount{0};
    bool m_failWrites{false};
};

} // namespace smartmate::tests
