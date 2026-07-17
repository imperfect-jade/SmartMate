#include "AppViewModel.h"
#include "persistence/SqliteTaskRepository.h"
#include "services/FocusService.h"
#include "services/TaskService.h"
#include "view/widgets/focus/FocusPage.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDateTime>
#include <QFrame>
#include <QMessageBox>
#include <QPushButton>
#include <QTest>
#include <QTimer>

using namespace smartmate;

namespace {

template<typename Widget>
Widget *requiredChild(QObject &parent, const char *objectName)
{
    Widget *result = parent.findChild<Widget *>(QString::fromLatin1(objectName));
    Q_ASSERT(result != nullptr);
    return result;
}

void confirmNextMessageBox()
{
    QTimer::singleShot(0, [] {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (auto *box = qobject_cast<QMessageBox *>(widget)) {
                box->button(QMessageBox::Yes)->click();
            }
        }
    });
}

} // namespace

class WidgetsFocusFlowTest final : public QObject {
    Q_OBJECT

private slots:
    void taskAndFocusChangesFlowThroughThePage();
};

void WidgetsFocusFlowTest::taskAndFocusChangesFlowThroughThePage()
{
    model::persistence::SqliteTaskRepository repository{QStringLiteral(":memory:")};
    QDateTime now = QDateTime::fromString(
        QStringLiteral("2026-07-17T01:00:00.000Z"), Qt::ISODateWithMs);
    model::FocusService focusService{
        repository, repository, repository, repository,
        [&now] { return now; }, 60'000};
    QVERIFY(focusService.initialize().ok());
    model::TaskService taskService{repository, repository, repository,
                                   repository, repository, repository,
                                   &repository};
    viewmodel::AppViewModel app{taskService, focusService};
    QVERIFY(app.focus() != nullptr);
    view::widgets::FocusPage page{*app.focus()};
    page.resize(1100, 700);
    page.show();

    QCOMPARE(app.focus()->pageState(), viewmodel::FocusContract::NoInProgressTask);
    model::TaskDraft draft;
    draft.title = QStringLiteral("验证专注纵向链路");
    const auto created = taskService.createTask(draft);
    QVERIFY2(created.ok(), qPrintable(created.detail));
    const model::TaskId taskId = created.value->id();
    QVERIFY(taskService.startTask(taskId).ok());

    QTRY_COMPARE(app.focus()->pageState(), viewmodel::FocusContract::ReadyToStart);
    auto *start = requiredChild<QPushButton>(page, "startFocusButton");
    QTRY_VERIFY(!start->isHidden());
    start->click();
    QTRY_COMPARE(app.focus()->pageState(), viewmodel::FocusContract::Running);

    QCOMPARE(taskService.completeTask(taskId).error,
             model::TaskError::ActiveFocusSession);
    QCOMPARE(taskService.cancelTask(taskId).error,
             model::TaskError::ActiveFocusSession);

    now = now.addMSecs(1'500);
    requiredChild<QPushButton>(page, "pauseFocusButton")->click();
    QTRY_COMPARE(app.focus()->pageState(), viewmodel::FocusContract::Paused);
    QVERIFY(app.focus()->canComplete());
    requiredChild<QPushButton>(page, "completeFocusButton")->click();
    QTRY_COMPARE(app.focus()->historyCount(), 1);
    QTRY_COMPARE(app.focus()->pageState(), viewmodel::FocusContract::ReadyToStart);
    QTRY_COMPARE(page.findChildren<QFrame *>(QStringLiteral("focusHistoryRow")).size(), 1);

    // 同一进行中任务可以完成第二次独立专注。
    now = now.addSecs(1);
    requiredChild<QPushButton>(page, "startFocusButton")->click();
    QTRY_COMPARE(app.focus()->pageState(), viewmodel::FocusContract::Running);
    now = now.addSecs(2);
    requiredChild<QPushButton>(page, "pauseFocusButton")->click();
    QTRY_COMPARE(app.focus()->pageState(), viewmodel::FocusContract::Paused);
    requiredChild<QPushButton>(page, "completeFocusButton")->click();
    QTRY_COMPARE(app.focus()->historyCount(), 2);
    QTRY_COMPARE(app.focus()->pageState(), viewmodel::FocusContract::ReadyToStart);

    // 第三次会话放弃后不会新增历史。
    now = now.addSecs(1);
    requiredChild<QPushButton>(page, "startFocusButton")->click();
    QTRY_COMPARE(app.focus()->pageState(), viewmodel::FocusContract::Running);
    now = now.addSecs(2);
    confirmNextMessageBox();
    requiredChild<QPushButton>(page, "abandonFocusButton")->click();
    QTRY_COMPARE(app.focus()->pageState(), viewmodel::FocusContract::ReadyToStart);
    QCOMPARE(app.focus()->historyCount(), 2);

    // 活动会话结束后，任务状态转换恢复正常。
    QVERIFY(taskService.completeTask(taskId).ok());
    QTRY_COMPARE(app.focus()->pageState(), viewmodel::FocusContract::NoInProgressTask);
}

QTEST_MAIN(WidgetsFocusFlowTest)
#include "tst_WidgetsFocusFlow.moc"
