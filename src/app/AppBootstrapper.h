#pragma once

#include <memory>

class QString;

namespace smartmate::model {
class TaskService;
class TaskCategoryService;
class AppearanceSettingsService;

namespace persistence {
class SqliteTaskRepository;
class QSettingsAppearanceRepository;
}
} // namespace smartmate::model

namespace smartmate::viewmodel {
class AppViewModel;
}

namespace smartmate::view::widgets {
struct MainWindowDependencies;
}

namespace smartmate::app {

/// 应用层唯一的组合根：在这里选择具体持久化实现，并按
/// Repository -> Service -> ViewModel 的方向完成对象创建与依赖注入。
class AppBootstrapper final {
public:
    explicit AppBootstrapper(QString databasePath);
    ~AppBootstrapper();

    /// 将具体 ViewModel 向上转换为 Widgets 当前切片所需的抽象契约。
    [[nodiscard]] view::widgets::MainWindowDependencies widgetDependencies() noexcept;

private:
    // 成员按依赖顺序声明。C++ 会逆序析构，因此 ViewModel 和 Service
    // 持有的引用在各自生命周期内始终指向仍然存活的对象。
    std::unique_ptr<model::persistence::SqliteTaskRepository> m_taskRepository;
    std::unique_ptr<model::TaskService> m_taskService;
    std::unique_ptr<model::TaskCategoryService> m_taskCategoryService;
    std::unique_ptr<model::persistence::QSettingsAppearanceRepository>
        m_appearanceRepository;
    std::unique_ptr<model::AppearanceSettingsService> m_appearanceService;
    std::unique_ptr<viewmodel::AppViewModel> m_appViewModel;
};

} // namespace smartmate::app
