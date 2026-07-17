#include "AppViewModel.h"
#include "DesktopPetSettingsViewModel.h"
#include "fakes/FakeAppearanceSettingsRepository.h"
#include "fakes/FakeDesktopPetSettingsRepository.h"
#include "persistence/SqliteTaskRepository.h"
#include "services/AppearanceSettingsService.h"
#include "services/DesktopPetSettingsService.h"
#include "services/StatisticsService.h"
#include "services/FocusService.h"
#include "services/TaskCategoryService.h"
#include "services/TaskService.h"
#include "view/widgets/MainWindow.h"
#include "view/widgets/pet/DesktopPetTaskPopup.h"

#include <QApplication>
#include <QColor>
#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QTest>

using namespace smartmate;

namespace {
class FakeFocusContract final : public viewmodel::TaskFocusContract {
public:
    FocusState focusState() const noexcept override { return state; }
    QString focusTaskId() const override { return taskId; }
    QString focusTitle() const override { return title; }
    QString focusDescription() const override { return {}; }
    QString focusStatusText() const override { return QStringLiteral("进行中"); }
    QString focusPriorityText() const override { return QStringLiteral("普通"); }
    QString focusDeadlineText() const override { return QStringLiteral("无截止时间"); }
    int focusEstimatedMinutes() const noexcept override { return 25; }
    QString focusReasonText() const override { return QStringLiteral("当前最推荐"); }
    bool focusOverdue() const noexcept override { return false; }
    bool focusCanStart() const noexcept override { return canStart; }
    bool focusCanComplete() const noexcept override { return canComplete; }
    QString focusCategoryName() const override { return {}; }
    QString focusCategoryAccent() const override { return {}; }
    bool focusHasCategory() const noexcept override { return false; }

    void replace(FocusState value)
    {
        state = value;
        canStart = value == FocusState::Suggested;
        canComplete = value == FocusState::InProgress;
        emit focusTaskChanged();
    }

    FocusState state{FocusState::NoTasks};
    QString taskId{QStringLiteral("stable-task-id")};
    QString title{QStringLiteral("测试任务")};
    bool canStart{false};
    bool canComplete{false};
};

QWidget *topLevelByName(const QString &name)
{
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (widget->objectName() == name) {
            return widget;
        }
    }
    return nullptr;
}
}

class WidgetsDesktopPetFlowTest final : public QObject {
    Q_OBJECT

private slots:
    void popupStartsAndCompletesStableFocusedTask();
    void popupRendersEveryFocusState();
    void mainWindowStateSelectsExactlyOnePetView();
};

void WidgetsDesktopPetFlowTest::popupStartsAndCompletesStableFocusedTask()
{
    model::persistence::SqliteTaskRepository repository{QStringLiteral(":memory:")};
    model::TaskService service{repository, repository, repository,
                               repository, repository, repository};
    viewmodel::AppViewModel app{service};
    view::widgets::pet::DesktopPetTaskPopup popup{
        *app.taskFocus(), *app.taskList()};

    auto *title = popup.findChild<QLabel *>(
        QStringLiteral("desktopPetPopupTitle"));
    auto *start = popup.findChild<QPushButton *>(
        QStringLiteral("desktopPetStartButton"));
    auto *complete = popup.findChild<QPushButton *>(
        QStringLiteral("desktopPetCompleteButton"));
    QVERIFY(title && start && complete);
    QCOMPARE(title->text(), QStringLiteral("暂时没有待处理任务"));

    model::TaskDraft draft;
    draft.title = QStringLiteral("由桌宠推进的任务");
    draft.estimatedMinutes = 25;
    QVERIFY(service.createTask(draft).ok());
    QTRY_COMPARE(app.taskFocus()->focusState(),
                 viewmodel::TaskFocusContract::FocusState::Suggested);
    popup.showNextTo({500, 500, 96, 104});
    QTRY_VERIFY(start->isVisible());
    const QImage popupImage = popup.grab().toImage().convertToFormat(
        QImage::Format_ARGB32);
    const QColor expectedBackground{QStringLiteral("#fffdf8")};
    qsizetype opaqueBackgroundPixels = 0;
    for (int y = 0; y < popupImage.height(); ++y) {
        for (int x = 0; x < popupImage.width(); ++x) {
            const QColor pixel = popupImage.pixelColor(x, y);
            if (pixel.alpha() > 240
                && qAbs(pixel.red() - expectedBackground.red()) <= 3
                && qAbs(pixel.green() - expectedBackground.green()) <= 3
                && qAbs(pixel.blue() - expectedBackground.blue()) <= 3) {
                ++opaqueBackgroundPixels;
            }
        }
    }
    QVERIFY(opaqueBackgroundPixels > popupImage.width() * popupImage.height() / 4);
    QCOMPARE(title->text(), draft.title);
    const QString stableId = app.taskFocus()->focusTaskId();
    QVERIFY(!stableId.isEmpty());

    QTest::mouseClick(start, Qt::LeftButton);
    QTRY_COMPARE(app.taskFocus()->focusState(),
                 viewmodel::TaskFocusContract::FocusState::InProgress);
    QCOMPARE(app.taskFocus()->focusTaskId(), stableId);
    QTRY_VERIFY(complete->isVisible());

    QTest::mouseClick(complete, Qt::LeftButton);
    QTRY_COMPARE(app.taskFocus()->focusState(),
                 viewmodel::TaskFocusContract::FocusState::NoTasks);
    QVERIFY(app.taskFocus()->focusTaskId().isEmpty());
}

void WidgetsDesktopPetFlowTest::popupRendersEveryFocusState()
{
    model::persistence::SqliteTaskRepository repository{QStringLiteral(":memory:")};
    model::TaskService service{repository, repository, repository,
                               repository, repository, repository};
    viewmodel::AppViewModel app{service};
    FakeFocusContract focus;
    view::widgets::pet::DesktopPetTaskPopup popup{focus, *app.taskList()};
    auto *title = popup.findChild<QLabel *>(
        QStringLiteral("desktopPetPopupTitle"));
    auto *start = popup.findChild<QPushButton *>(
        QStringLiteral("desktopPetStartButton"));
    auto *complete = popup.findChild<QPushButton *>(
        QStringLiteral("desktopPetCompleteButton"));

    QCOMPARE(title->text(), QStringLiteral("暂时没有待处理任务"));
    QVERIFY(start->isHidden());
    QVERIFY(complete->isHidden());

    focus.replace(viewmodel::TaskFocusContract::FocusState::AllBlocked);
    QCOMPARE(title->text(), QStringLiteral("当前任务都被前置依赖阻塞"));
    QVERIFY(start->isHidden());
    QVERIFY(complete->isHidden());

    focus.replace(viewmodel::TaskFocusContract::FocusState::Suggested);
    QCOMPARE(title->text(), focus.title);
    QVERIFY(!start->isHidden());
    QVERIFY(start->isEnabled());
    QVERIFY(complete->isHidden());
    QTest::mouseClick(start, Qt::LeftButton);
    auto *error = popup.findChild<QLabel *>(
        QStringLiteral("desktopPetPopupError"));
    QVERIFY(error != nullptr);
    QTRY_VERIFY(!error->isHidden());
    QVERIFY(!error->text().isEmpty());

    focus.replace(viewmodel::TaskFocusContract::FocusState::InProgress);
    QVERIFY(start->isHidden());
    QVERIFY(!complete->isHidden());
    QVERIFY(complete->isEnabled());
}

void WidgetsDesktopPetFlowTest::mainWindowStateSelectsExactlyOnePetView()
{
    model::persistence::SqliteTaskRepository repository{QStringLiteral(":memory:")};
    model::FocusService focusService{repository, repository, repository, repository};
    QVERIFY(focusService.initialize().ok());
    model::TaskService taskService{repository, repository, repository,
                                   repository, repository, repository, &repository};
    model::TaskCategoryService categoryService{repository};
    model::StatisticsService statisticsService{repository, repository, repository};
    tests::FakeAppearanceSettingsRepository appearanceRepository;
    model::AppearanceSettingsService appearanceService{appearanceRepository};
    tests::FakeDesktopPetSettingsRepository petRepository;
    petRepository.settings.enabled = true;
    model::DesktopPetSettingsService petService{petRepository};
    viewmodel::DesktopPetSettingsViewModel petSettings{petService};
    viewmodel::AppViewModel app{taskService, categoryService, statisticsService,
                                focusService, appearanceService};
    view::widgets::MainWindow window{{*app.appearanceSettings(), petSettings,
                                      *app.taskList(), *app.taskFocus(),
                                      *app.taskDetails(), *app.taskEditor(),
                                      *app.taskCategories(), *app.taskDependencies(),
                                      *app.taskGraph(), *app.focus(),
                                      *app.statistics()}};
    window.showNormal();
    QTRY_VERIFY(topLevelByName(QStringLiteral("attachedDesktopPetWindow"))
                    ->isVisible());
    QVERIFY(!topLevelByName(QStringLiteral("floatingDesktopPetWindow"))
                 ->isVisible());

    window.setWindowState(Qt::WindowMaximized);
    QTRY_VERIFY(!topLevelByName(QStringLiteral("attachedDesktopPetWindow"))
                     ->isVisible());
    QVERIFY(!topLevelByName(QStringLiteral("floatingDesktopPetWindow"))
                 ->isVisible());

    window.setWindowState(Qt::WindowMinimized);
    QTRY_VERIFY(topLevelByName(QStringLiteral("floatingDesktopPetWindow"))
                    ->isVisible());
    QVERIFY(!topLevelByName(QStringLiteral("attachedDesktopPetWindow"))
                 ->isVisible());

    window.setWindowState(Qt::WindowNoState);
    QTRY_VERIFY(topLevelByName(QStringLiteral("attachedDesktopPetWindow"))
                    ->isVisible());
    petSettings.setEnabled(false);
    QTRY_VERIFY(!topLevelByName(QStringLiteral("attachedDesktopPetWindow"))
                     ->isVisible());
    QVERIFY(!topLevelByName(QStringLiteral("floatingDesktopPetWindow"))
                 ->isVisible());
}

QTEST_MAIN(WidgetsDesktopPetFlowTest)
#include "tst_WidgetsDesktopPetFlow.moc"
