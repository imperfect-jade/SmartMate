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
    QApplication application(argc, argv);
    smartmate::app::configureApplicationIdentity();

    try {
        const auto runtime = smartmate::app::applicationRuntime();
        smartmate::app::AppBootstrapper bootstrapper{runtime.databasePath};
        smartmate::view::widgets::MainWindow window{
            bootstrapper.widgetDependencies()};
        window.show();

        if (runtime.smokeTest) {
            QTimer::singleShot(0, &application, &QCoreApplication::quit);
        }
        return application.exec();
    } catch (const std::exception &error) {
        qCritical("SmartMate startup failed: %s", error.what());
        return EXIT_FAILURE;
    }
}
