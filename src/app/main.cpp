#include "AppBootstrapper.h"
#include "ApplicationRuntime.h"
#include "view/widgets/MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QLoggingCategory>
#include <QTimer>

#include <cstdlib>
#include <exception>

int main(int argc, char *argv[])
{
    // QApplication 必须先于任何 Widget 创建，并负责整个 GUI 事件循环。
    QApplication application(argc, argv);
    // 应用标识会参与 QSettings 和用户数据目录解析，必须在组合根构造前设置。
    smartmate::app::configureApplicationIdentity();

    try {
        // 入口只负责进程配置与对象装配；业务规则和展示投影留在各自层内。
        const auto runtime = smartmate::app::applicationRuntime();
        smartmate::app::AppBootstrapper bootstrapper{runtime.databasePath};
        // MainWindow 持有非拥有 Contract 引用，因此 bootstrapper 必须比窗口后析构。
        smartmate::view::widgets::MainWindow window{
            bootstrapper.widgetDependencies()};
        window.show();

        if (runtime.smokeTest) {
            // 进入一次事件循环后退出，用于验证正式入口能够完成离屏启动。
            QTimer::singleShot(0, &application, &QCoreApplication::quit);
        }

        return application.exec();
    } catch (const std::exception &error) {
        // 启动阶段无法建立完整对象图时立即失败，避免展示不可用的半初始化窗口。
        qCritical("SmartMate startup failed: %s", error.what());
        return EXIT_FAILURE;
    }
}
