#pragma once

#include "repositories/ITaskDependencyRepository.h"

#include <QList>

#include <utility>

namespace smartmate::tests {

class FakeTaskDeletionRepository;

/// Model 测试使用的依赖端口，可独立注入读写故障并观察原子替换次数。
class FakeTaskDependencyRepository final
    : public model::ITaskDependencyRepository {
public:
    explicit FakeTaskDependencyRepository(
        QList<model::TaskDependency> dependencies = {})
        : m_dependencies(std::move(dependencies))
    {
    }

    [[nodiscard]] QList<model::TaskDependency> findAllDependencies() const override
    {
        ++m_findAllCount;
        if (m_failReads) {
            throw model::RepositoryException("Fake dependency read failure.");
        }
        return m_dependencies;
    }

    void replacePredecessors(
        const model::TaskId &successorId,
        const QList<model::TaskId> &predecessorIds) override
    {
        if (m_failWrites) {
            throw model::RepositoryException("Fake dependency write failure.");
        }

        QList<model::TaskDependency> replacement;
        replacement.reserve(m_dependencies.size() + predecessorIds.size());
        for (const model::TaskDependency &dependency : m_dependencies) {
            if (dependency.successorId != successorId) {
                replacement.append(dependency);
            }
        }
        for (const model::TaskId &predecessorId : predecessorIds) {
            replacement.append({predecessorId, successorId});
        }
        m_dependencies = std::move(replacement);
        ++m_replaceCount;
    }

    void setReadFailure(const bool enabled) noexcept
    {
        m_failReads = enabled;
    }

    void setWriteFailure(const bool enabled) noexcept
    {
        m_failWrites = enabled;
    }

    [[nodiscard]] const QList<model::TaskDependency> &dependencies() const noexcept
    {
        return m_dependencies;
    }

    [[nodiscard]] int replaceCount() const noexcept
    {
        return m_replaceCount;
    }

    [[nodiscard]] int findAllCount() const noexcept { return m_findAllCount; }

private:
    friend class FakeTaskDeletionRepository;

    QList<model::TaskDependency> m_dependencies;
    int m_replaceCount{0};
    mutable int m_findAllCount{0};
    bool m_failReads{false};
    bool m_failWrites{false};
};

} // namespace smartmate::tests
