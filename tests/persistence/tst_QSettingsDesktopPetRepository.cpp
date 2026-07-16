#include "persistence/QSettingsDesktopPetRepository.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

using namespace smartmate;

class QSettingsDesktopPetRepositoryTest final : public QObject {
    Q_OBJECT

private slots:
    void missingKeysUseDisabledDefaults();
    void snapshotRoundTripsAndDisabledRetainsPlacement();
    void incompletePlacementIsIgnored();
};

void QSettingsDesktopPetRepositoryTest::missingKeysUseDisabledDefaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    model::persistence::QSettingsDesktopPetRepository repository{
        directory.filePath(QStringLiteral("pet.ini"))};
    const auto settings = repository.load();
    QVERIFY(!settings.enabled);
    QVERIFY(!settings.placement.has_value());
}

void QSettingsDesktopPetRepositoryTest::snapshotRoundTripsAndDisabledRetainsPlacement()
{
    QTemporaryDir directory;
    const QString file = directory.filePath(QStringLiteral("pet.ini"));
    model::persistence::QSettingsDesktopPetRepository repository{file};
    model::DesktopPetSettings settings{
        true, model::DesktopPetPlacement{QStringLiteral("Screen A"), 0.2, 0.8}};
    repository.save(settings);
    QCOMPARE(repository.load(), settings);
    settings.enabled = false;
    repository.save(settings);
    QCOMPARE(repository.load(), settings);
}

void QSettingsDesktopPetRepositoryTest::incompletePlacementIsIgnored()
{
    QTemporaryDir directory;
    const QString file = directory.filePath(QStringLiteral("pet.ini"));
    QSettings raw{file, QSettings::IniFormat};
    raw.setValue(QStringLiteral("desktopPet/enabled"), true);
    raw.setValue(QStringLiteral("desktopPet/placement/screenName"),
                 QStringLiteral("Screen A"));
    raw.sync();
    model::persistence::QSettingsDesktopPetRepository repository{file};
    const auto loaded = repository.load();
    QVERIFY(loaded.enabled);
    QVERIFY(!loaded.placement.has_value());
}

QTEST_MAIN(QSettingsDesktopPetRepositoryTest)
#include "tst_QSettingsDesktopPetRepository.moc"
