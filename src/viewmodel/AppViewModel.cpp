#include "AppViewModel.h"

namespace smartmate::viewmodel {

AppViewModel::AppViewModel(model::TaskService &taskService, QObject *parent)
    : QObject(parent)
    , m_taskList(taskService)
    , m_taskEditor(taskService)
    , m_taskDependencies(new TaskDependencyViewModel(taskService, this))
    , m_taskGraph(taskService)
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

} // namespace smartmate::viewmodel
