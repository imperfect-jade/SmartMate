#pragma once

#include "domain/TaskCategory.h"

#include <QList>

#include <optional>

namespace smartmate::model {

/// 删除类别的原子写入结果；任务只解除归属，不删除任务或依赖边。
struct CategoryDeletionWriteResult final {
    bool categoryDeleted{false};
    int unassignedTaskCount{0};
};

/// 类别持久化端口；方法名显式携带Category以便SQLite实现可同时实现任务端口。
class ITaskCategoryRepository {
public:
    virtual ~ITaskCategoryRepository() = default;

    [[nodiscard]] virtual QList<TaskCategory> findAllCategories() const = 0;
    [[nodiscard]] virtual std::optional<TaskCategory> findCategoryById(
        const TaskCategoryId &id) const = 0;
    virtual void insertCategory(const TaskCategory &category) = 0;
    [[nodiscard]] virtual bool updateCategory(const TaskCategory &category) = 0;

    /// 同一事务内解除全部任务归属并删除类别；不得改动任务状态或依赖边。
    [[nodiscard]] virtual CategoryDeletionWriteResult
    deleteCategoryAndUnassignTasks(const TaskCategoryId &id,
                                   const QDateTime &updatedAtUtc) = 0;
};

} // namespace smartmate::model
