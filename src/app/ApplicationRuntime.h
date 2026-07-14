#pragma once

#include <QString>

namespace smartmate::app {

/// 进程启动阶段解析出的运行参数，不承载业务状态或界面状态。
struct ApplicationRuntime {
    /// 为 true 时使用隔离数据源，并在窗口完成一次事件循环后自动退出。
    bool smokeTest{false};
    /// SQLite 数据库路径；冒烟测试使用特殊值“:memory:”。
    QString databasePath;
};

/// 设置 Qt 应用标识；QSettings 和系统数据目录会使用这些稳定名称。
void configureApplicationIdentity();

/// 解析命令行并选择内存数据库或用户本地数据库。
/// 无法确定或创建正式数据目录时抛出异常，禁止静默退回临时位置。
[[nodiscard]] ApplicationRuntime applicationRuntime();

} // namespace smartmate::app
