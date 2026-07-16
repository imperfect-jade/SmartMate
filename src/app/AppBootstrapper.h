#pragma once

#include <memory>

class QString;

namespace smartmate::model {
class TaskService;
class TaskCategoryService;
class StatisticsService;
class AppearanceSettingsService;
class DesktopPetSettingsService;

namespace persistence {
class SqliteTaskRepository;
class QSettingsAppearanceRepository;
class QSettingsDesktopPetRepository;
}
} // namespace smartmate::model

namespace smartmate::viewmodel {
class AppViewModel;
class DesktopPetSettingsViewModel;
}

namespace smartmate::view::widgets {
struct MainWindowDependencies;
}

namespace smartmate::app {

/// 应用层唯一的组合根：在这里选择具体持久化实现，并按
/// Repository -> Service -> ViewModel 的方向完成对象创建与依赖注入。
class AppBootstrapper final {
public:
    /// 使用指定 SQLite 路径构造完整应用对象图；初始数据源不可读时抛出异常。
    explicit AppBootstrapper(QString databasePath);
    ~AppBootstrapper();

    /// 将具体 ViewModel 向上转换为主窗口所需的抽象 Contract 引用。
    /// 返回值不拥有对象，必须在本组合根和 AppViewModel 存活期间使用。
    [[nodiscard]] view::widgets::MainWindowDependencies widgetDependencies() noexcept;

private:
    // 成员按依赖顺序声明。C++ 会逆序析构，因此 ViewModel 和 Service
    // 持有的引用在各自生命周期内始终指向仍然存活的对象。
    /// 任务 SQLite 适配器，同时实现任务、依赖、批量命令和类别 Repository 端口。
    std::unique_ptr<model::persistence::SqliteTaskRepository> m_taskRepository;
    /// 任务业务用例入口；通过抽象 Repository 端口访问同一个 SQLite 适配器。
    std::unique_ptr<model::TaskService> m_taskService;
    /// 类别业务用例入口，与任务服务共享类别 Repository 数据源。
    std::unique_ptr<model::TaskCategoryService> m_taskCategoryService;
    /// 统计聚合入口，只通过任务事件、任务和依赖 Repository 读取事实数据。
    std::unique_ptr<model::StatisticsService> m_statisticsService;
    /// 外观偏好的 QSettings 持久化适配器。
    std::unique_ptr<model::persistence::QSettingsAppearanceRepository>
        m_appearanceRepository;
    /// 外观设置的校验和保存入口。
    std::unique_ptr<model::AppearanceSettingsService> m_appearanceService;
    /// 桌宠轻量设置使用独立 QSettings Repository 与 Service。
    std::unique_ptr<model::persistence::QSettingsDesktopPetRepository>
        m_desktopPetRepository;
    std::unique_ptr<model::DesktopPetSettingsService> m_desktopPetService;
    /// 拥有全部具体 ViewModel，并向 View 暴露对应的抽象 Contract。
    std::unique_ptr<viewmodel::AppViewModel> m_appViewModel;
    /// 桌宠设置投影独立于任务 ViewModel 聚合，但由同一组合根管理生命周期。
    std::unique_ptr<viewmodel::DesktopPetSettingsViewModel>
        m_desktopPetViewModel;
};

} // namespace smartmate::app
