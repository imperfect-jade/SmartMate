#pragma once

#include "TaskProjectionSources.h"
#include "viewmodel/contracts/TaskFocusContract.h"

namespace smartmate::viewmodel {

/// 从共享计划与类别源投影焦点任务，不读取列表筛选或其他 ViewModel。
class TaskFocusViewModel final : public TaskFocusContract {
    Q_OBJECT
public:
    TaskFocusViewModel(TaskPlanProjectionSource &planSource,
                       TaskCategoryProjectionSource &categorySource,
                       QObject *parent = nullptr);

    [[nodiscard]] FocusState focusState() const noexcept override;
    [[nodiscard]] QString focusTaskId() const override;
    [[nodiscard]] QString focusTitle() const override;
    [[nodiscard]] QString focusDescription() const override;
    [[nodiscard]] QString focusStatusText() const override;
    [[nodiscard]] QString focusPriorityText() const override;
    [[nodiscard]] QString focusDeadlineText() const override;
    [[nodiscard]] int focusEstimatedMinutes() const noexcept override;
    [[nodiscard]] QString focusEstimatedText() const override;
    [[nodiscard]] QString focusReasonText() const override;
    [[nodiscard]] bool focusOverdue() const noexcept override;
    [[nodiscard]] bool focusCanStart() const noexcept override;
    [[nodiscard]] bool focusCanComplete() const noexcept override;
    [[nodiscard]] QString focusCategoryName() const override;
    [[nodiscard]] QString focusCategoryAccent() const override;
    [[nodiscard]] bool focusHasCategory() const noexcept override;

private:
    /// 共享计划变化后决定 FocusState 与稳定焦点 TaskId。
    void applyPlanProjection();
    void applyCategories();
    [[nodiscard]] const model::Task *focusTask() const;
    [[nodiscard]] const model::TaskCategory *focusCategory() const;

    TaskPlanProjectionSource &m_planSource;
    TaskCategoryProjectionSource &m_categorySource;
    /// 当前焦点稳定身份；NoTasks/AllBlocked 时可为空。
    model::TaskId m_focusTaskId;
    FocusState m_focusState{FocusState::NoTasks};
};

} // namespace smartmate::viewmodel
