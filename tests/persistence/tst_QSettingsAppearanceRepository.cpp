#include "persistence/QSettingsAppearanceRepository.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

using namespace smartmate::model;
using smartmate::model::persistence::QSettingsAppearanceRepository;

class QSettingsAppearanceRepositoryTest final : public QObject {
    Q_OBJECT
private slots:
    void missingKeysUseDefaults();
    void valuesRoundTrip();
    void invalidLegacyValuesFallBackSafely();
};

void QSettingsAppearanceRepositoryTest::missingKeysUseDefaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettingsAppearanceRepository repository{directory.filePath("appearance.ini")};
    QCOMPARE(repository.load(), AppearanceSettings{});
}

void QSettingsAppearanceRepositoryTest::valuesRoundTrip()
{
    QTemporaryDir directory;
    const QString path = directory.filePath("appearance.ini");
    QSettingsAppearanceRepository repository{path};
    const AppearanceSettings expected{AccentTheme::Blue, UiFontFamily::DengXian,
                                      UiFontScale::Large};
    repository.save(expected);
    QCOMPARE(repository.load(), expected);
}

void QSettingsAppearanceRepositoryTest::invalidLegacyValuesFallBackSafely()
{
    QTemporaryDir directory;
    const QString path = directory.filePath("appearance.ini");
    {
        QSettings raw{path, QSettings::IniFormat};
        raw.setValue(QStringLiteral("appearance/accent"), QStringLiteral("purple"));
        raw.setValue(QStringLiteral("appearance/fontFamily"), QStringLiteral("unknown"));
        raw.setValue(QStringLiteral("appearance/fontScale"), QStringLiteral("huge"));
    }
    QSettingsAppearanceRepository repository{path};
    QCOMPARE(repository.load(), AppearanceSettings{});
}

QTEST_GUILESS_MAIN(QSettingsAppearanceRepositoryTest)
#include "tst_QSettingsAppearanceRepository.moc"
