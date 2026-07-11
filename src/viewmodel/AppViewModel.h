#pragma once

#include "TaskEditorViewModel.h"
#include "TaskListViewModel.h"

#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

namespace smartmate::model {
class TaskService;
}

namespace smartmate::viewmodel {

/// 应用级 ViewModel 只负责向 View 暴露子 ViewModel；它不保存界面控件，
/// 也不把具体 Repository 或数据库实现泄漏给 QML。
class AppViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString applicationName READ applicationName CONSTANT)
    Q_PROPERTY(smartmate::viewmodel::TaskListViewModel *taskList READ taskList CONSTANT)
    Q_PROPERTY(smartmate::viewmodel::TaskEditorViewModel *taskEditor READ taskEditor CONSTANT)
    QML_NAMED_ELEMENT(AppViewModel)
    QML_UNCREATABLE("AppViewModel is created and owned by AppBootstrapper")

public:
    explicit AppViewModel(model::TaskService &taskService, QObject *parent = nullptr);

    [[nodiscard]] QString applicationName() const;
    [[nodiscard]] TaskListViewModel *taskList() noexcept;
    [[nodiscard]] TaskEditorViewModel *taskEditor() noexcept;

private:
    // 两个子 ViewModel 共享同一个 TaskService，通过 Service 的状态变化保持同步，
    // 但彼此不直接调用，从而避免 ViewModel 之间形成隐式耦合。
    TaskListViewModel m_taskList;
    TaskEditorViewModel m_taskEditor;
};

} // namespace smartmate::viewmodel
