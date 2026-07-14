#pragma once

#include "TaskDependencyViewModel.h"
#include "AppearanceSettingsViewModel.h"
#include "TaskEditorViewModel.h"
#include "TaskCategoryViewModel.h"
#include "TaskGraphViewModel.h"
#include "TaskListViewModel.h"
#include "TaskFocusViewModel.h"
#include "TaskDetailsViewModel.h"

#include <QObject>

namespace smartmate::model {
class TaskService;
class AppearanceSettingsService;
class TaskCategoryService;
}

namespace smartmate::viewmodel {

/// 应用级 ViewModel 只负责组合子 ViewModel；它不保存界面控件，
/// 也不把具体 Repository 或数据库实现泄漏给 View。
class AppViewModel : public QObject {
public:
    explicit AppViewModel(model::TaskService &taskService, QObject *parent = nullptr);
    AppViewModel(model::TaskService &taskService,
                 model::AppearanceSettingsService &appearanceService,
                 QObject *parent = nullptr);
    AppViewModel(model::TaskService &taskService,
                 model::TaskCategoryService &categoryService,
                 QObject *parent = nullptr);
    AppViewModel(model::TaskService &taskService,
                 model::TaskCategoryService &categoryService,
                 model::AppearanceSettingsService &appearanceService,
                 QObject *parent = nullptr);

    [[nodiscard]] TaskListViewModel *taskList() noexcept;
    [[nodiscard]] TaskFocusViewModel *taskFocus() noexcept;
    [[nodiscard]] TaskDetailsViewModel *taskDetails() noexcept;
    [[nodiscard]] TaskEditorViewModel *taskEditor() noexcept;
    [[nodiscard]] TaskDependencyViewModel *taskDependencies() noexcept;
    [[nodiscard]] TaskGraphViewModel *taskGraph() noexcept;
    [[nodiscard]] TaskCategoryViewModel *taskCategories() noexcept;
    [[nodiscard]] AppearanceSettingsViewModel *appearanceSettings() noexcept;

private:
    // 子ViewModel共享任务与类别Service的状态通知，但彼此不直接调用，
    // 从而让列表、编辑器、类别管理和依赖图保持同步而不形成隐式耦合。
    TaskCategoryViewModel m_taskCategories;
    TaskListViewModel m_taskList;
    TaskFocusViewModel m_taskFocus;
    TaskDetailsViewModel m_taskDetails;
    TaskEditorViewModel m_taskEditor;
    /// 采用 QObject 子对象所有权，集中管理依赖编辑投影的生命周期。
    TaskDependencyViewModel *m_taskDependencies;
    TaskGraphViewModel m_taskGraph;
    AppearanceSettingsViewModel m_appearanceSettings;
};

} // namespace smartmate::viewmodel
