#include "DesktopPetSettingsViewModel.h"
#include "fakes/FakeDesktopPetSettingsRepository.h"
#include "services/DesktopPetSettingsService.h"

#include <QSignalSpy>
#include <QTest>

using namespace smartmate;

class DesktopPetSettingsViewModelTest final : public QObject {
    Q_OBJECT

private slots:
    void initialProjectionAndCommandsArePersisted();
    void idempotentCommandsDoNotNotify();
    void saveFailurePreservesConfirmedProjection();
};

void DesktopPetSettingsViewModelTest::initialProjectionAndCommandsArePersisted()
{
    tests::FakeDesktopPetSettingsRepository repository;
    repository.settings.enabled = true;
    model::DesktopPetSettingsService service{repository};
    viewmodel::DesktopPetSettingsViewModel viewModel{service};
    QVERIFY(viewModel.enabled());

    QSignalSpy placementSpy{&viewModel,
        &viewmodel::DesktopPetSettingsContract::floatingPlacementChanged};
    viewModel.saveFloatingPlacement(QStringLiteral("Screen A"), 0.3, 0.7);
    QCOMPARE(placementSpy.count(), 1);
    QVERIFY(viewModel.hasFloatingPlacement());
    QCOMPARE(viewModel.floatingScreenName(), QStringLiteral("Screen A"));
    QCOMPARE(repository.saveCount, 1);

    QSignalSpy enabledSpy{&viewModel,
        &viewmodel::DesktopPetSettingsContract::enabledChanged};
    viewModel.setEnabled(false);
    QCOMPARE(enabledSpy.count(), 1);
    QVERIFY(!viewModel.enabled());
    QVERIFY(viewModel.hasFloatingPlacement());
}

void DesktopPetSettingsViewModelTest::idempotentCommandsDoNotNotify()
{
    tests::FakeDesktopPetSettingsRepository repository;
    model::DesktopPetSettingsService service{repository};
    viewmodel::DesktopPetSettingsViewModel viewModel{service};
    QSignalSpy enabledSpy{&viewModel,
        &viewmodel::DesktopPetSettingsContract::enabledChanged};
    viewModel.setEnabled(false);
    QCOMPARE(enabledSpy.count(), 0);
    QCOMPARE(repository.saveCount, 0);
}

void DesktopPetSettingsViewModelTest::saveFailurePreservesConfirmedProjection()
{
    tests::FakeDesktopPetSettingsRepository repository;
    model::DesktopPetSettingsService service{repository};
    viewmodel::DesktopPetSettingsViewModel viewModel{service};
    repository.failSave = true;
    QSignalSpy notificationSpy{&viewModel,
        &viewmodel::DesktopPetSettingsContract::notificationRaised};
    viewModel.setEnabled(true);
    QVERIFY(!viewModel.enabled());
    QCOMPARE(notificationSpy.count(), 1);
}

QTEST_MAIN(DesktopPetSettingsViewModelTest)
#include "tst_DesktopPetSettingsViewModel.moc"
