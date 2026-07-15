#pragma once

#include "repositories/ITaskCategoryRepository.h"
#include "repositories/RepositoryException.h"

#include <algorithm>
#include <optional>
#include <utility>

namespace smartmate::tests {

/// 类别Service和任务类别校验使用的可控内存Repository端口。
class FakeTaskCategoryRepository final
    : public model::ITaskCategoryRepository {
public:
    explicit FakeTaskCategoryRepository(
        QList<model::TaskCategory> categories = {})
        : m_categories(std::move(categories))
    {
    }

    [[nodiscard]] QList<model::TaskCategory> findAllCategories() const override
    {
        ++m_findAllCount;
        throwIfReadFails();
        return m_categories;
    }

    [[nodiscard]] std::optional<model::TaskCategory> findCategoryById(
        const model::TaskCategoryId &id) const override
    {
        throwIfReadFails();
        const qsizetype index = findIndex(id);
        return index < 0
            ? std::nullopt
            : std::optional<model::TaskCategory>{m_categories.at(index)};
    }

    void insertCategory(const model::TaskCategory &category) override
    {
        throwIfWriteFails();
        if (findIndex(category.id) >= 0) {
            throw model::RepositoryException("Duplicate fake category ID.");
        }
        m_categories.append(category);
        ++m_insertCount;
    }

    [[nodiscard]] bool updateCategory(
        const model::TaskCategory &category) override
    {
        throwIfWriteFails();
        const qsizetype index = findIndex(category.id);
        if (index < 0) {
            return false;
        }
        m_categories[index] = category;
        ++m_updateCount;
        return true;
    }

    [[nodiscard]] model::CategoryDeletionWriteResult
    deleteCategoryAndUnassignTasks(
        const model::TaskCategoryId &id,
        const QDateTime &updatedAtUtc) override
    {
        throwIfWriteFails();
        m_lastDeletionTimeUtc = updatedAtUtc;
        const qsizetype index = findIndex(id);
        if (index < 0) {
            return {};
        }
        m_categories.removeAt(index);
        ++m_deleteCount;
        return {true, m_unassignedTaskCount};
    }

    void setReadFailure(bool enabled) noexcept { m_failReads = enabled; }
    void setWriteFailure(bool enabled) noexcept { m_failWrites = enabled; }
    void setUnassignedTaskCount(int count) noexcept
    {
        m_unassignedTaskCount = count;
    }

    [[nodiscard]] const QList<model::TaskCategory> &categories() const noexcept
    {
        return m_categories;
    }
    [[nodiscard]] int insertCount() const noexcept { return m_insertCount; }
    [[nodiscard]] int updateCount() const noexcept { return m_updateCount; }
    [[nodiscard]] int deleteCount() const noexcept { return m_deleteCount; }
    [[nodiscard]] int findAllCount() const noexcept { return m_findAllCount; }
    [[nodiscard]] const QDateTime &lastDeletionTimeUtc() const noexcept
    {
        return m_lastDeletionTimeUtc;
    }

private:
    [[nodiscard]] qsizetype findIndex(const model::TaskCategoryId &id) const
    {
        for (qsizetype index = 0; index < m_categories.size(); ++index) {
            if (m_categories.at(index).id == id) {
                return index;
            }
        }
        return -1;
    }

    void throwIfReadFails() const
    {
        if (m_failReads) {
            throw model::RepositoryException("Fake category read failure.");
        }
    }
    void throwIfWriteFails() const
    {
        if (m_failWrites) {
            throw model::RepositoryException("Fake category write failure.");
        }
    }

    QList<model::TaskCategory> m_categories;
    QDateTime m_lastDeletionTimeUtc;
    int m_unassignedTaskCount{0};
    int m_insertCount{0};
    int m_updateCount{0};
    int m_deleteCount{0};
    mutable int m_findAllCount{0};
    bool m_failReads{false};
    bool m_failWrites{false};
};

} // namespace smartmate::tests
