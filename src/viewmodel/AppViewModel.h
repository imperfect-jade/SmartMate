#pragma once

#include "TaskDependencyViewModel.h"
#include "AppearanceSettingsViewModel.h"
#include "TaskEditorViewModel.h"
#include "TaskGraphViewModel.h"
#include "TaskListViewModel.h"

#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

namespace smartmate::model {
class TaskService;
class AppearanceSettingsService;
}

namespace smartmate::viewmodel {

/// 应用级 ViewModel 只负责向 View 暴露子 ViewModel；它不保存界面控件，
/// 也不把具体 Repository 或数据库实现泄漏给 QML。
class AppViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString applicationName READ applicationName CONSTANT)
    Q_PROPERTY(smartmate::viewmodel::TaskListViewModel *taskList READ taskList CONSTANT)
    Q_PROPERTY(smartmate::viewmodel::TaskEditorViewModel *taskEditor READ taskEditor CONSTANT)
    Q_PROPERTY(smartmate::viewmodel::TaskDependencyViewModel *taskDependencies
                   READ taskDependencies CONSTANT)
    Q_PROPERTY(smartmate::viewmodel::TaskGraphViewModel *taskGraph READ taskGraph CONSTANT)
    Q_PROPERTY(smartmate::viewmodel::AppearanceSettingsViewModel *appearanceSettings
                   READ appearanceSettings CONSTANT)
    QML_NAMED_ELEMENT(AppViewModel)
    QML_UNCREATABLE("AppViewModel is created and owned by AppBootstrapper")

public:
    explicit AppViewModel(model::TaskService &taskService, QObject *parent = nullptr);
    AppViewModel(model::TaskService &taskService,
                 model::AppearanceSettingsService &appearanceService,
                 QObject *parent = nullptr);

    [[nodiscard]] QString applicationName() const;
    [[nodiscard]] TaskListViewModel *taskList() noexcept;
    [[nodiscard]] TaskEditorViewModel *taskEditor() noexcept;
    [[nodiscard]] TaskDependencyViewModel *taskDependencies() noexcept;
    [[nodiscard]] TaskGraphViewModel *taskGraph() noexcept;
    [[nodiscard]] AppearanceSettingsViewModel *appearanceSettings() noexcept;

private:
    // 四个子 ViewModel 共享同一个 TaskService，通过 Service 的状态变化保持同步，
    // 但彼此不直接调用，从而避免 ViewModel 之间形成隐式耦合。
    TaskListViewModel m_taskList;
    TaskEditorViewModel m_taskEditor;
    /// 采用 QObject 子对象所有权，使 QML 只能观察且无需 app 层单独登记生命周期。
    TaskDependencyViewModel *m_taskDependencies;
    TaskGraphViewModel m_taskGraph;
    AppearanceSettingsViewModel m_appearanceSettings;
};

} // namespace smartmate::viewmodel
