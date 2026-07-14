#include "ApplicationRuntime.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

#include <stdexcept>

namespace smartmate::app {

void configureApplicationIdentity()
{
    // 必须在创建 QSettings Repository 和查询标准目录前设置，确保路径长期稳定。
    QCoreApplication::setOrganizationName(QStringLiteral("imperfect-jade"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("github.com/imperfect-jade"));
    QCoreApplication::setApplicationName(QStringLiteral("SmartMate"));
}

ApplicationRuntime applicationRuntime()
{
    // 冒烟测试使用内存数据库，既验证正式对象图，又不接触用户真实任务数据。
    const bool smokeTest = QCoreApplication::arguments().contains(
        QStringLiteral("--smoke-test"));
    if (smokeTest) {
        return {true, QStringLiteral(":memory:")};
    }

    // 正式数据库统一放入 Qt 为当前用户解析的本地应用数据目录。
    const QString dataDirectory =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (dataDirectory.isEmpty() || !QDir{}.mkpath(dataDirectory)) {
        throw std::runtime_error("Unable to create the SmartMate application data directory");
    }
    return {false, QDir{dataDirectory}.filePath(QStringLiteral("smartmate.db"))};
}

} // namespace smartmate::app
