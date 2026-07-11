#include "AppViewModel.h"

namespace smartmate::viewmodel {

AppViewModel::AppViewModel(model::TaskService &taskService, QObject *parent)
    : QObject(parent)
    , m_taskList(taskService)
    , m_taskEditor(taskService)
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

} // namespace smartmate::viewmodel
