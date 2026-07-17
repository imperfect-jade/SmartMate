#include "view/widgets/MainWindow.h"
#include "view/widgets/settings/SettingsPage.h"
#include "view/widgets/theme/WidgetTheme.h"
#include "viewmodel/contracts/DesktopPetSettingsContract.h"

#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QFrame>
#include <QLabel>
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
using smartmate::viewmodel::DesktopPetSettingsContract;

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
        return scaleIndex == 0 ? 0.95 : scaleIndex == 2 ? 1.25 : 1.10;
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

class FakeDesktopPetSettingsContract final : public DesktopPetSettingsContract {
public:
    bool enabled() const noexcept override { return enabledValue; }
    bool hasFloatingPlacement() const noexcept override { return false; }
    QString floatingScreenName() const override { return {}; }
    qreal floatingXRatio() const noexcept override { return 0.0; }
    qreal floatingYRatio() const noexcept override { return 0.0; }
    void setEnabled(const bool value) override
    {
        enabledValue = value;
        ++setEnabledCount;
        emit enabledChanged();
    }
    void saveFloatingPlacement(const QString &, qreal, qreal) override {}

    bool enabledValue{false};
    int setEnabledCount{0};
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
    void previewRemainsReadableAtNarrowWidthAndLargeFont();
    void fontScaleOptionsApplyDistinctNonAccumulatingSizes();
    void accentSwitchPreservesAppliedChildFont();
    void desktopPetToggleUsesContractWithoutWriteBack();
    void desktopPetCardRemainsReadableAtNarrowWidthAndLargeFont();
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
    QCOMPARE(pages->count(), 5);
    QCOMPARE(pages->currentIndex(), 0);
    auto *statisticsNavigation =
        requiredChild<QPushButton>(window, "statisticsNavigationButton");
    auto *focusNavigation =
        requiredChild<QPushButton>(window, "focusNavigationButton");
    QCOMPARE(focusNavigation->accessibleName(), QStringLiteral("专注"));
    QTest::mouseClick(focusNavigation, Qt::LeftButton);
    QCOMPARE(pages->currentIndex(), 2);
    QCOMPARE(statisticsNavigation->accessibleName(), QStringLiteral("统计"));
    QTest::mouseClick(statisticsNavigation, Qt::LeftButton);
    QCOMPARE(pages->currentIndex(), 3);
    QTest::mouseClick(requiredChild<QPushButton>(window, "settingsNavigationButton"),
                      Qt::LeftButton);
    QCOMPARE(pages->currentIndex(), 4);

    window.show();
    window.resize(900, 620);
    QCoreApplication::processEvents();
    QCOMPARE(requiredChild<QFrame>(window, "navigationPanel")->width(), 64);
    QCOMPARE(statisticsNavigation->toolTip(), QStringLiteral("统计"));
    QCOMPARE(focusNavigation->toolTip(), QStringLiteral("专注"));
    window.resize(1180, 760);
    QCoreApplication::processEvents();
    QCOMPARE(requiredChild<QFrame>(window, "navigationPanel")->width(), 208);
    QCOMPARE(statisticsNavigation->toolTip(), QString{});
    QCOMPARE(pages->currentIndex(), 4);
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
    const qreal baselineSize = QApplication::font().pointSizeF();

    settings.replaceProjection(1, 0, 2);
    const WidgetTheme blueTheme = WidgetTheme::fromAccentIndex(1);
    QCOMPARE(window.palette().color(QPalette::Window), blueTheme.background);
    QCOMPARE(window.font().pointSizeF(), baselineSize * 1.25);

    settings.raiseNotification({UiSeverity::Error,
                                QStringLiteral("外观设置失败"),
                                QStringLiteral("无法保存外观设置")});
    QCOMPARE(window.statusBar()->currentMessage(),
             QStringLiteral("外观设置失败：无法保存外观设置"));
    QCOMPARE(window.statusBar()->palette().color(QPalette::WindowText),
             blueTheme.danger);
}

void SettingsWidgetsTest::previewRemainsReadableAtNarrowWidthAndLargeFont()
{
    FakeAppearanceSettingsContract settings;
    SettingsPage page{settings};
    QFont enlarged = page.font();
    enlarged.setPointSizeF(enlarged.pointSizeF() * 1.25);
    page.setFont(enlarged);
    page.resize(350, 620);
    page.show();
    QCoreApplication::processEvents();

    auto *preview = requiredChild<QFrame>(page, "previewCard");
    auto *title = requiredChild<QLabel>(page, "settingsPreviewTitle");
    auto *description = requiredChild<QLabel>(page, "settingsPreviewDescription");
    auto *status = requiredChild<QLabel>(page, "previewStatus");
    const auto inPreview = [preview](QWidget *child) {
        return QRect{child->mapTo(preview, QPoint{}), child->size()};
    };
    const QRect titleRect = inPreview(title);
    const QRect descriptionRect = inPreview(description);
    const QRect statusRect = inPreview(status);
    QVERIFY(preview->height() >= preview->minimumHeight());
    QVERIFY(preview->rect().contains(titleRect));
    QVERIFY(preview->rect().contains(descriptionRect));
    QVERIFY(preview->rect().contains(statusRect));
    QVERIFY(titleRect.bottom() < descriptionRect.top());
    QVERIFY(descriptionRect.bottom() < statusRect.top());

    QCOMPARE(settings.accentSetCount, 0);
    QCOMPARE(settings.familySetCount, 0);
    QCOMPARE(settings.scaleSetCount, 0);
}

void SettingsWidgetsTest::fontScaleOptionsApplyDistinctNonAccumulatingSizes()
{
    FakeAppearanceSettingsContract settings;
    MainWindow window{settings};
    const qreal baseline = QApplication::font().pointSizeF();
    settings.replaceProjection(0, 0, 0);
    QCOMPARE(window.font().pointSizeF(), baseline * 0.95);
    settings.replaceProjection(0, 0, 1);
    QCOMPARE(window.font().pointSizeF(), baseline * 1.10);
    settings.replaceProjection(0, 0, 2);
    QCOMPARE(window.font().pointSizeF(), baseline * 1.25);
    settings.replaceProjection(0, 0, 1);
    QCOMPARE(window.font().pointSizeF(), baseline * 1.10);
    QCOMPARE(settings.scaleSetCount, 0);
}

void SettingsWidgetsTest::accentSwitchPreservesAppliedChildFont()
{
    FakeAppearanceSettingsContract settings;
    settings.familyIndex = 2;
    settings.scaleIndex = 2;
    MainWindow window{settings};
    window.show();

    QTest::mouseClick(requiredChild<QPushButton>(window, "settingsNavigationButton"),
                      Qt::LeftButton);
    auto *largeScale = requiredChild<QPushButton>(window, "fontScaleButton_2");
    auto *reset = requiredChild<QPushButton>(window, "resetAppearanceButton");
    auto *blueAccent = requiredChild<QPushButton>(window, "accentThemeButton_1");
    QVERIFY(largeScale->isChecked());

    QTest::mouseClick(blueAccent, Qt::LeftButton);
    QCOMPARE(settings.accentIndex, 1);
    QCOMPARE(settings.familyIndex, 2);
    QCOMPARE(settings.scaleIndex, 2);
    QVERIFY(largeScale->isChecked());

    const QFont expected = smartmate::view::widgets::appearanceFont(
        QApplication::font(), settings);
    QCOMPARE(window.font(), expected);
    QCOMPARE(reset->font().family(), expected.family());
    QCOMPARE(reset->font().pointSizeF(), expected.pointSizeF());
}

void SettingsWidgetsTest::desktopPetToggleUsesContractWithoutWriteBack()
{
    FakeAppearanceSettingsContract appearance;
    FakeDesktopPetSettingsContract pet;
    pet.enabledValue = true;
    SettingsPage page{appearance, pet};
    auto *toggle = requiredChild<QCheckBox>(page, "desktopPetEnabledCheckBox");
    QVERIFY(toggle->isChecked());
    QCOMPARE(pet.setEnabledCount, 0);

    toggle->click();
    QVERIFY(!pet.enabledValue);
    QCOMPARE(pet.setEnabledCount, 1);

    pet.enabledValue = true;
    emit pet.enabledChanged();
    QVERIFY(toggle->isChecked());
    QCOMPARE(pet.setEnabledCount, 1);
}

void SettingsWidgetsTest::desktopPetCardRemainsReadableAtNarrowWidthAndLargeFont()
{
    FakeAppearanceSettingsContract appearance;
    FakeDesktopPetSettingsContract pet;
    SettingsPage page{appearance, pet};
    QFont enlarged = page.font();
    enlarged.setPointSizeF(enlarged.pointSizeF() * 1.25);
    page.setFont(enlarged);
    page.resize(350, 620);
    page.show();
    QCoreApplication::processEvents();

    auto *card = requiredChild<QFrame>(page, "desktopPetSettingsCard");
    auto *toggle = requiredChild<QCheckBox>(page, "desktopPetEnabledCheckBox");
    QLabel *description = nullptr;
    const auto secondaryLabels = card->findChildren<QLabel *>(
        QStringLiteral("secondaryText"));
    for (QLabel *label : secondaryLabels) {
        if (label->text().contains(QStringLiteral("普通窗口"))) {
            description = label;
            break;
        }
    }
    QVERIFY(description != nullptr);
    const QRect descriptionRect{description->mapTo(card, QPoint{}),
                                description->size()};
    const QRect toggleRect{toggle->mapTo(card, QPoint{}), toggle->size()};
    QVERIFY(card->width() >= page.width() - 110);
    QVERIFY(card->rect().contains(descriptionRect));
    QVERIFY(card->rect().contains(toggleRect));
    QVERIFY(descriptionRect.bottom() < toggleRect.top());
}

QTEST_MAIN(SettingsWidgetsTest)
#include "tst_SettingsWidgets.moc"
