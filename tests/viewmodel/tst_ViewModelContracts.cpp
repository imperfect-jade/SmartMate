#include "AppearanceSettingsViewModel.h"
#include "TaskCategoryViewModel.h"
#include "TaskDependencyViewModel.h"
#include "TaskEditorViewModel.h"
#include "TaskGraphViewModel.h"
#include "TaskListViewModel.h"
#include "TaskFocusViewModel.h"
#include "TaskDetailsViewModel.h"
#include "common/presentation/UiNotification.h"
#include "fakes/FakeAppearanceSettingsRepository.h"
#include "fakes/FakeTaskBatchTransitionRepository.h"
#include "fakes/FakeTaskCategoryRepository.h"
#include "fakes/FakeTaskCreationRepository.h"
#include "fakes/FakeTaskDeletionRepository.h"
#include "fakes/FakeTaskDependencyRepository.h"
#include "fakes/FakeTaskRepository.h"
#include "services/AppearanceSettingsService.h"
#include "services/TaskService.h"
#include "viewmodel/contracts/AppearanceSettingsContract.h"
#include "viewmodel/contracts/TaskCategoryContract.h"
#include "viewmodel/contracts/TaskDependencyContract.h"
#include "viewmodel/contracts/TaskEditorContract.h"
#include "viewmodel/contracts/TaskGraphContract.h"
#include "viewmodel/contracts/TaskListContract.h"
#include "viewmodel/contracts/TaskFocusContract.h"
#include "viewmodel/contracts/TaskDetailsContract.h"

#include <QAbstractItemModelTester>
#include <QMetaEnum>
#include <QMetaMethod>
#include <QSignalSpy>
#include <QTest>
#include <QVariant>

#include <type_traits>

using namespace smartmate;

static_assert(std::is_abstract_v<viewmodel::AppearanceSettingsContract>);
static_assert(std::is_abstract_v<viewmodel::TaskCategoryContract>);
static_assert(std::is_abstract_v<viewmodel::TaskDependencyContract>);
static_assert(std::is_abstract_v<viewmodel::TaskEditorContract>);
static_assert(std::is_abstract_v<viewmodel::TaskGraphContract>);
static_assert(std::is_abstract_v<viewmodel::TaskListContract>);
static_assert(std::is_abstract_v<viewmodel::TaskFocusContract>);
static_assert(std::is_abstract_v<viewmodel::TaskDetailsContract>);

static_assert(std::has_virtual_destructor_v<viewmodel::AppearanceSettingsContract>);
static_assert(std::has_virtual_destructor_v<viewmodel::TaskCategoryContract>);
static_assert(std::has_virtual_destructor_v<viewmodel::TaskDependencyContract>);
static_assert(std::has_virtual_destructor_v<viewmodel::TaskEditorContract>);
static_assert(std::has_virtual_destructor_v<viewmodel::TaskGraphContract>);
static_assert(std::has_virtual_destructor_v<viewmodel::TaskListContract>);
static_assert(std::has_virtual_destructor_v<viewmodel::TaskFocusContract>);
static_assert(std::has_virtual_destructor_v<viewmodel::TaskDetailsContract>);

static_assert(std::is_base_of_v<viewmodel::AppearanceSettingsContract,
                                viewmodel::AppearanceSettingsViewModel>);
static_assert(std::is_base_of_v<viewmodel::TaskCategoryContract,
                                viewmodel::TaskCategoryViewModel>);
static_assert(std::is_base_of_v<viewmodel::TaskDependencyContract,
                                viewmodel::TaskDependencyViewModel>);
static_assert(std::is_base_of_v<viewmodel::TaskEditorContract,
                                viewmodel::TaskEditorViewModel>);
static_assert(std::is_base_of_v<viewmodel::TaskGraphContract,
                                viewmodel::TaskGraphViewModel>);
static_assert(std::is_base_of_v<viewmodel::TaskListContract,
                                viewmodel::TaskListViewModel>);
static_assert(std::is_base_of_v<viewmodel::TaskFocusContract,
                                viewmodel::TaskFocusViewModel>);
static_assert(std::is_base_of_v<viewmodel::TaskDetailsContract,
                                viewmodel::TaskDetailsViewModel>);

namespace {

struct TaskServiceFixture {
    tests::FakeTaskRepository tasks;
    tests::FakeTaskDependencyRepository dependencies;
    tests::FakeTaskCreationRepository creation{tasks, dependencies};
    tests::FakeTaskBatchTransitionRepository batchTransitions{tasks};
    tests::FakeTaskDeletionRepository deletion{tasks, dependencies};
    tests::FakeTaskCategoryRepository categories;
    model::TaskService service{tasks, dependencies, creation, batchTransitions,
                               deletion, categories};
};

[[nodiscard]] bool hasMetaMethod(const QMetaObject &metaObject,
                                 const QByteArray &methodName)
{
    for (int index = 0; index < metaObject.methodCount(); ++index) {
        if (metaObject.method(index).name() == methodName) {
            return true;
        }
    }
    return false;
}

void verifyErrorNotifications(QSignalSpy &spy, const QString &title)
{
    QCOMPARE(spy.count(), 2);
    for (const auto &arguments : spy) {
        const auto notification =
            qvariant_cast<common::UiNotification>(arguments.constFirst());
        QCOMPARE(notification.severity, common::UiSeverity::Error);
        QCOMPARE(notification.title, title);
        QVERIFY(!notification.message.isEmpty());
    }
}

} // namespace

class ViewModelContractsTest final : public QObject {
    Q_OBJECT

private slots:
    void metatypesAreRegistered();
    void contractReferencesDispatchToConcreteImplementations();
    void concreteMetaObjectsExposeInheritedQmlApi();
    void listImplementationsRespectTheItemModelProtocol();
    void failuresRaiseRepeatableTypedNotifications();
};

void ViewModelContractsTest::metatypesAreRegistered()
{
    QVERIFY(QMetaType::fromType<common::UiSeverity>().isValid());
    QVERIFY(QMetaType::fromType<common::UiNotification>().isValid());
    const common::UiNotification expected{common::UiSeverity::Warning,
                                          QStringLiteral("标题"),
                                          QStringLiteral("消息")};
    QCOMPARE(QVariant::fromValue(expected).value<common::UiNotification>(), expected);
}

void ViewModelContractsTest::contractReferencesDispatchToConcreteImplementations()
{
    viewmodel::AppearanceSettingsViewModel appearance;
    viewmodel::AppearanceSettingsContract &appearanceContract = appearance;
    appearanceContract.setAccentThemeIndex(1);
    QCOMPARE(appearanceContract.accentThemeIndex(), 1);

    TaskServiceFixture fixture;
    viewmodel::TaskListViewModel list{fixture.service};
    viewmodel::TaskListContract &listContract = list;
    listContract.setShowArchived(true);
    QVERIFY(listContract.showArchived());

    viewmodel::TaskFocusViewModel focus{fixture.service};
    viewmodel::TaskFocusContract &focusContract = focus;
    QCOMPARE(focusContract.focusState(),
             viewmodel::TaskFocusContract::FocusState::NoTasks);

    viewmodel::TaskDetailsViewModel details{fixture.service};
    viewmodel::TaskDetailsContract &detailsContract = details;
    QVERIFY(!detailsContract.selectTask(QStringLiteral("invalid")));

    viewmodel::TaskCategoryViewModel category{fixture.service};
    viewmodel::TaskCategoryContract &categoryContract = category;
    categoryContract.beginCreate();
    categoryContract.setDraftName(QStringLiteral("学习"));
    QCOMPARE(categoryContract.draftName(), QStringLiteral("学习"));

    viewmodel::TaskDependencyViewModel dependency{fixture.service};
    viewmodel::TaskDependencyContract &dependencyContract = dependency;
    QVERIFY(!dependencyContract.beginEdit(QStringLiteral("invalid")));

    viewmodel::TaskEditorViewModel editor{fixture.service};
    viewmodel::TaskEditorContract &editorContract = editor;
    QVERIFY(editorContract.beginCreate());
    editorContract.setTitle(QStringLiteral("契约调用"));
    QCOMPARE(editorContract.title(), QStringLiteral("契约调用"));

    viewmodel::TaskGraphViewModel graph{fixture.service};
    viewmodel::TaskGraphContract &graphContract = graph;
    graphContract.setSearchText(QStringLiteral("节点"));
    QCOMPARE(graphContract.searchText(), QStringLiteral("节点"));
}

void ViewModelContractsTest::concreteMetaObjectsExposeInheritedQmlApi()
{
    const QMetaObject &listMeta = viewmodel::TaskListViewModel::staticMetaObject;
    QVERIFY(listMeta.indexOfProperty("searchText") >= 0);
    QVERIFY(hasMetaMethod(listMeta, QByteArrayLiteral("startTask")));
    QVERIFY(hasMetaMethod(listMeta, QByteArrayLiteral("notificationRaised")));
    QVERIFY(listMeta.indexOfEnumerator("Role") >= 0);

    const QMetaObject &focusMeta = viewmodel::TaskFocusViewModel::staticMetaObject;
    QVERIFY(focusMeta.indexOfProperty("focusTaskId") >= 0);
    QVERIFY(focusMeta.indexOfEnumerator("FocusState") >= 0);
    const QMetaObject &detailsMeta = viewmodel::TaskDetailsViewModel::staticMetaObject;
    QVERIFY(detailsMeta.indexOfProperty("selectedTaskId") >= 0);
    QVERIFY(hasMetaMethod(detailsMeta, QByteArrayLiteral("selectTask")));

    const QMetaObject &graphMeta = viewmodel::TaskGraphViewModel::staticMetaObject;
    QVERIFY(graphMeta.indexOfProperty("edges") >= 0);
    QVERIFY(hasMetaMethod(graphMeta, QByteArrayLiteral("selectTask")));
    QVERIFY(graphMeta.indexOfEnumerator("EmphasisLevel") >= 0);
    QVERIFY(graphMeta.indexOfEnumerator("EdgeRole") >= 0);
    QVERIFY(graphMeta.indexOfEnumerator("RelationRole") >= 0);

    const QMetaObject &editorMeta = viewmodel::TaskEditorViewModel::staticMetaObject;
    QVERIFY(editorMeta.indexOfProperty("deadlineDisplayText") >= 0);
    QVERIFY(editorMeta.indexOfProperty("sessionActive") >= 0);
    QVERIFY(hasMetaMethod(editorMeta, QByteArrayLiteral("setDeadlineSelection")));

    QVERIFY(viewmodel::TaskCategoryViewModel::staticMetaObject
                .indexOfProperty("draftName") >= 0);
    QVERIFY(viewmodel::TaskDependencyViewModel::staticMetaObject
                .indexOfProperty("selectedCount") >= 0);
    QVERIFY(viewmodel::AppearanceSettingsViewModel::staticMetaObject
                .indexOfProperty("fontScale") >= 0);
}

void ViewModelContractsTest::listImplementationsRespectTheItemModelProtocol()
{
    TaskServiceFixture fixture;
    viewmodel::TaskCategoryViewModel category{fixture.service};
    viewmodel::TaskDependencyViewModel dependency{fixture.service};
    viewmodel::TaskEditorViewModel editor{fixture.service};
    viewmodel::TaskGraphViewModel graph{fixture.service};
    viewmodel::TaskListViewModel list{fixture.service};

    QAbstractItemModelTester categoryTester{
        &category, QAbstractItemModelTester::FailureReportingMode::QtTest};
    QAbstractItemModelTester dependencyTester{
        &dependency, QAbstractItemModelTester::FailureReportingMode::QtTest};
    QAbstractItemModelTester editorTester{
        &editor, QAbstractItemModelTester::FailureReportingMode::QtTest};
    QAbstractItemModelTester graphTester{
        &graph, QAbstractItemModelTester::FailureReportingMode::QtTest};
    QAbstractItemModelTester edgeTester{
        graph.edges(), QAbstractItemModelTester::FailureReportingMode::QtTest};
    QAbstractItemModelTester predecessorTester{
        graph.selectedPredecessors(),
        QAbstractItemModelTester::FailureReportingMode::QtTest};
    QAbstractItemModelTester successorTester{
        graph.selectedSuccessors(),
        QAbstractItemModelTester::FailureReportingMode::QtTest};
    QAbstractItemModelTester listTester{
        &list, QAbstractItemModelTester::FailureReportingMode::QtTest};

    QCOMPARE(category.rowCount(), 0);
    QCOMPARE(dependency.rowCount(), 0);
    QCOMPARE(editor.rowCount(), 0);
    QCOMPARE(graph.rowCount(), 0);
    QCOMPARE(graph.edges()->roleNames().value(
                 viewmodel::TaskGraphContract::EdgeRoutePointsRole),
             QByteArrayLiteral("routePoints"));
    QCOMPARE(graph.selectedPredecessors()->roleNames().value(
                 viewmodel::TaskGraphContract::RelationTaskIdRole),
             QByteArrayLiteral("taskId"));
    QCOMPARE(list.rowCount(), 0);
}

void ViewModelContractsTest::failuresRaiseRepeatableTypedNotifications()
{
    tests::FakeAppearanceSettingsRepository appearanceRepository;
    model::AppearanceSettingsService appearanceService{appearanceRepository};
    viewmodel::AppearanceSettingsViewModel appearance{appearanceService};
    appearanceRepository.failSave = true;
    QSignalSpy appearanceSpy{&appearance,
        &viewmodel::AppearanceSettingsContract::notificationRaised};
    appearance.setFontScaleIndex(2);
    appearance.setFontScaleIndex(2);
    verifyErrorNotifications(appearanceSpy, QStringLiteral("外观设置失败"));
    appearance.clearError();
    QCOMPARE(appearanceSpy.count(), 2);

    TaskServiceFixture fixture;

    viewmodel::TaskCategoryViewModel category{fixture.service};
    QSignalSpy categorySpy{&category,
        &viewmodel::TaskCategoryContract::notificationRaised};
    category.beginEdit(QStringLiteral("invalid"));
    category.beginEdit(QStringLiteral("invalid"));
    verifyErrorNotifications(categorySpy, QStringLiteral("类别操作失败"));
    category.clearError();
    QCOMPARE(categorySpy.count(), 2);

    viewmodel::TaskDependencyViewModel dependency{fixture.service};
    QSignalSpy dependencySpy{&dependency,
        &viewmodel::TaskDependencyContract::notificationRaised};
    dependency.beginEdit(QStringLiteral("invalid"));
    dependency.beginEdit(QStringLiteral("invalid"));
    verifyErrorNotifications(dependencySpy, QStringLiteral("依赖操作失败"));
    dependency.clearError();
    QCOMPARE(dependencySpy.count(), 2);

    viewmodel::TaskEditorViewModel editor{fixture.service};
    QSignalSpy editorSpy{&editor,
        &viewmodel::TaskEditorContract::notificationRaised};
    editor.save();
    editor.save();
    verifyErrorNotifications(editorSpy, QStringLiteral("任务编辑失败"));

    viewmodel::TaskListViewModel list{fixture.service};
    QSignalSpy listSpy{&list,
        &viewmodel::TaskListContract::notificationRaised};
    list.startTask(QStringLiteral("invalid"));
    list.startTask(QStringLiteral("invalid"));
    verifyErrorNotifications(listSpy, QStringLiteral("任务操作失败"));
    list.clearError();
    QCOMPARE(listSpy.count(), 2);

    viewmodel::TaskGraphViewModel graph{fixture.service};
    QSignalSpy graphSpy{&graph,
        &viewmodel::TaskGraphContract::notificationRaised};
    fixture.tasks.setReadFailure(true);
    graph.reload();
    graph.reload();
    verifyErrorNotifications(graphSpy, QStringLiteral("依赖图操作失败"));
}

QTEST_GUILESS_MAIN(ViewModelContractsTest)
#include "tst_ViewModelContracts.moc"
