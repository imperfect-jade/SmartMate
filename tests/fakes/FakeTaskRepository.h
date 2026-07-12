#pragma once

#include "repositories/ITaskRepository.h"

#include <QList>

#include <algorithm>
#include <utility>

namespace smartmate::tests {

class FakeTaskDeletionRepository;
class FakeTaskBatchTransitionRepository;

/// 可控内存端口同时模拟常规读写故障，以及预检后出现另一进行中任务的竞争窗口。
class FakeTaskRepository final : public model::ITaskRepository {
public:
    explicit FakeTaskRepository(QList<model::Task> tasks = {})
        : m_tasks(std::move(tasks))
    {
    }

    [[nodiscard]] QList<model::Task> findAll() const override
    {
        throwIfReadFails();
        return m_tasks;
    }

    [[nodiscard]] std::optional<model::Task> findById(const model::TaskId &id) const override
    {
        throwIfReadFails();
        const auto iterator = std::find_if(m_tasks.cbegin(), m_tasks.cend(),
                                           [&id](const model::Task &task) {
                                               return task.id() == id;
                                           });
        if (iterator == m_tasks.cend()) {
            return std::nullopt;
        }
        return *iterator;
    }

    void insert(const model::Task &task) override
    {
        injectCompetingTaskAndThrow();
        throwIfWriteFails();
        if (findIndex(task.id()) >= 0) {
            throw model::RepositoryException("Duplicate task ID.");
        }
        m_tasks.append(task);
        ++m_insertCount;
    }

    [[nodiscard]] bool update(const model::Task &task) override
    {
        injectCompetingTaskAndThrow();
        throwIfWriteFails();
        const qsizetype index = findIndex(task.id());
        if (index < 0) {
            return false;
        }
        m_tasks[index] = task;
        ++m_updateCount;
        return true;
    }

    void setReadFailure(bool enabled) noexcept
    {
        m_failReads = enabled;
    }

    void setWriteFailure(bool enabled) noexcept
    {
        m_failWrites = enabled;
    }

    void setCompetingTaskOnNextWrite(model::Task task)
    {
        m_competingTaskOnNextWrite = std::move(task);
    }

    [[nodiscard]] const QList<model::Task> &tasks() const noexcept
    {
        return m_tasks;
    }

    [[nodiscard]] int insertCount() const noexcept
    {
        return m_insertCount;
    }

    [[nodiscard]] int updateCount() const noexcept
    {
        return m_updateCount;
    }

    /// 仅供原子创建 Fake 在第二阶段失败时撤销尚未对外成功的测试写入。
    void removeTaskForAtomicRollback(const model::TaskId &id)
    {
        const qsizetype index = findIndex(id);
        if (index >= 0) {
            m_tasks.removeAt(index);
        }
    }

private:
    friend class FakeTaskDeletionRepository;
    friend class FakeTaskBatchTransitionRepository;

    [[nodiscard]] qsizetype findIndex(const model::TaskId &id) const
    {
        for (qsizetype index = 0; index < m_tasks.size(); ++index) {
            if (m_tasks.at(index).id() == id) {
                return index;
            }
        }
        return -1;
    }

    void throwIfReadFails() const
    {
        if (m_failReads) {
            throw model::RepositoryException("Fake repository read failure.");
        }
    }

    void throwIfWriteFails() const
    {
        if (m_failWrites) {
            throw model::RepositoryException("Fake repository write failure.");
        }
    }

    void injectCompetingTaskAndThrow()
    {
        if (!m_competingTaskOnNextWrite.has_value()) {
            return;
        }

        // 竞争任务恰好在目标写入前落库，以验证 Service 的失败后重读防线。
        model::Task competingTask = std::move(*m_competingTaskOnNextWrite);
        m_competingTaskOnNextWrite.reset();
        m_tasks.append(std::move(competingTask));
        throw model::RepositoryException("Simulated concurrent task write.");
    }

    QList<model::Task> m_tasks;
    std::optional<model::Task> m_competingTaskOnNextWrite;
    int m_insertCount{0};
    int m_updateCount{0};
    bool m_failReads{false};
    bool m_failWrites{false};
};

} // namespace smartmate::tests
