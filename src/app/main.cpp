#include "AppBootstrapper.h"

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include <QTimer>

#include <cstdlib>
#include <exception>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("imperfect-jade"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("github.com/imperfect-jade"));
    QCoreApplication::setApplicationName(QStringLiteral("SmartMate"));

    // 离屏启动测试使用内存数据库，正常运行才写入用户的本地应用数据目录。
    const bool smokeTest = QCoreApplication::arguments().contains(QStringLiteral("--smoke-test"));
    QString databasePath = QStringLiteral(":memory:");
    if (!smokeTest) {
        const QString dataDirectory =
            QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (dataDirectory.isEmpty() || !QDir{}.mkpath(dataDirectory)) {
            qCritical("Unable to create the SmartMate application data directory");
            return EXIT_FAILURE;
        }
        databasePath = QDir{dataDirectory}.filePath(QStringLiteral("smartmate.db"));
    }

    try {
        // bootstrapper 先于 QML 引擎构造、后于引擎析构，保证所有 QML 绑定
        // 都在其引用的 ViewModel 被释放前结束。
        smartmate::app::AppBootstrapper bootstrapper{databasePath};
        QQmlApplicationEngine engine;
        bootstrapper.configure(engine);
        engine.loadFromModule(QStringLiteral("SmartMate.View"), QStringLiteral("Main"));

        if (engine.rootObjects().isEmpty()) {
            return EXIT_FAILURE;
        }

        if (smokeTest) {
            QTimer::singleShot(0, &application, &QCoreApplication::quit);
        }

        return application.exec();
    } catch (const std::exception &error) {
        qCritical("SmartMate startup failed: %s", error.what());
        return EXIT_FAILURE;
    }
}
