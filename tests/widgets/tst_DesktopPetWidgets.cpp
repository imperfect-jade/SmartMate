#include "view/widgets/pet/DesktopPetGeometry.h"
#include "view/widgets/pet/DesktopPetSpriteWidget.h"

#include <QTest>

using namespace smartmate::view::widgets::pet;

class DesktopPetWidgetsTest final : public QObject {
    Q_OBJECT

private slots:
    void normalizedGeometryRoundTripsAndClamps();
    void popupPrefersLeftThenFallsBackRight();
    void spritesheetLoadsAndTimerStopsWhenHidden();
};

void DesktopPetWidgetsTest::normalizedGeometryRoundTripsAndClamps()
{
    const QRect available{-1920, 0, 1920, 1080};
    const QSize pet{96, 104};
    const QPoint position = positionFromRatios(available, pet, 0.25, 0.75);
    const QPointF ratios = ratiosFromPosition(available, pet, position);
    QVERIFY(qAbs(ratios.x() - 0.25) < 0.001);
    QVERIFY(qAbs(ratios.y() - 0.75) < 0.001);
    QCOMPARE(clampTopLeft({500, -500}, pet, available),
             QPoint(-96, 0));
}

void DesktopPetWidgetsTest::popupPrefersLeftThenFallsBackRight()
{
    const QRect screen{0, 0, 1200, 800};
    QCOMPARE(taskPopupPosition(screen, {900, 500, 96, 104}, {340, 180}),
             QPoint(552, 461));
    const QPoint fallback = taskPopupPosition(
        screen, {10, 500, 96, 104}, {340, 180});
    QCOMPARE(fallback.x(), 114);
    QCOMPARE(fallback.y(), 461);
}

void DesktopPetWidgetsTest::spritesheetLoadsAndTimerStopsWhenHidden()
{
    DesktopPetSpriteWidget sprite{DesktopPetSpriteWidget::Animation::Idle};
    QVERIFY(sprite.assetReady());
    QCOMPARE(sprite.size(), QSize(96, 104));
    sprite.show();
    QTRY_VERIFY(sprite.animationRunning());
    sprite.hide();
    QVERIFY(!sprite.animationRunning());
}

QTEST_MAIN(DesktopPetWidgetsTest)
#include "tst_DesktopPetWidgets.moc"
