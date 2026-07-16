#include "AppearanceSettingsViewModel.h"
#include "fakes/FakeAppearanceSettingsRepository.h"
#include "services/AppearanceSettingsService.h"

#include <QSignalSpy>
#include <QTest>

using namespace smartmate::model;
using smartmate::tests::FakeAppearanceSettingsRepository;
using smartmate::viewmodel::AppearanceSettingsViewModel;

class AppearanceSettingsTest final : public QObject {
    Q_OBJECT
private slots:
    void loadsAndPersistsValidatedSelections();
    void mapsDocumentedFontScaleRatios();
    void saveFailureKeepsPreviousProjection();
    void resetRestoresDocumentedDefaults();
};

void AppearanceSettingsTest::loadsAndPersistsValidatedSelections()
{
    FakeAppearanceSettingsRepository repository;
    repository.settings = {AccentTheme::Blue, UiFontFamily::MicrosoftYaHeiUI,
                           UiFontScale::Large};
    AppearanceSettingsService service{repository};
    AppearanceSettingsViewModel viewModel{service};
    QCOMPARE(viewModel.accentThemeIndex(), 1);
    QCOMPARE(viewModel.fontFamilyName(), QStringLiteral("Microsoft YaHei UI"));
    QCOMPARE(viewModel.fontScale(), 1.25);
    QSignalSpy changed{&viewModel, &AppearanceSettingsViewModel::appearanceChanged};
    viewModel.setAccentThemeIndex(0);
    const AppearanceSettings expected{AccentTheme::Green,
                                      UiFontFamily::MicrosoftYaHeiUI,
                                      UiFontScale::Large};
    QCOMPARE(repository.settings, expected);
    QCOMPARE(viewModel.fontFamilyIndex(), 1);
    QCOMPARE(viewModel.fontScaleIndex(), 2);
    QCOMPARE(repository.saveCount, 1);
    QCOMPARE(changed.count(), 1);
}

void AppearanceSettingsTest::mapsDocumentedFontScaleRatios()
{
    const QList<QPair<UiFontScale, qreal>> cases{
        {UiFontScale::Small, 0.95},
        {UiFontScale::Standard, 1.10},
        {UiFontScale::Large, 1.25},
    };
    for (const auto &[scale, expected] : cases) {
        FakeAppearanceSettingsRepository repository;
        repository.settings.fontScale = scale;
        AppearanceSettingsService service{repository};
        AppearanceSettingsViewModel viewModel{service};
        QCOMPARE(viewModel.fontScale(), expected);
    }
}

void AppearanceSettingsTest::saveFailureKeepsPreviousProjection()
{
    FakeAppearanceSettingsRepository repository;
    AppearanceSettingsService service{repository};
    AppearanceSettingsViewModel viewModel{service};
    repository.failSave = true;
    QSignalSpy errors{&viewModel, &AppearanceSettingsViewModel::errorOccurred};
    viewModel.setFontScaleIndex(2);
    QCOMPARE(viewModel.fontScaleIndex(), 1);
    QCOMPARE(errors.count(), 1);
    QVERIFY(!viewModel.errorMessage().isEmpty());
}

void AppearanceSettingsTest::resetRestoresDocumentedDefaults()
{
    FakeAppearanceSettingsRepository repository;
    repository.settings = {AccentTheme::Blue, UiFontFamily::DengXian,
                           UiFontScale::Small};
    AppearanceSettingsService service{repository};
    AppearanceSettingsViewModel viewModel{service};
    viewModel.resetDefaults();
    QCOMPARE(viewModel.accentThemeIndex(), 0);
    QCOMPARE(viewModel.fontFamilyIndex(), 0);
    QCOMPARE(viewModel.fontScaleIndex(), 1);
}

QTEST_GUILESS_MAIN(AppearanceSettingsTest)
#include "tst_AppearanceSettings.moc"
