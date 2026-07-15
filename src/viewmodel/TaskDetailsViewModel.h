#pragma once

#include "TaskProjectionSources.h"
#include "viewmodel/contracts/TaskDetailsContract.h"

namespace smartmate::viewmodel {

/// 从共享源读取任务详情，只保存稳定 TaskId，不读取或调用其他 ViewModel。
class TaskDetailsViewModel final : public TaskDetailsContract {
    Q_OBJECT
public:
    TaskDetailsViewModel(TaskPlanProjectionSource &planSource,
                         TaskCategoryProjectionSource &categorySource,
                         QObject *parent = nullptr);

    [[nodiscard]] QString selectedTaskId() const override;
    [[nodiscard]] QString selectedTitle() const override;
    [[nodiscard]] QString selectedDescription() const override;
    [[nodiscard]] QString selectedStatusText() const override;
    [[nodiscard]] QString selectedPriorityText() const override;
    [[nodiscard]] QString selectedDeadlineText() const override;
    [[nodiscard]] int selectedEstimatedMinutes() const noexcept override;
    [[nodiscard]] QString selectedCreatedAtText() const override;
    [[nodiscard]] QString selectedUpdatedAtText() const override;
    [[nodiscard]] QString selectedReasonText() const override;
    [[nodiscard]] QString selectedBlockingReasonText() const override;
    [[nodiscard]] int selectedPredecessorCount() const noexcept override;
    [[nodiscard]] int selectedUnlockCount() const noexcept override;
    [[nodiscard]] bool selectedCanEditTask() const noexcept override;
    [[nodiscard]] bool selectedCanEditDependencies() const noexcept override;
    [[nodiscard]] QString selectedCategoryName() const override;
    [[nodiscard]] QString selectedCategoryAccent() const override;
    [[nodiscard]] bool selectedHasCategory() const noexcept override;

    bool selectTask(const QString &taskId) override;
    void clearSelection() override;

private:
    /// 共享计划变化时校验所选任务，并通知详情 getter 重读。
    void applyPlanProjection();
    void applyCategories();
    /// 在当前计划投影中解析稳定选择；返回指针只在投影未替换期间有效。
    [[nodiscard]] const model::Task *selectedTask() const;
    [[nodiscard]] const model::TaskCategory *selectedCategory() const;

    TaskPlanProjectionSource &m_planSource;
    TaskCategoryProjectionSource &m_categorySource;
    /// 当前详情选择；空 ID 表示未选择。
    model::TaskId m_selectedTaskId;
};

} // namespace smartmate::viewmodel
