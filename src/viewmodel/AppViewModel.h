#pragma once

#include "TaskDependencyViewModel.h"
#include "AppearanceSettingsViewModel.h"
#include "TaskEditorViewModel.h"
#include "TaskCategoryViewModel.h"
#include "TaskGraphViewModel.h"
#include "TaskListViewModel.h"
#include "TaskFocusViewModel.h"
#include "TaskDetailsViewModel.h"
#include "StatisticsViewModel.h"

#include <QObject>

#include <memory>

namespace smartmate::model {
class TaskService;
class AppearanceSettingsService;
class TaskCategoryService;
class StatisticsService;
}

namespace smartmate::viewmodel {

/// 应用级 ViewModel 只负责组合子 ViewModel；它不保存界面控件，
/// 也不把具体 Repository 或数据库实现泄漏给 View。
/// 子 ViewModel 通过共享投影源同步读取，彼此不直接调用。
class AppViewModel : public QObject {
public:
    /// 下列构造重载用于按功能注入 Service；正式组合根使用完整依赖版本。
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
    AppViewModel(model::TaskService &taskService,
                 model::StatisticsService &statisticsService,
                 QObject *parent = nullptr);
    AppViewModel(model::TaskService &taskService,
                 model::TaskCategoryService &categoryService,
                 model::StatisticsService &statisticsService,
                 model::AppearanceSettingsService &appearanceService,
                 QObject *parent = nullptr);

    // 返回稳定子对象地址，供组合根以对应 Contract 引用注入 Widget；调用方不拥有指针。
    [[nodiscard]] TaskListViewModel *taskList() noexcept;
    [[nodiscard]] TaskFocusViewModel *taskFocus() noexcept;
    [[nodiscard]] TaskDetailsViewModel *taskDetails() noexcept;
    [[nodiscard]] TaskEditorViewModel *taskEditor() noexcept;
    [[nodiscard]] TaskDependencyViewModel *taskDependencies() noexcept;
    [[nodiscard]] TaskGraphViewModel *taskGraph() noexcept;
    [[nodiscard]] TaskCategoryViewModel *taskCategories() noexcept;
    /// 兼容构造未注入 StatisticsService 时返回空；正式统计构造保证地址稳定且非空。
    [[nodiscard]] StatisticsViewModel *statistics() noexcept;
    [[nodiscard]] AppearanceSettingsViewModel *appearanceSettings() noexcept;

private:
    /// 投影源必须早于所有消费者构造，并与 AppViewModel 同生命周期。
    TaskPlanProjectionSource m_taskPlanSource;
    TaskCategoryProjectionSource m_taskCategorySource;
    /// 值成员与 AppViewModel 同生命周期，地址在运行期间稳定。
    TaskCategoryViewModel m_taskCategories;
    TaskListViewModel m_taskList;
    TaskFocusViewModel m_taskFocus;
    TaskDetailsViewModel m_taskDetails;
    TaskEditorViewModel m_taskEditor;
    TaskDependencyViewModel m_taskDependencies;
    TaskGraphViewModel m_taskGraph;
    std::unique_ptr<StatisticsViewModel> m_statistics;
    AppearanceSettingsViewModel m_appearanceSettings;
};

} // namespace smartmate::viewmodel
