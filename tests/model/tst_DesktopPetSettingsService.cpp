#include "fakes/FakeDesktopPetSettingsRepository.h"
#include "services/DesktopPetSettingsService.h"

#include <QTest>

#include <limits>

using namespace smartmate;

class DesktopPetSettingsServiceTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultsAreDisabled();
    void validSnapshotIsSaved();
    void invalidPlacementIsRejected();
    void corruptLoadedPlacementIsClearedWithoutDisabling();
    void repositoryFailuresAreMapped();
};

void DesktopPetSettingsServiceTest::defaultsAreDisabled()
{
    tests::FakeDesktopPetSettingsRepository repository;
    model::DesktopPetSettingsService service{repository};
    const auto result = service.load();
    QVERIFY(result.ok());
    QVERIFY(!result.value->enabled);
    QVERIFY(!result.value->placement.has_value());
}

void DesktopPetSettingsServiceTest::validSnapshotIsSaved()
{
    tests::FakeDesktopPetSettingsRepository repository;
    model::DesktopPetSettingsService service{repository};
    const model::DesktopPetSettings settings{
        true, model::DesktopPetPlacement{QStringLiteral("DISPLAY1"), 0.25, 0.75}};
    const auto result = service.save(settings);
    QVERIFY(result.ok());
    QCOMPARE(repository.settings, settings);
    QCOMPARE(repository.saveCount, 1);
}

void DesktopPetSettingsServiceTest::invalidPlacementIsRejected()
{
    tests::FakeDesktopPetSettingsRepository repository;
    model::DesktopPetSettingsService service{repository};
    model::DesktopPetSettings invalid{
        true, model::DesktopPetPlacement{{},
            std::numeric_limits<double>::quiet_NaN(), 1.2}};
    const auto result = service.save(invalid);
    QVERIFY(!result.ok());
    QCOMPARE(result.error, model::DesktopPetSettingsError::InvalidValue);
    QCOMPARE(repository.saveCount, 0);
}

void DesktopPetSettingsServiceTest::corruptLoadedPlacementIsClearedWithoutDisabling()
{
    tests::FakeDesktopPetSettingsRepository repository;
    repository.settings = {true,
        model::DesktopPetPlacement{QStringLiteral("DISPLAY1"), -0.1, 0.5}};
    model::DesktopPetSettingsService service{repository};
    const auto result = service.load();
    QVERIFY(result.ok());
    QVERIFY(result.value->enabled);
    QVERIFY(!result.value->placement.has_value());
}

void DesktopPetSettingsServiceTest::repositoryFailuresAreMapped()
{
    tests::FakeDesktopPetSettingsRepository repository;
    repository.failLoad = true;
    model::DesktopPetSettingsService service{repository};
    QCOMPARE(service.load().error,
             model::DesktopPetSettingsError::PersistenceFailure);
    repository.failLoad = false;
    repository.failSave = true;
    QCOMPARE(service.save({}).error,
             model::DesktopPetSettingsError::PersistenceFailure);
}

QTEST_MAIN(DesktopPetSettingsServiceTest)
#include "tst_DesktopPetSettingsService.moc"
