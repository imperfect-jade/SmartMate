#include "AppViewModel.h"
#include "domain/TaskCreationRequest.h"
#include "persistence/SqliteTaskRepository.h"
#include "services/TaskCategoryService.h"
#include "services/TaskService.h"
#include "view/widgets/graph/DependencyGraphPage.h"
#include "view/widgets/graph/DependencyGraphView.h"
#include "view/widgets/task/TaskDependencyDialog.h"

#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTest>

using namespace smartmate;

namespace {

int categoryIndex(QComboBox &combo, const QString &id)
{
    for (int index = 0; index < combo.count(); ++index)
        if (combo.itemData(index, Qt::UserRole + 1).toString() == id) return index;
    return -1;
}

int graphRow(viewmodel::TaskGraphContract &graph, const QString &id)
{
    for (int row = 0; row < graph.rowCount(); ++row)
        if (graph.data(graph.index(row), viewmodel::TaskGraphContract::TaskIdRole)
                .toString() == id) return row;
    return -1;
}

} // namespace

class WidgetsGraphFlowTest final : public QObject {
    Q_OBJECT
private slots:
    void graphReflectsServiceStateAndWidgetCommands();
};

void WidgetsGraphFlowTest::graphReflectsServiceStateAndWidgetCommands()
{
    model::persistence::SqliteTaskRepository repository{QStringLiteral(":memory:")};
    model::TaskService taskService{repository, repository, repository,
                                   repository, repository, repository};
    model::TaskCategoryService categoryService{repository};
    const auto categoryResult = categoryService.createCategory(
        {QStringLiteral("学习"), model::TaskCategoryColor::Blue});
    QVERIFY(categoryResult.ok());

    model::TaskDraft predecessorDraft;
    predecessorDraft.title = QStringLiteral("需求分析");
    predecessorDraft.categoryId = categoryResult.value->id;
    const auto predecessorResult = taskService.createTask(predecessorDraft);
    QVERIFY(predecessorResult.ok());
    const model::TaskId predecessorId = predecessorResult.value->id();

    model::TaskDraft successorDraft;
    successorDraft.title = QStringLiteral("实现任务模块");
    const auto successorResult = taskService.createTask(
        model::TaskCreationRequest{successorDraft, {predecessorId}});
    QVERIFY(successorResult.ok());
    const model::TaskId successorId = successorResult.value->id();
    const QString predecessorText = predecessorId.toString(QUuid::WithoutBraces);
    const QString successorText = successorId.toString(QUuid::WithoutBraces);
    const QString categoryText = categoryResult.value->id.toString(QUuid::WithoutBraces);

    viewmodel::AppViewModel app{taskService, categoryService};
    view::widgets::DependencyGraphPage page{{*app.appearanceSettings(),
                                              *app.taskGraph(), *app.taskDetails(),
                                              *app.taskDependencies()}};
    page.resize(920, 700);
    page.show();
    auto *view = page.findChild<view::widgets::DependencyGraphView *>(
        QStringLiteral("dependencyGraphViewport"));
    QVERIFY(view);
    QTRY_COMPARE(view->nodeItemCount(), 2);
    QTRY_COMPARE(view->edgeItemCount(), 1);
    const int successorRow = graphRow(*app.taskGraph(), successorText);
    QVERIFY(successorRow >= 0);
    QVERIFY(app.taskGraph()->data(app.taskGraph()->index(successorRow),
        viewmodel::TaskGraphContract::BlockedRole).toBool());

    auto *search = page.findChild<QLineEdit *>(QStringLiteral("graphSearchField"));
    search->setText(QStringLiteral("实现任务"));
    emit search->textEdited(search->text());
    emit search->returnPressed();
    QTRY_COMPARE(app.taskGraph()->selectedTaskId(), successorText);
    QVERIFY(page.findChild<QFrame *>(QStringLiteral("dependencyGraphDetails"))->isVisible());

    auto *category = page.findChild<QComboBox *>(QStringLiteral("graphCategoryFilter"));
    const int studyIndex = categoryIndex(*category, categoryText);
    QVERIFY(studyIndex >= 0);
    category->setCurrentIndex(studyIndex);
    emit category->activated(studyIndex);
    QTRY_COMPARE(app.taskGraph()->categoryFilterMode(), 2);
    QCOMPARE(app.taskList()->categoryFilterMode(), 0);
    QCOMPARE(app.taskGraph()->rowCount(), 2);
    const int predecessorRow = graphRow(*app.taskGraph(), predecessorText);
    QVERIFY(predecessorRow >= 0);
    QVERIFY(app.taskGraph()->data(app.taskGraph()->index(predecessorRow),
        viewmodel::TaskGraphContract::CoreNodeRole).toBool());
    QVERIFY(!app.taskGraph()->data(app.taskGraph()->index(successorRow),
        viewmodel::TaskGraphContract::CoreNodeRole).toBool());

    QAbstractItemModel *edges = app.taskGraph()->edges();
    QVERIFY(taskService.cancelTask(predecessorId).ok());
    QTRY_VERIFY(edges->data(edges->index(0, 0),
        viewmodel::TaskGraphContract::EdgeCancelledRole).toBool());
    QCOMPARE(view->edgeItemCount(), 1);
    QVERIFY(taskService.redoTask(predecessorId).ok());
    QTRY_VERIFY(!edges->data(edges->index(0, 0),
        viewmodel::TaskGraphContract::EdgeCancelledRole).toBool());

    QVERIFY(app.taskGraph()->setCategoryFilter(0));
    QVERIFY(app.taskGraph()->selectTask(successorText));
    auto *edit = page.findChild<QPushButton *>(
        QStringLiteral("editSelectedGraphDependenciesButton"));
    QTRY_VERIFY(edit->isVisible());
    QTest::mouseClick(edit, Qt::LeftButton);
    QTRY_COMPARE(app.taskDependencies()->taskId(), successorText);
    auto *dialog = page.findChild<view::widgets::TaskDependencyDialog *>(
        QStringLiteral("graphTaskDependencyDialog"));
    QVERIFY(dialog->isVisible());
    dialog->reject();

    QVERIFY(taskService.replaceTaskPredecessors(successorId, {}).ok());
    QTRY_COMPARE(view->edgeItemCount(), 0);
    const int refreshedSuccessorRow = graphRow(*app.taskGraph(), successorText);
    QVERIFY(refreshedSuccessorRow >= 0);
    QVERIFY(!app.taskGraph()->data(app.taskGraph()->index(refreshedSuccessorRow),
        viewmodel::TaskGraphContract::BlockedRole).toBool());
}

QTEST_MAIN(WidgetsGraphFlowTest)
#include "tst_WidgetsGraphFlow.moc"
