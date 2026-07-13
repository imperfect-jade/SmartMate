#include "AppViewModel.h"

namespace smartmate::viewmodel {

AppViewModel::AppViewModel(model::TaskService &taskService, QObject *parent)
    : QObject(parent)
    , m_taskCategories(taskService)
    , m_taskList(taskService)
    , m_taskEditor(taskService)
    , m_taskDependencies(new TaskDependencyViewModel(taskService, this))
    , m_taskGraph(taskService)
    , m_appearanceSettings()
{
}

AppViewModel::AppViewModel(model::TaskService &taskService,
                           model::AppearanceSettingsService &appearanceService,
                           QObject *parent)
    : QObject(parent)
    , m_taskCategories(taskService)
    , m_taskList(taskService)
    , m_taskEditor(taskService)
    , m_taskDependencies(new TaskDependencyViewModel(taskService, this))
    , m_taskGraph(taskService)
    , m_appearanceSettings(appearanceService)
{
}

AppViewModel::AppViewModel(model::TaskService &taskService,
                           model::TaskCategoryService &categoryService,
                           QObject *parent)
    : QObject(parent)
    , m_taskCategories(taskService, &categoryService)
    , m_taskList(taskService, categoryService)
    , m_taskEditor(taskService, categoryService)
    , m_taskDependencies(new TaskDependencyViewModel(
          taskService, categoryService, this))
    , m_taskGraph(taskService, categoryService)
    , m_appearanceSettings()
{
}

AppViewModel::AppViewModel(model::TaskService &taskService,
                           model::TaskCategoryService &categoryService,
                           model::AppearanceSettingsService &appearanceService,
                           QObject *parent)
    : QObject(parent)
    , m_taskCategories(taskService, &categoryService)
    , m_taskList(taskService, categoryService)
    , m_taskEditor(taskService, categoryService)
    , m_taskDependencies(new TaskDependencyViewModel(
          taskService, categoryService, this))
    , m_taskGraph(taskService, categoryService)
    , m_appearanceSettings(appearanceService)
{
}

QString AppViewModel::applicationName() const
{
    return QStringLiteral("SmartMate");
}

TaskListViewModel *AppViewModel::taskList() noexcept
{
    return &m_taskList;
}

TaskEditorViewModel *AppViewModel::taskEditor() noexcept
{
    return &m_taskEditor;
}

TaskDependencyViewModel *AppViewModel::taskDependencies() noexcept
{
    return m_taskDependencies;
}

TaskGraphViewModel *AppViewModel::taskGraph() noexcept
{
    return &m_taskGraph;
}

TaskCategoryViewModel *AppViewModel::taskCategories() noexcept
{
    return &m_taskCategories;
}

AppearanceSettingsViewModel *AppViewModel::appearanceSettings() noexcept
{
    return &m_appearanceSettings;
}

} // namespace smartmate::viewmodel
