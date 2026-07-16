#include "AppViewModel.h"

namespace smartmate::viewmodel {

AppViewModel::AppViewModel(model::TaskService &taskService, QObject *parent)
    : QObject(parent)
    , m_taskPlanSource(taskService)
    , m_taskCategorySource()
    , m_taskCategories(nullptr, m_taskPlanSource, m_taskCategorySource)
    , m_taskList(taskService, m_taskPlanSource, m_taskCategorySource)
    , m_taskFocus(m_taskPlanSource, m_taskCategorySource)
    , m_taskDetails(m_taskPlanSource, m_taskCategorySource)
    , m_taskEditor(taskService, m_taskCategorySource)
    , m_taskDependencies(taskService, m_taskCategorySource)
    , m_taskGraph(taskService, m_taskCategorySource)
    , m_appearanceSettings()
{
}

AppViewModel::AppViewModel(model::TaskService &taskService,
                           model::AppearanceSettingsService &appearanceService,
                           QObject *parent)
    : QObject(parent)
    , m_taskPlanSource(taskService)
    , m_taskCategorySource()
    , m_taskCategories(nullptr, m_taskPlanSource, m_taskCategorySource)
    , m_taskList(taskService, m_taskPlanSource, m_taskCategorySource)
    , m_taskFocus(m_taskPlanSource, m_taskCategorySource)
    , m_taskDetails(m_taskPlanSource, m_taskCategorySource)
    , m_taskEditor(taskService, m_taskCategorySource)
    , m_taskDependencies(taskService, m_taskCategorySource)
    , m_taskGraph(taskService, m_taskCategorySource)
    , m_appearanceSettings(appearanceService)
{
}

AppViewModel::AppViewModel(model::TaskService &taskService,
                           model::TaskCategoryService &categoryService,
                           QObject *parent)
    : QObject(parent)
    , m_taskPlanSource(taskService, &categoryService)
    , m_taskCategorySource(&categoryService)
    , m_taskCategories(&categoryService, m_taskPlanSource, m_taskCategorySource)
    , m_taskList(taskService, m_taskPlanSource, m_taskCategorySource)
    , m_taskFocus(m_taskPlanSource, m_taskCategorySource)
    , m_taskDetails(m_taskPlanSource, m_taskCategorySource)
    , m_taskEditor(taskService, m_taskCategorySource)
    , m_taskDependencies(taskService, m_taskCategorySource)
    , m_taskGraph(taskService, m_taskCategorySource)
    , m_appearanceSettings()
{
}

AppViewModel::AppViewModel(model::TaskService &taskService,
                           model::TaskCategoryService &categoryService,
                           model::AppearanceSettingsService &appearanceService,
                           QObject *parent)
    : QObject(parent)
    , m_taskPlanSource(taskService, &categoryService)
    , m_taskCategorySource(&categoryService)
    , m_taskCategories(&categoryService, m_taskPlanSource, m_taskCategorySource)
    , m_taskList(taskService, m_taskPlanSource, m_taskCategorySource)
    , m_taskFocus(m_taskPlanSource, m_taskCategorySource)
    , m_taskDetails(m_taskPlanSource, m_taskCategorySource)
    , m_taskEditor(taskService, m_taskCategorySource)
    , m_taskDependencies(taskService, m_taskCategorySource)
    , m_taskGraph(taskService, m_taskCategorySource)
    , m_appearanceSettings(appearanceService)
{
    // 子 ViewModel 不相互引用；计划与类别展示通过显式注入的共享源同步。
}

AppViewModel::AppViewModel(model::TaskService &taskService,
                           model::StatisticsService &statisticsService,
                           QObject *parent)
    : QObject(parent)
    , m_taskPlanSource(taskService)
    , m_taskCategorySource()
    , m_taskCategories(nullptr, m_taskPlanSource, m_taskCategorySource)
    , m_taskList(taskService, m_taskPlanSource, m_taskCategorySource)
    , m_taskFocus(m_taskPlanSource, m_taskCategorySource)
    , m_taskDetails(m_taskPlanSource, m_taskCategorySource)
    , m_taskEditor(taskService, m_taskCategorySource)
    , m_taskDependencies(taskService, m_taskCategorySource)
    , m_taskGraph(taskService, m_taskCategorySource)
    , m_statistics(std::make_unique<StatisticsViewModel>(statisticsService,
                                                        taskService))
    , m_appearanceSettings()
{
}

AppViewModel::AppViewModel(model::TaskService &taskService,
                           model::TaskCategoryService &categoryService,
                           model::StatisticsService &statisticsService,
                           model::AppearanceSettingsService &appearanceService,
                           QObject *parent)
    : QObject(parent)
    , m_taskPlanSource(taskService, &categoryService)
    , m_taskCategorySource(&categoryService)
    , m_taskCategories(&categoryService, m_taskPlanSource, m_taskCategorySource)
    , m_taskList(taskService, m_taskPlanSource, m_taskCategorySource)
    , m_taskFocus(m_taskPlanSource, m_taskCategorySource)
    , m_taskDetails(m_taskPlanSource, m_taskCategorySource)
    , m_taskEditor(taskService, m_taskCategorySource)
    , m_taskDependencies(taskService, m_taskCategorySource)
    , m_taskGraph(taskService, m_taskCategorySource)
    , m_statistics(std::make_unique<StatisticsViewModel>(statisticsService,
                                                        taskService))
    , m_appearanceSettings(appearanceService)
{
}

TaskListViewModel *AppViewModel::taskList() noexcept
{
    return &m_taskList;
}

TaskFocusViewModel *AppViewModel::taskFocus() noexcept
{
    return &m_taskFocus;
}

TaskDetailsViewModel *AppViewModel::taskDetails() noexcept
{
    return &m_taskDetails;
}

TaskEditorViewModel *AppViewModel::taskEditor() noexcept
{
    return &m_taskEditor;
}

TaskDependencyViewModel *AppViewModel::taskDependencies() noexcept
{
    return &m_taskDependencies;
}

TaskGraphViewModel *AppViewModel::taskGraph() noexcept
{
    return &m_taskGraph;
}

TaskCategoryViewModel *AppViewModel::taskCategories() noexcept
{
    return &m_taskCategories;
}

StatisticsViewModel *AppViewModel::statistics() noexcept
{
    return m_statistics.get();
}

AppearanceSettingsViewModel *AppViewModel::appearanceSettings() noexcept
{
    return &m_appearanceSettings;
}

} // namespace smartmate::viewmodel
