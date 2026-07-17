#include "AppBootstrapper.h"
#include "AppViewModel.h"
#include "persistence/SqliteTaskRepository.h"
#include "services/StatisticsService.h"
#include "services/TaskCategoryService.h"
#include "services/TaskService.h"
#include "view/widgets/MainWindow.h"
#include "view/widgets/statistics/StatisticsPage.h"
#include "view/widgets/focus/FocusPage.h"
#include "viewmodel/contracts/StatisticsContract.h"

#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTest>

using namespace smartmate;

namespace {

int modelTotal(const QAbstractItemModel &model, const int role)
{
    int total = 0;
    for (int row = 0; row < model.rowCount(); ++row) {
        total += model.index(row, 0).data(role).toInt();
    }
    return total;
}

template<typename Widget>
Widget *requiredChild(QObject &parent, const char *objectName)
{
    Widget *result = parent.findChild<Widget *>(QString::fromLatin1(objectName));
    Q_ASSERT(result != nullptr);
    return result;
}

} // namespace

class WidgetsStatisticsFlowTest final : public QObject {
    Q_OBJECT

private slots:
    void taskEventsAutomaticallyRefreshTheDashboard();
    void bootstrapInjectsTheStatisticsPageIntoNavigation();
};

void WidgetsStatisticsFlowTest::taskEventsAutomaticallyRefreshTheDashboard()
{
    model::persistence::SqliteTaskRepository repository{QStringLiteral(":memory:")};
    model::TaskService taskService{repository, repository, repository,
                                   repository, repository, repository};
    model::TaskCategoryService categoryService{repository};
    model::StatisticsService statisticsService{repository, repository, repository};
    viewmodel::AppViewModel app{taskService, statisticsService};
    view::widgets::StatisticsPage page{*app.statistics()};
    page.resize(1100, 700);
    page.show();

    QCOMPARE(app.statistics()->todayCount(), 0);
    QCOMPARE(requiredChild<QLabel>(page, "todayStatisticsValue")->text(),
             QStringLiteral("0 次"));

    const auto category = categoryService.createCategory(
        {QStringLiteral("学习"), model::TaskCategoryColor::Blue});
    QVERIFY2(category.ok(), qPrintable(category.detail));
    model::TaskDraft draft;
    draft.title = QStringLiteral("完成统计纵向链路");
    draft.categoryId = category.value->id;
    const auto created = taskService.createTask(draft);
    QVERIFY2(created.ok(), qPrintable(created.detail));
    const model::TaskId taskId = created.value->id();

    QVERIFY(taskService.startTask(taskId).ok());
    QVERIFY(taskService.completeTask(taskId).ok());
    QTRY_COMPARE(app.statistics()->todayCount(), 1);
    QTRY_COMPARE(requiredChild<QLabel>(page, "todayStatisticsValue")->text(),
                 QStringLiteral("1 次"));
    QTRY_COMPARE(modelTotal(*app.statistics()->trend(),
                            viewmodel::StatisticsTrendContract::ValueRole), 1);
    QTRY_COMPARE(modelTotal(*app.statistics()->categories(),
                            viewmodel::StatisticsCategoryContract::ValueRole), 1);

    QVERIFY(taskService.redoTask(taskId).ok());
    QVERIFY(taskService.startTask(taskId).ok());
    QVERIFY(taskService.completeTask(taskId).ok());
    QTRY_COMPARE(app.statistics()->todayCount(), 2);
    QTRY_COMPARE(modelTotal(*app.statistics()->trend(),
                            viewmodel::StatisticsTrendContract::ValueRole), 2);
    QTRY_COMPARE(modelTotal(*app.statistics()->categories(),
                            viewmodel::StatisticsCategoryContract::ValueRole), 2);

    QVERIFY(taskService.archiveTask(taskId).ok());
    QTRY_COMPARE(app.statistics()->todayCount(), 2);
    QCOMPARE(modelTotal(*app.statistics()->trend(),
                        viewmodel::StatisticsTrendContract::ValueRole), 2);

    QVERIFY(taskService.deleteArchivedTask(taskId).ok());
    QTRY_COMPARE(app.statistics()->todayCount(), 0);
    QTRY_VERIFY(!app.statistics()->hasCompletionHistory());
    QTRY_COMPARE(modelTotal(*app.statistics()->trend(),
                            viewmodel::StatisticsTrendContract::ValueRole), 0);
    QTRY_COMPARE(app.statistics()->categories()->rowCount(), 0);
    QTRY_COMPARE(requiredChild<QLabel>(page, "todayStatisticsValue")->text(),
                 QStringLiteral("0 次"));
}

void WidgetsStatisticsFlowTest::bootstrapInjectsTheStatisticsPageIntoNavigation()
{
    app::AppBootstrapper bootstrap{QStringLiteral(":memory:")};
    view::widgets::MainWindow window{bootstrap.widgetDependencies()};
    window.show();

    auto *pages = window.findChild<QStackedWidget *>();
    QVERIFY(pages != nullptr);
    QCOMPARE(pages->count(), 5);
    QVERIFY(qobject_cast<view::widgets::FocusPage *>(pages->widget(2)) != nullptr);
    QVERIFY(qobject_cast<view::widgets::StatisticsPage *>(pages->widget(3)) != nullptr);

    auto *focusNavigation = requiredChild<QPushButton>(
        window, "focusNavigationButton");
    QCOMPARE(focusNavigation->accessibleName(), QStringLiteral("专注"));
    QTest::mouseClick(focusNavigation, Qt::LeftButton);
    QCOMPARE(pages->currentIndex(), 2);

    auto *statisticsNavigation = requiredChild<QPushButton>(
        window, "statisticsNavigationButton");
    QCOMPARE(statisticsNavigation->accessibleName(), QStringLiteral("统计"));
    QTest::mouseClick(statisticsNavigation, Qt::LeftButton);
    QCOMPARE(pages->currentIndex(), 3);

    QTest::mouseClick(requiredChild<QPushButton>(window, "settingsNavigationButton"),
                      Qt::LeftButton);
    QCOMPARE(pages->currentIndex(), 4);
}

QTEST_MAIN(WidgetsStatisticsFlowTest)
#include "tst_WidgetsStatisticsFlow.moc"
