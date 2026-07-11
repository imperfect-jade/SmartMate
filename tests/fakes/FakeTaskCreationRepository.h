#pragma once

#include "fakes/FakeTaskDependencyRepository.h"
#include "fakes/FakeTaskRepository.h"

#include "repositories/ITaskCreationRepository.h"

namespace smartmate::tests {

/// 测试用原子创建端口，将任务和依赖两个 Fake 的变更组合成同一成功边界。
class FakeTaskCreationRepository final
    : public model::ITaskCreationRepository {
public:
    FakeTaskCreationRepository(FakeTaskRepository &taskRepository,
                               FakeTaskDependencyRepository &dependencyRepository)
        : m_taskRepository(taskRepository)
        , m_dependencyRepository(dependencyRepository)
    {
    }

    void insertTaskWithPredecessors(
        const model::Task &task,
        const QList<model::TaskId> &predecessorIds) override
    {
        if (m_failWrites) {
            throw model::RepositoryException("Fake atomic creation failure.");
        }

        bool taskInserted = false;
        try {
            m_taskRepository.insert(task);
            taskInserted = true;
            if (!predecessorIds.isEmpty()) {
                m_dependencyRepository.replacePredecessors(task.id(),
                                                           predecessorIds);
            }
        } catch (...) {
            // 依赖阶段失败时只撤销本次尚未发布的任务；并发注入的其他任务应保留。
            if (taskInserted) {
                m_taskRepository.removeTaskForAtomicRollback(task.id());
            }
            throw;
        }
        ++m_insertCount;
    }

    void setWriteFailure(const bool enabled) noexcept
    {
        m_failWrites = enabled;
    }

    [[nodiscard]] int insertCount() const noexcept
    {
        return m_insertCount;
    }

private:
    FakeTaskRepository &m_taskRepository;
    FakeTaskDependencyRepository &m_dependencyRepository;
    int m_insertCount{0};
    bool m_failWrites{false};
};

} // namespace smartmate::tests
