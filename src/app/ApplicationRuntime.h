#pragma once

#include <QString>

namespace smartmate::app {

struct ApplicationRuntime {
    bool smokeTest{false};
    QString databasePath;
};

/// 设置正式 Widgets 入口与删除门禁前 QML 基线共享的组织与应用标识。
void configureApplicationIdentity();

/// 根据当前命令行选择内存或用户数据目录数据库；目录失败时抛出异常。
[[nodiscard]] ApplicationRuntime applicationRuntime();

} // namespace smartmate::app
