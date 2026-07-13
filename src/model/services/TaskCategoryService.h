#pragma once

#include "repositories/ITaskCategoryRepository.h"
#include "services/TaskCategoryResult.h"

#include <QObject>

namespace smartmate::model {

/// 类别名称、颜色及生命周期规则的唯一Model入口；不拥有注入的Repository。
class TaskCategoryService final : public QObject {
    Q_OBJECT

public:
    explicit TaskCategoryService(ITaskCategoryRepository &repository,
                                 QObject *parent = nullptr);

    [[nodiscard]] TaskCategoryListResult listCategories() const;
    [[nodiscard]] TaskCategoryResult createCategory(
        const TaskCategoryDraft &draft);
    [[nodiscard]] TaskCategoryResult updateCategory(
        const TaskCategoryId &id,
        const TaskCategoryDraft &draft);
    [[nodiscard]] TaskCategoryDeletionResult deleteCategory(
        const TaskCategoryId &id);

signals:
    /// 类别目录实际创建、修改或删除后发送；无变化保存不会通知。
    void categoriesChanged();
    /// 删除类别实际解除任务归属时发送，任务投影和依赖图据此刷新。
    void taskCategoryAssignmentsChanged();

private:
    ITaskCategoryRepository &m_repository;
};

} // namespace smartmate::model
