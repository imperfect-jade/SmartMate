#include "view/widgets/MainWindow.h"
#include "view/widgets/settings/SettingsPage.h"
#include "view/widgets/theme/WidgetTheme.h"

#include <QComboBox>
#include <QFrame>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTest>

using smartmate::common::UiNotification;
using smartmate::common::UiSeverity;
using smartmate::view::widgets::MainWindow;
using smartmate::view::widgets::MainWindowDependencies;
using smartmate::view::widgets::SettingsPage;
using smartmate::view::widgets::WidgetTheme;
using smartmate::viewmodel::AppearanceSettingsContract;

namespace {

class FakeAppearanceSettingsContract final : public AppearanceSettingsContract {
public:
    FakeAppearanceSettingsContract()
        : AppearanceSettingsContract(nullptr)
    {
    }

    int accentThemeIndex() const noexcept override { return accentIndex; }
    int fontFamilyIndex() const noexcept override { return familyIndex; }
    int fontScaleIndex() const noexcept override { return scaleIndex; }
    QStringList accentThemeOptions() const override
    {
        return {QStringLiteral("青绿清新"), QStringLiteral("清蓝专注")};
    }
    QStringList fontFamilyOptions() const override
    {
        return {QStringLiteral("系统默认"), QStringLiteral("Microsoft YaHei UI"),
                QStringLiteral("等线")};
    }
    QStringList fontScaleOptions() const override
    {
        return {QStringLiteral("较小"), QStringLiteral("标准"), QStringLiteral("较大")};
    }
    QString fontFamilyName() const override
    {
        if (familyIndex == 1) return QStringLiteral("Microsoft YaHei UI");
        if (familyIndex == 2) return QStringLiteral("DengXian");
        return {};
    }
    qreal fontScale() const noexcept override
    {
        return scaleIndex == 0 ? 0.9 : scaleIndex == 2 ? 1.1 : 1.0;
    }

    void setAccentThemeIndex(const int index) override
    {
        accentIndex = index;
        ++accentSetCount;
        emit appearanceChanged();
    }
    void setFontFamilyIndex(const int index) override
    {
        familyIndex = index;
        ++familySetCount;
        emit appearanceChanged();
    }
    void setFontScaleIndex(const int index) override
    {
        scaleIndex = index;
        ++scaleSetCount;
        emit appearanceChanged();
    }
    void resetDefaults() override
    {
        accentIndex = 0;
        familyIndex = 0;
        scaleIndex = 1;
        ++resetCount;
        emit appearanceChanged();
    }

    void replaceProjection(const int accent, const int family, const int scale)
    {
        accentIndex = accent;
        familyIndex = family;
        scaleIndex = scale;
        emit appearanceChanged();
    }

    void raiseNotification(const UiNotification &notification)
    {
        emit notificationRaised(notification);
    }

    int accentIndex{0};
    int familyIndex{0};
    int scaleIndex{1};
    int accentSetCount{0};
    int familySetCount{0};
    int scaleSetCount{0};
    int resetCount{0};
};

template<typename Widget>
Widget *requiredChild(QObject &parent, const char *objectName)
{
    Widget *result = parent.findChild<Widget *>(QString::fromLatin1(objectName));
    Q_ASSERT(result != nullptr);
    return result;
}

} // namespace

class SettingsWidgetsTest final : public QObject {
    Q_OBJECT

private slots:
    void initialStateAndNavigationAreSynchronized();
    void userEventsInvokeStronglyTypedCommands();
    void contractNotificationUpdatesControlsWithoutWriteBack();
    void themeAndUiNotificationArePresentedByMainWindow();
};

void SettingsWidgetsTest::initialStateAndNavigationAreSynchronized()
{
    FakeAppearanceSettingsContract settings;
    settings.accentIndex = 1;
    settings.familyIndex = 2;
    settings.scaleIndex = 0;
    MainWindow window{settings};

    QVERIFY(requiredChild<QPushButton>(window, "accentThemeButton_1")->isChecked());
    QCOMPARE(requiredChild<QComboBox>(window, "fontFamilyComboBox")->currentIndex(), 2);
    QVERIFY(requiredChild<QPushButton>(window, "fontScaleButton_0")->isChecked());

    auto *pages = window.findChild<QStackedWidget *>();
    QVERIFY(pages != nullptr);
    QCOMPARE(pages->currentIndex(), 0);
    QTest::mouseClick(requiredChild<QPushButton>(window, "settingsNavigationButton"),
                      Qt::LeftButton);
    QCOMPARE(pages->currentIndex(), 2);

    window.show();
    window.resize(900, 620);
    QCoreApplication::processEvents();
    QCOMPARE(requiredChild<QFrame>(window, "navigationPanel")->width(), 64);
    window.resize(1180, 760);
    QCoreApplication::processEvents();
    QCOMPARE(requiredChild<QFrame>(window, "navigationPanel")->width(), 208);
    QCOMPARE(pages->currentIndex(), 2);
    QVERIFY(requiredChild<QPushButton>(window, "settingsNavigationButton")->isChecked());
}

void SettingsWidgetsTest::userEventsInvokeStronglyTypedCommands()
{
    FakeAppearanceSettingsContract settings;
    SettingsPage page{settings};
    page.show();

    QTest::mouseClick(requiredChild<QPushButton>(page, "accentThemeButton_1"),
                      Qt::LeftButton);
    QCOMPARE(settings.accentIndex, 1);
    QCOMPARE(settings.accentSetCount, 1);

    auto *family = requiredChild<QComboBox>(page, "fontFamilyComboBox");
    family->setCurrentIndex(2);
    emit family->activated(2);
    QCOMPARE(settings.familyIndex, 2);
    QCOMPARE(settings.familySetCount, 1);

    QTest::mouseClick(requiredChild<QPushButton>(page, "fontScaleButton_2"),
                      Qt::LeftButton);
    QCOMPARE(settings.scaleIndex, 2);
    QCOMPARE(settings.scaleSetCount, 1);

    QTest::mouseClick(requiredChild<QPushButton>(page, "resetAppearanceButton"),
                      Qt::LeftButton);
    QCOMPARE(settings.resetCount, 1);
}

void SettingsWidgetsTest::contractNotificationUpdatesControlsWithoutWriteBack()
{
    FakeAppearanceSettingsContract settings;
    SettingsPage page{settings};
    settings.replaceProjection(1, 2, 0);

    QVERIFY(requiredChild<QPushButton>(page, "accentThemeButton_1")->isChecked());
    QCOMPARE(requiredChild<QComboBox>(page, "fontFamilyComboBox")->currentIndex(), 2);
    QVERIFY(requiredChild<QPushButton>(page, "fontScaleButton_0")->isChecked());
    QCOMPARE(settings.accentSetCount, 0);
    QCOMPARE(settings.familySetCount, 0);
    QCOMPARE(settings.scaleSetCount, 0);
}

void SettingsWidgetsTest::themeAndUiNotificationArePresentedByMainWindow()
{
    FakeAppearanceSettingsContract settings;
    MainWindow window{settings};
    const qreal baselineSize = window.font().pointSizeF();

    settings.replaceProjection(1, 0, 2);
    const WidgetTheme blueTheme = WidgetTheme::fromAccentIndex(1);
    QCOMPARE(window.palette().color(QPalette::Window), blueTheme.background);
    QCOMPARE(window.font().pointSizeF(), baselineSize * 1.1);

    settings.raiseNotification({UiSeverity::Error,
                                QStringLiteral("外观设置失败"),
                                QStringLiteral("无法保存外观设置")});
    QCOMPARE(window.statusBar()->currentMessage(),
             QStringLiteral("外观设置失败：无法保存外观设置"));
    QCOMPARE(window.statusBar()->palette().color(QPalette::WindowText),
             blueTheme.danger);
}

QTEST_MAIN(SettingsWidgetsTest)
#include "tst_SettingsWidgets.moc"
