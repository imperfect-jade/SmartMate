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
    QCOMPARE(viewModel.fontScale(), 1.1);
    QSignalSpy changed{&viewModel, &AppearanceSettingsViewModel::appearanceChanged};
    viewModel.setAccentThemeIndex(0);
    QCOMPARE(repository.settings.accentTheme, AccentTheme::Green);
    QCOMPARE(repository.saveCount, 1);
    QCOMPARE(changed.count(), 1);
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
