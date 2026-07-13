#include "view/widgets/graph/DependencyGraphPage.h"
#include "view/widgets/graph/DependencyGraphView.h"
#include "view/widgets/graph/TaskGraphItems.h"
#include "view/widgets/task/TaskDependencyDialog.h"
#include "view/widgets/task/TaskDetailsDialog.h"
#include "viewmodel/contracts/AppearanceSettingsContract.h"
#include "viewmodel/contracts/TaskDependencyContract.h"
#include "viewmodel/contracts/TaskDetailsContract.h"
#include "viewmodel/contracts/TaskGraphContract.h"

#include <QComboBox>
#include <QFrame>
#include <QGraphicsScene>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QTest>

using namespace smartmate;

namespace {

constexpr auto firstId = "11111111-1111-1111-1111-111111111111";
constexpr auto secondId = "22222222-2222-2222-2222-222222222222";
constexpr auto categoryId = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";

class FlatModel final : public QAbstractListModel {
public:
    explicit FlatModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex &parent = {}) const override
    { return parent.isValid() ? 0 : rows.size(); }
    QVariant data(const QModelIndex &index, int role) const override
    { return index.isValid() && index.row() < rows.size() ? rows.at(index.row()).value(role) : QVariant{}; }
    void setRole(const int row, const int role, const QVariant &value)
    {
        rows[row][role] = value;
        emit dataChanged(index(row), index(row), {role});
    }
    QList<QHash<int, QVariant>> rows;
};

class FakeAppearance final : public viewmodel::AppearanceSettingsContract {
public:
    FakeAppearance() : AppearanceSettingsContract(nullptr) {}
    int accentThemeIndex() const noexcept override { return accent; }
    int fontFamilyIndex() const noexcept override { return 0; }
    int fontScaleIndex() const noexcept override { return 1; }
    QStringList accentThemeOptions() const override { return {"Green", "Blue"}; }
    QStringList fontFamilyOptions() const override { return {"Default"}; }
    QStringList fontScaleOptions() const override { return {"Small", "Normal", "Large"}; }
    QString fontFamilyName() const override { return {}; }
    qreal fontScale() const noexcept override { return 1.0; }
    void setAccentThemeIndex(int value) override { accent = value; emit appearanceChanged(); }
    void setFontFamilyIndex(int) override {}
    void setFontScaleIndex(int) override {}
    void resetDefaults() override { accent = 0; emit appearanceChanged(); }
    int accent{0};
};

class FakeDetails final : public viewmodel::TaskDetailsContract {
public:
    FakeDetails() : TaskDetailsContract(nullptr) {}
    QString selectedTaskId() const override { return id; }
    QString selectedTitle() const override { return QStringLiteral("实现任务模块"); }
    QString selectedDescription() const override { return QStringLiteral("详情"); }
    QString selectedStatusText() const override { return QStringLiteral("待办"); }
    QString selectedPriorityText() const override { return QStringLiteral("高"); }
    QString selectedDeadlineText() const override { return QStringLiteral("未设置"); }
    int selectedEstimatedMinutes() const noexcept override { return 30; }
    QString selectedCreatedAtText() const override { return QStringLiteral("今天"); }
    QString selectedUpdatedAtText() const override { return QStringLiteral("今天"); }
    QString selectedReasonText() const override { return QStringLiteral("推荐"); }
    QString selectedBlockingReasonText() const override { return {}; }
    int selectedPredecessorCount() const noexcept override { return 1; }
    int selectedUnlockCount() const noexcept override { return 0; }
    bool selectedCanEditTask() const noexcept override { return true; }
    bool selectedCanEditDependencies() const noexcept override { return true; }
    QString selectedCategoryName() const override { return QStringLiteral("学习"); }
    QString selectedCategoryAccent() const override { return QStringLiteral("#175cd3"); }
    bool selectedHasCategory() const noexcept override { return true; }
    bool selectTask(const QString &taskId) override { id = taskId; ++selectCalls; emit selectionChanged(); return true; }
    void clearSelection() override { id.clear(); emit selectionChanged(); }
    QString id;
    int selectCalls{0};
};

class FakeDependency final : public viewmodel::TaskDependencyContract {
public:
    FakeDependency() : TaskDependencyContract(nullptr) {}
    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : 0; }
    QVariant data(const QModelIndex &, int) const override { return {}; }
    QString taskId() const override { return id; }
    QString taskTitle() const override { return QStringLiteral("任务"); }
    int count() const noexcept override { return 0; }
    int selectedCount() const noexcept override { return 0; }
    bool dirty() const noexcept override { return false; }
    bool canSave() const noexcept override { return false; }
    bool beginEdit(const QString &taskId) override { id = taskId; ++beginCalls; emit contextChanged(); return true; }
    bool setPredecessorSelected(const QString &, bool) override { return false; }
    bool save() override { return true; }
    void cancel() override { ++cancelCalls; emit cancelled(); }
    QString id;
    int beginCalls{0};
    int cancelCalls{0};
};

class FakeGraph final : public viewmodel::TaskGraphContract {
public:
    FakeGraph() : TaskGraphContract(nullptr), edgeModel(this), predecessorModel(this), successorModel(this)
    {
        auto node = [](const QString &id, const QString &title, qreal y, bool blocked) {
            return QHash<int, QVariant>{{TaskIdRole, id}, {ShortIdRole, id.left(8)},
                {TitleRole, title}, {StatusTextRole, QStringLiteral("待办")},
                {StatusIndexRole, 0}, {PriorityTextRole, QStringLiteral("高")},
                {DeadlineTextRole, QStringLiteral("未设置截止时间")}, {UnlockCountRole, 0},
                {BlockedRole, blocked}, {BlockingReasonTextRole, blocked ? QStringLiteral("等待需求分析完成或取消") : QString{}},
                {ArchivedRole, false}, {CanEditDependenciesRole, true},
                {NodeXRole, 52.0}, {NodeYRole, y}, {NodeWidthRole, 220.0}, {NodeHeightRole, 92.0},
                {SelectedRole, false}, {EmphasisLevelRole, NormalEmphasis}, {FilterMatchedRole, true},
                {CategoryNameRole, QStringLiteral("学习")}, {CategoryAccentRole, QStringLiteral("#175cd3")},
                {HasCategoryRole, true}, {CoreNodeRole, true}};
        };
        nodes = {node(QString::fromLatin1(firstId), QStringLiteral("需求分析"), 52, false),
                 node(QString::fromLatin1(secondId), QStringLiteral("实现任务模块"), 254, true)};
        QVariantList route{QPointF{162, 144}, QPointF{162, 199}, QPointF{162, 254}};
        edgeModel.rows = {{
            {EdgePredecessorIdRole, QString::fromLatin1(firstId)},
            {EdgeSuccessorIdRole, QString::fromLatin1(secondId)},
            {EdgeRoutePointsRole, route}, {EdgeArrowTipXRole, 162.0}, {EdgeArrowTipYRole, 254.0},
            {EdgeArrowLeftXRole, 156.0}, {EdgeArrowLeftYRole, 242.0},
            {EdgeArrowRightXRole, 168.0}, {EdgeArrowRightYRole, 242.0},
            {EdgeSatisfiedRole, false}, {EdgeCancelledRole, false},
            {EdgeHighlightedRole, false}, {EdgeDimmedRole, false}, {EdgeHoveredRole, false}}};
        predecessorModel.rows = {{{RelationTaskIdRole, QString::fromLatin1(firstId)},
            {RelationTitleRole, QStringLiteral("需求分析")}, {RelationStatusTextRole, QStringLiteral("待办")},
            {RelationTextRole, QStringLiteral("待完成")}}};
    }
    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : nodes.size(); }
    QVariant data(const QModelIndex &index, int role) const override
    { return index.isValid() && index.row() < nodes.size() ? nodes.at(index.row()).value(role) : QVariant{}; }
    QAbstractItemModel *edges() noexcept override { return &edgeModel; }
    QAbstractItemModel *selectedPredecessors() noexcept override { return &predecessorModel; }
    QAbstractItemModel *selectedSuccessors() noexcept override { return &successorModel; }
    qreal contentWidth() const noexcept override { return 324; }
    qreal contentHeight() const noexcept override { return 398; }
    QString searchText() const override { return search; }
    void setSearchText(const QString &value) override { if (search == value) return; search = value; ++searchWrites; emit searchTextChanged(); }
    int statusFilterIndex() const noexcept override { return status; }
    void setStatusFilterIndex(int value) override { status = value; ++statusWrites; emit statusFilterIndexChanged(); }
    QVariantList categoryFilterOptions() const override { return {
        QVariantMap{{"mode", 0}, {"categoryId", ""}, {"name", QStringLiteral("全部类别")}},
        QVariantMap{{"mode", 2}, {"categoryId", QString::fromLatin1(categoryId)}, {"name", QStringLiteral("学习")}}}; }
    int categoryFilterMode() const noexcept override { return categoryMode; }
    QString categoryFilterCategoryId() const override { return category; }
    int taskCount() const noexcept override { return nodes.size(); }
    int blockedCount() const noexcept override { return 1; }
    QString currentTaskId() const override { return QString::fromLatin1(firstId); }
    QString selectedTaskId() const override { return selected; }
    QString selectedTaskTitle() const override { return selected.isEmpty() ? QString{} : QStringLiteral("实现任务模块"); }
    QString selectedDescription() const override { return QStringLiteral("实现 Widgets 图"); }
    QString selectedStatusText() const override { return QStringLiteral("待办"); }
    QString selectedPriorityText() const override { return QStringLiteral("高"); }
    QString selectedDeadlineText() const override { return QStringLiteral("未设置截止时间"); }
    QString selectedEstimatedDurationText() const override { return QStringLiteral("30 分钟"); }
    QString selectedBlockingReason() const override { return selected.isEmpty() ? QString{} : QStringLiteral("等待需求分析完成或取消"); }
    int selectedUnlockCount() const noexcept override { return 0; }
    int selectedPredecessorCount() const noexcept override { return selected.isEmpty() ? 0 : 1; }
    int selectedSuccessorCount() const noexcept override { return 0; }
    qreal selectedNodeCenterX() const noexcept override { return 162; }
    qreal selectedNodeCenterY() const noexcept override { return 300; }
    bool canEditSelectedDependencies() const noexcept override { return !selected.isEmpty(); }
    QString selectedCategoryName() const override { return QStringLiteral("学习"); }
    QString selectedCategoryAccent() const override { return QStringLiteral("#175cd3"); }
    bool selectedHasCategory() const noexcept override { return true; }
    bool selectedCoreNode() const noexcept override { return true; }
    bool empty() const noexcept override { return nodes.isEmpty(); }
    void reload() override { ++reloadCalls; emit graphChanged(); }
    bool selectTask(const QString &taskId) override {
        if (taskId != QString::fromLatin1(firstId) && taskId != QString::fromLatin1(secondId)) return false;
        selected = taskId; ++selectCalls;
        for (int row = 0; row < nodes.size(); ++row) nodes[row][SelectedRole] = nodes[row][TaskIdRole] == selected;
        emit dataChanged(index(0), index(nodes.size() - 1), {SelectedRole, EmphasisLevelRole});
        emit selectionChanged(); return true;
    }
    void clearSelection() override { selected.clear(); emit selectionChanged(); }
    bool locateFirstMatch() override { ++locateCalls; return selectTask(QString::fromLatin1(secondId)); }
    bool selectCurrentTask() override { ++currentCalls; return selectTask(QString::fromLatin1(firstId)); }
    bool setCategoryFilter(int mode, const QString &id = {}) override {
        categoryMode = mode; category = id; ++categoryCalls; emit categoryFilterChanged(); return true;
    }
    void setHoveredTask(const QString &id) override { hovered = id; ++hoverCalls; }
    void clearHoveredTask() override { hovered.clear(); }
    void pushSearch(const QString &value) { search = value; emit searchTextChanged(); }
    void raiseError() { emit notificationRaised({common::UiSeverity::Error, QStringLiteral("依赖图操作失败"), QStringLiteral("读取失败")}); }
    QList<QHash<int, QVariant>> nodes;
    FlatModel edgeModel, predecessorModel, successorModel;
    QString search, selected, category, hovered;
    int status{0}, categoryMode{0};
    int searchWrites{0}, statusWrites{0}, categoryCalls{0}, selectCalls{0};
    int locateCalls{0}, currentCalls{0}, reloadCalls{0}, hoverCalls{0};
};

template<typename T> T *child(QObject &parent, const char *name)
{ auto *result = parent.findChild<T *>(QString::fromLatin1(name)); Q_ASSERT(result); return result; }

} // namespace

class GraphWidgetsTest final : public QObject {
    Q_OBJECT
private slots:
    void projectedGeometryAndStableCommandsDriveTheView();
    void bindingZoomDetailsAndNotificationsMatchTheBaseline();
    void detailsPanelUsesScopedBorderlessVisualHierarchy();
    void edgeStylesAndToolbarCommandsFollowContractProjection();
};

void GraphWidgetsTest::projectedGeometryAndStableCommandsDriveTheView()
{
    FakeAppearance appearance; FakeGraph graph; FakeDetails details; FakeDependency dependencies;
    view::widgets::DependencyGraphPage page{{appearance, graph, details, dependencies}};
    page.resize(920, 700); page.show();
    auto *view = child<view::widgets::DependencyGraphView>(page, "dependencyGraphViewport");
    QCOMPARE(view->nodeItemCount(), 2);
    QCOMPARE(view->edgeItemCount(), 1);
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                      view->mapFromScene(QPointF{162, 300}));
    QTRY_COMPARE(graph.selected, QString::fromLatin1(secondId));
    QVERIFY(child<QFrame>(page, "dependencyGraphDetails")->isVisible());

    auto *category = child<QComboBox>(page, "graphCategoryFilter");
    category->setCurrentIndex(1); emit category->activated(1);
    QCOMPARE(graph.categoryCalls, 1);
    QCOMPARE(graph.category, QString::fromLatin1(categoryId));
    auto *search = child<QLineEdit>(page, "graphSearchField");
    graph.pushSearch(QStringLiteral("程序更新"));
    QCOMPARE(search->text(), QStringLiteral("程序更新"));
    QCOMPARE(graph.searchWrites, 0);
    QTest::keyClicks(search, QStringLiteral("A"));
    QVERIFY(graph.searchWrites > 0);
}

void GraphWidgetsTest::bindingZoomDetailsAndNotificationsMatchTheBaseline()
{
    FakeAppearance appearance; FakeGraph graph; FakeDetails details; FakeDependency dependencies;
    view::widgets::DependencyGraphPage page{{appearance, graph, details, dependencies}};
    page.resize(920, 700); page.show();
    auto *view = child<view::widgets::DependencyGraphView>(page, "dependencyGraphViewport");
    view->setZoomFactor(0.1); QCOMPARE(view->zoomFactor(), 0.5);
    view->setZoomFactor(3.0); QCOMPARE(view->zoomFactor(), 2.0);
    QTest::mouseClick(child<QPushButton>(page, "resetGraphZoomButton"), Qt::LeftButton);
    QCOMPARE(view->zoomFactor(), 1.0);
    QVERIFY(graph.selectTask(QString::fromLatin1(secondId)));
    QTRY_VERIFY(child<QPushButton>(page, "editSelectedGraphDependenciesButton")->isVisible());
    QTest::mouseClick(child<QPushButton>(page, "openSelectedGraphTaskDetailsButton"),
                      Qt::LeftButton);
    auto *fullDetails = child<view::widgets::TaskDetailsDialog>(
        page, "graphTaskDetailsDialog");
    QTRY_VERIFY(fullDetails->isVisible());
    QCOMPARE(details.id, QString::fromLatin1(secondId));
    QVERIFY(!child<QPushButton>(*fullDetails, "editSelectedTaskButton")->isVisible());
    QVERIFY(!child<QPushButton>(*fullDetails, "editSelectedDependenciesButton")->isVisible());
    fullDetails->reject();
    QTest::mouseClick(child<QPushButton>(page, "editSelectedGraphDependenciesButton"), Qt::LeftButton);
    QCOMPARE(dependencies.beginCalls, 1);
    QCOMPARE(dependencies.id, QString::fromLatin1(secondId));
    child<view::widgets::TaskDependencyDialog>(page, "graphTaskDependencyDialog")->reject();
    graph.raiseError();
    QTRY_VERIFY(child<QLabel>(page, "graphNotificationLabel")->isVisible());
    QCOMPARE(child<QLabel>(page, "graphNotificationLabel")->text(), QStringLiteral("读取失败"));
}

void GraphWidgetsTest::detailsPanelUsesScopedBorderlessVisualHierarchy()
{
    FakeAppearance appearance; FakeGraph graph; FakeDetails details; FakeDependency dependencies;
    view::widgets::DependencyGraphPage page{{appearance, graph, details, dependencies}};
    page.resize(920, 700);
    page.show();
    QVERIFY(graph.selectTask(QString::fromLatin1(secondId)));

    auto *panel = child<QFrame>(page, "dependencyGraphDetails");
    QTRY_VERIFY(panel->isVisible());
    QVERIFY(panel->styleSheet().contains(QStringLiteral("QFrame#dependencyGraphDetails")));
    QVERIFY(!panel->styleSheet().contains(QStringLiteral("QFrame {")));

    auto *title = child<QLabel>(page, "selectedGraphTaskTitle");
    auto *category = child<QLabel>(page, "selectedGraphTaskCategory");
    auto *context = child<QLabel>(page, "selectedGraphTaskContext");
    auto *description = child<QLabel>(page, "selectedGraphTaskDescription");
    auto *deadline = child<QLabel>(page, "selectedGraphTaskDeadline");
    auto *blocking = child<QLabel>(page, "selectedGraphTaskBlockingReason");
    QVERIFY(title->styleSheet().contains(QStringLiteral("border: none")));
    QVERIFY(description->styleSheet().contains(QStringLiteral("border: none")));
    QVERIFY(deadline->styleSheet().contains(QStringLiteral("border: none")));
    QVERIFY(category->styleSheet().contains(QStringLiteral("border: 1px solid")));
    QVERIFY(category->isVisible());
    QVERIFY(!context->isVisible());
    QVERIFY(blocking->isVisible());
    QVERIFY(blocking->styleSheet().contains(QStringLiteral("border: none")));

    auto *divider = child<QFrame>(page, "graphDetailsDivider");
    QCOMPARE(divider->frameShape(), QFrame::HLine);
    QVERIFY(divider->styleSheet().contains(QStringLiteral("QFrame#graphDetailsDivider")));
    auto *predecessors = child<QListView>(page, "graphPredecessorList");
    auto *successors = child<QListView>(page, "graphSuccessorList");
    QCOMPARE(predecessors->frameShape(), QFrame::NoFrame);
    QCOMPARE(successors->frameShape(), QFrame::NoFrame);

    const QString greenTitleStyle = title->styleSheet();
    appearance.setAccentThemeIndex(1);
    QVERIFY(title->styleSheet() != greenTitleStyle);
}

void GraphWidgetsTest::edgeStylesAndToolbarCommandsFollowContractProjection()
{
    FakeAppearance appearance; FakeGraph graph; FakeDetails details; FakeDependency dependencies;
    view::widgets::DependencyGraphPage page{{appearance, graph, details, dependencies}};
    page.resize(920, 700);
    page.show();
    auto *view = page.findChild<view::widgets::DependencyGraphView *>(
        QStringLiteral("dependencyGraphViewport"));
    QVERIFY(view);
    view::widgets::TaskGraphEdgeItem *edge = nullptr;
    for (QGraphicsItem *item : view->scene()->items()) {
        if (auto *candidate = dynamic_cast<view::widgets::TaskGraphEdgeItem *>(item)) {
            edge = candidate;
            break;
        }
    }
    QVERIFY(edge);

    const auto theme = view::widgets::WidgetTheme::fromAccentIndex(0);
    QCOMPARE(edge->presentationPen().color(), theme.warning);
    QCOMPARE(edge->presentationPen().widthF(), 2.2);
    QCOMPARE(edge->presentationPen().style(), Qt::SolidLine);

    graph.edgeModel.setRole(0, viewmodel::TaskGraphContract::EdgeSatisfiedRole, true);
    QCOMPARE(edge->presentationPen().color(), theme.done);
    graph.edgeModel.setRole(0, viewmodel::TaskGraphContract::EdgeSatisfiedRole, false);
    graph.edgeModel.setRole(0, viewmodel::TaskGraphContract::EdgeCancelledRole, true);
    QCOMPARE(edge->presentationPen().color(), theme.textDisabled);
    QCOMPARE(edge->presentationPen().style(), Qt::CustomDashLine);

    graph.edgeModel.setRole(0, viewmodel::TaskGraphContract::EdgeCancelledRole, false);
    graph.edgeModel.setRole(0, viewmodel::TaskGraphContract::EdgeHighlightedRole, true);
    QCOMPARE(edge->presentationPen().widthF(), 4.0);
    graph.edgeModel.setRole(0, viewmodel::TaskGraphContract::EdgeHighlightedRole, false);
    graph.edgeModel.setRole(0, viewmodel::TaskGraphContract::EdgeHoveredRole, true);
    QCOMPARE(edge->presentationPen().widthF(), 4.0);
    graph.edgeModel.setRole(0, viewmodel::TaskGraphContract::EdgeDimmedRole, true);
    QCOMPARE(edge->opacity(), 0.24);

}

QTEST_MAIN(GraphWidgetsTest)
#include "tst_GraphWidgets.moc"
