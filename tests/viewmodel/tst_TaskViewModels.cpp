#include "AppViewModel.h"
#include "TaskDependencyViewModel.h"
#include "TaskEditorViewModel.h"
#include "TaskGraphViewModel.h"
#include "TaskListViewModel.h"
#include "fakes/FakeTaskDependencyRepository.h"
#include "fakes/FakeTaskCreationRepository.h"
#include "fakes/FakeTaskRepository.h"

#include "domain/Task.h"
#include "services/TaskService.h"

#include <QDateTime>
#include <QRectF>
#include <QSet>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QTimeZone>

#include <algorithm>
#include <optional>
#include <utility>

using smartmate::model::Task;
using smartmate::model::TaskDependency;
using smartmate::model::TaskPriority;
using smartmate::model::TaskService;
using smartmate::model::TaskStatus;
using smartmate::tests::FakeTaskRepository;
using smartmate::tests::FakeTaskCreationRepository;
using smartmate::tests::FakeTaskDependencyRepository;
using smartmate::viewmodel::AppViewModel;
using smartmate::viewmodel::TaskDependencyViewModel;
using smartmate::viewmodel::TaskEditorViewModel;
using smartmate::viewmodel::TaskGraphViewModel;
using smartmate::viewmodel::TaskListViewModel;

namespace {

[[nodiscard]] QDateTime utcTime(const qint64 milliseconds)
{
    return QDateTime::fromMSecsSinceEpoch(milliseconds, QTimeZone::UTC);
}

[[nodiscard]] Task task(const QString &id,
                        QString title,
                        const TaskStatus status,
                        const qint64 updatedMilliseconds,
                        const TaskPriority priority = TaskPriority::Normal,
                        QString description = QStringLiteral("description"),
                        std::optional<QDateTime> deadline = std::nullopt)
{
    return Task{QUuid::fromString(id),
                std::move(title),
                std::move(description),
                priority,
                status,
                status == TaskStatus::Archived
                    ? std::optional<TaskStatus>{TaskStatus::Todo}
                    : std::nullopt,
                std::move(deadline),
                30,
                utcTime(1700000000000),
                utcTime(updatedMilliseconds)};
}

[[nodiscard]] QString idAt(const TaskListViewModel &viewModel, const int row)
{
    return viewModel.data(viewModel.index(row), TaskListViewModel::TaskIdRole).toString();
}

[[nodiscard]] int rowForId(const TaskListViewModel &viewModel, const QString &taskId)
{
    for (int row = 0; row < viewModel.count(); ++row) {
        if (idAt(viewModel, row) == taskId) {
            return row;
        }
    }
    return -1;
}

[[nodiscard]] QString reasonForId(const TaskListViewModel &viewModel,
                                  const QString &taskId)
{
    const int row = rowForId(viewModel, taskId);
    return row >= 0
        ? viewModel.data(viewModel.index(row),
                         TaskListViewModel::OrderReasonTextRole).toString()
        : QString{};
}

[[nodiscard]] int dependencyRowForId(const TaskDependencyViewModel &viewModel,
                                     const QString &taskId)
{
    for (int row = 0; row < viewModel.count(); ++row) {
        if (viewModel.data(viewModel.index(row),
                           TaskDependencyViewModel::TaskIdRole).toString() == taskId) {
            return row;
        }
    }
    return -1;
}

[[nodiscard]] int roleForName(const QAbstractItemModel &model,
                              const QByteArray &name)
{
    const auto names = model.roleNames();
    for (auto iterator = names.cbegin(); iterator != names.cend(); ++iterator) {
        if (iterator.value() == name) {
            return iterator.key();
        }
    }
    return -1;
}

[[nodiscard]] int graphRowForId(const TaskGraphViewModel &viewModel,
                                const QString &taskId)
{
    for (int row = 0; row < viewModel.rowCount(); ++row) {
        if (viewModel.data(viewModel.index(row), TaskGraphViewModel::TaskIdRole)
                .toString() == taskId) {
            return row;
        }
    }
    return -1;
}

} // namespace

// 测试链路使用 FakeRepository -> TaskService -> ViewModel，既覆盖可绑定状态，
// 又不依赖 QML 窗口或真实数据库。
class TaskViewModelsTest final : public QObject {
    Q_OBJECT

private slots:
    // 组合关系与列表投影。
    void appViewModelOwnsBindableChildren();
    void listProjectsActiveAndArchivedTasksInPlanOrder();
    void listMapsEveryRecommendedOrderReason();
    void listSearchesTrimmedKeywordsInTitleAndDescription();
    void listCombinesPrioritySearchAndArchiveFilters();
    void listFilterPropertiesNotifyOnceAndRejectInvalidIndexes();
    void listRetainsFiltersAndStableTaskIdAcrossServiceReloads();
    void listSchedulesMinuteRefreshForTimeSensitiveReasons();
    void listArchivesAndRestoresByStableTaskId();
    void listExposesAndClearsChineseErrors();
    void listProjectsBlockingAndUnlockInformation();
    // 依赖多选草稿、稳定ID与结构化错误映射。
    void dependencyDraftCancelDoesNotChangeModel();
    void dependencyDraftSavesStableTaskIds();
    void dependencyDraftMapsCompleteCyclePath();
    // 编辑草稿、命令入口与业务校验委托。
    void editorCreatesACompleteTypedDraft();
    void editorCancelLeavesTheStoredTaskUnchanged();
    void editorRejectsInvalidSelectionsAndMapsServiceErrors();
    void editorSupportsDurationBoundariesAndClear();
    void editorConvertsInjectedTimeZoneAndRejectsDstTransitions();
    void editorPreservesUnchangedDeadlinePrecision();
    void editorRejectsSaveWhenNothingChanged();
    void editorSuccessfullyUpdatesAStoredTask();
    void editorCreationPredecessorPickerUsesIsolatedCheckpoints();
    void editorAtomicCreationFailurePreservesTheWholeDraft();
    // 图像素布局与箭头几何属于ViewModel，拓扑语义仍取自Model快照。
    void graphProjectsNodesEdgesAndSelectionDetails();
    void graphReloadPreservesVisibleSelectionAndClearsHiddenSelection();
};

void TaskViewModelsTest::appViewModelOwnsBindableChildren()
{
    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    AppViewModel app{service};

    QCOMPARE(app.applicationName(), QStringLiteral("SmartMate"));
    QVERIFY(app.taskList() != nullptr);
    QVERIFY(app.taskEditor() != nullptr);
    QVERIFY(app.taskDependencies() != nullptr);
    QVERIFY(app.taskGraph() != nullptr);
}

void TaskViewModelsTest::listProjectsActiveAndArchivedTasksInPlanOrder()
{
    // Repository 返回顺序不是 UI 契约；ViewModel 必须保留Model给出的计划顺序。
    const Task older = task(QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
                            QStringLiteral("older"), TaskStatus::Todo, 1700000001000);
    const Task secondAtSameTime =
        task(QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
             QStringLiteral("same time second"), TaskStatus::Done, 1700000003000,
             TaskPriority::High);
    const Task firstAtSameTime =
        task(QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
             QStringLiteral("same time first"), TaskStatus::InProgress, 1700000003000,
             TaskPriority::Urgent);
    const Task archived = task(QStringLiteral("{44444444-4444-4444-4444-444444444444}"),
                               QStringLiteral("archived"), TaskStatus::Archived,
                               1700000004000);
    FakeTaskRepository repository{{older, secondAtSameTime, archived, firstAtSameTime}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskListViewModel viewModel{service};

    QCOMPARE(viewModel.count(), 3);
    QCOMPARE(idAt(viewModel, 0), QStringLiteral("11111111-1111-1111-1111-111111111111"));
    QCOMPARE(idAt(viewModel, 1), QStringLiteral("33333333-3333-3333-3333-333333333333"));
    QCOMPARE(idAt(viewModel, 2), QStringLiteral("22222222-2222-2222-2222-222222222222"));
    QCOMPARE(viewModel.data(viewModel.index(0), TaskListViewModel::PriorityTextRole).toString(),
             QStringLiteral("紧急"));
    QCOMPARE(viewModel.data(viewModel.index(0),
                            TaskListViewModel::OrderReasonTextRole).toString(),
             QStringLiteral("正在进行"));

    QSignalSpy countSpy{&viewModel, &TaskListViewModel::countChanged};
    viewModel.setShowArchived(true);

    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(idAt(viewModel, 0), QStringLiteral("44444444-4444-4444-4444-444444444444"));
    QCOMPARE(viewModel.data(viewModel.index(0),
                            TaskListViewModel::OrderReasonTextRole).toString(),
             QStringLiteral("已归档"));
}

void TaskViewModelsTest::listMapsEveryRecommendedOrderReason()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const Task inProgress = task(
        QStringLiteral("{10000000-0000-0000-0000-000000000001}"),
        QStringLiteral("in progress"), TaskStatus::InProgress, 1700000001000,
        TaskPriority::Urgent);
    const Task overdue = task(
        QStringLiteral("{20000000-0000-0000-0000-000000000002}"),
        QStringLiteral("overdue"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Urgent, QStringLiteral("description"), now.addDays(-1));
    const Task urgent = task(
        QStringLiteral("{30000000-0000-0000-0000-000000000003}"),
        QStringLiteral("urgent"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Urgent);
    const Task high = task(
        QStringLiteral("{40000000-0000-0000-0000-000000000004}"),
        QStringLiteral("high"), TaskStatus::Todo, 1700000001000,
        TaskPriority::High);
    const Task upcoming = task(
        QStringLiteral("{50000000-0000-0000-0000-000000000005}"),
        QStringLiteral("upcoming"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Normal, QStringLiteral("description"), now.addDays(1));
    const Task todo = task(
        QStringLiteral("{60000000-0000-0000-0000-000000000006}"),
        QStringLiteral("todo"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Normal);
    const Task completed = task(
        QStringLiteral("{70000000-0000-0000-0000-000000000007}"),
        QStringLiteral("completed"), TaskStatus::Done, 1700000001000);
    const Task cancelled = task(
        QStringLiteral("{80000000-0000-0000-0000-000000000008}"),
        QStringLiteral("cancelled"), TaskStatus::Cancelled, 1700000001000);
    const Task archived = task(
        QStringLiteral("{90000000-0000-0000-0000-000000000009}"),
        QStringLiteral("archived"), TaskStatus::Archived, 1700000001000);
    FakeTaskRepository repository{{archived, todo, upcoming, cancelled, high,
                                   overdue, completed, urgent, inProgress}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskListViewModel viewModel{service};

    QCOMPARE(reasonForId(viewModel, inProgress.id().toString(QUuid::WithoutBraces)),
             QStringLiteral("正在进行"));
    QCOMPARE(reasonForId(viewModel, overdue.id().toString(QUuid::WithoutBraces)),
             QStringLiteral("已逾期"));
    QCOMPARE(reasonForId(viewModel, urgent.id().toString(QUuid::WithoutBraces)),
             QStringLiteral("紧急优先"));
    QCOMPARE(reasonForId(viewModel, high.id().toString(QUuid::WithoutBraces)),
             QStringLiteral("高优先"));
    QCOMPARE(reasonForId(viewModel, upcoming.id().toString(QUuid::WithoutBraces)),
             QStringLiteral("截止较近"));
    QCOMPARE(reasonForId(viewModel, todo.id().toString(QUuid::WithoutBraces)),
             QStringLiteral("待办"));
    QCOMPARE(reasonForId(viewModel, completed.id().toString(QUuid::WithoutBraces)),
             QStringLiteral("已完成"));
    QCOMPARE(reasonForId(viewModel, cancelled.id().toString(QUuid::WithoutBraces)),
             QStringLiteral("已取消"));

    viewModel.setShowArchived(true);
    QCOMPARE(reasonForId(viewModel, archived.id().toString(QUuid::WithoutBraces)),
             QStringLiteral("已归档"));
}

void TaskViewModelsTest::listSearchesTrimmedKeywordsInTitleAndDescription()
{
    const Task titleMatch = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("Write MVVM Report"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Low, QStringLiteral("other"));
    const Task descriptionMatch = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("整理资料"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Normal, QStringLiteral("REPORT 中包含中文说明"));
    const Task noMatch = task(
        QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
        QStringLiteral("unrelated"), TaskStatus::Todo, 1700000001000,
        TaskPriority::High, QStringLiteral("nothing here"));
    const Task archivedMatch = task(
        QStringLiteral("{44444444-4444-4444-4444-444444444444}"),
        QStringLiteral("archived report"), TaskStatus::Archived, 1700000001000,
        TaskPriority::Urgent);
    FakeTaskRepository repository{{noMatch, archivedMatch, descriptionMatch, titleMatch}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskListViewModel viewModel{service};

    viewModel.setSearchText(QStringLiteral("  report  "));
    QCOMPARE(viewModel.searchText(), QStringLiteral("  report  "));
    QVERIFY(viewModel.hasActiveFilters());
    QCOMPARE(viewModel.count(), 2);
    const QSet<QString> matchingIds{idAt(viewModel, 0), idAt(viewModel, 1)};
    const QSet<QString> expectedIds{
        titleMatch.id().toString(QUuid::WithoutBraces),
        descriptionMatch.id().toString(QUuid::WithoutBraces)};
    QCOMPARE(matchingIds, expectedIds);

    viewModel.setShowArchived(true);
    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(idAt(viewModel, 0),
             archivedMatch.id().toString(QUuid::WithoutBraces));

    viewModel.setSearchText(QStringLiteral("   "));
    QCOMPARE(viewModel.count(), 1);
    QVERIFY(!viewModel.hasActiveFilters());
}

void TaskViewModelsTest::listCombinesPrioritySearchAndArchiveFilters()
{
    const Task lowReport = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("report low"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Low);
    const Task urgentReport = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("report urgent"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Urgent);
    const Task urgentOther = task(
        QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
        QStringLiteral("other urgent"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Urgent);
    const Task archivedUrgentReport = task(
        QStringLiteral("{44444444-4444-4444-4444-444444444444}"),
        QStringLiteral("archived report"), TaskStatus::Archived, 1700000001000,
        TaskPriority::Urgent);
    FakeTaskRepository repository{{urgentOther, archivedUrgentReport,
                                   lowReport, urgentReport}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskListViewModel viewModel{service};

    QCOMPARE(viewModel.priorityFilterOptions(),
             QStringList({QStringLiteral("全部优先级"), QStringLiteral("低"),
                          QStringLiteral("普通"), QStringLiteral("高"),
                          QStringLiteral("紧急")}));
    viewModel.setSearchText(QStringLiteral("report"));
    viewModel.setPriorityFilterIndex(4);
    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(idAt(viewModel, 0), urgentReport.id().toString(QUuid::WithoutBraces));

    viewModel.setShowArchived(true);
    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(idAt(viewModel, 0),
             archivedUrgentReport.id().toString(QUuid::WithoutBraces));

    viewModel.clearFilters();
    QVERIFY(viewModel.showArchived());
    QCOMPARE(viewModel.searchText(), QString{});
    QCOMPARE(viewModel.priorityFilterIndex(), 0);
    QVERIFY(!viewModel.hasActiveFilters());
    QCOMPARE(viewModel.count(), 1);
}

void TaskViewModelsTest::listFilterPropertiesNotifyOnceAndRejectInvalidIndexes()
{
    const Task stored = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("report"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Urgent);
    FakeTaskRepository repository{{stored}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskListViewModel viewModel{service};
    QSignalSpy searchSpy{&viewModel, &TaskListViewModel::searchTextChanged};
    QSignalSpy prioritySpy{&viewModel,
                           &TaskListViewModel::priorityFilterIndexChanged};
    QSignalSpy activeSpy{&viewModel, &TaskListViewModel::hasActiveFiltersChanged};
    QSignalSpy resetSpy{&viewModel, &QAbstractItemModel::modelReset};
    QSignalSpy countSpy{&viewModel, &TaskListViewModel::countChanged};

    viewModel.setSearchText(QStringLiteral("report"));
    viewModel.setSearchText(QStringLiteral("report"));
    viewModel.setPriorityFilterIndex(4);
    viewModel.setPriorityFilterIndex(4);
    viewModel.setPriorityFilterIndex(-1);
    viewModel.setPriorityFilterIndex(5);

    QCOMPARE(viewModel.priorityFilterIndex(), 4);
    QCOMPARE(searchSpy.count(), 1);
    QCOMPARE(prioritySpy.count(), 1);
    QCOMPARE(activeSpy.count(), 1);
    QCOMPARE(resetSpy.count(), 2);
    QCOMPARE(countSpy.count(), 2);

    // 分钟刷新读取到相同计划时不得无意义重置列表。
    viewModel.reload();
    QCOMPARE(resetSpy.count(), 2);
    QCOMPARE(countSpy.count(), 2);

    viewModel.clearFilters();
    viewModel.clearFilters();
    QCOMPARE(searchSpy.count(), 2);
    QCOMPARE(prioritySpy.count(), 2);
    QCOMPARE(activeSpy.count(), 2);
    QCOMPARE(resetSpy.count(), 3);
    QCOMPARE(countSpy.count(), 3);
}

void TaskViewModelsTest::listRetainsFiltersAndStableTaskIdAcrossServiceReloads()
{
    const Task stored = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("needle task"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Urgent);
    const Task other = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("other"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Urgent);
    FakeTaskRepository repository{{other, stored}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskListViewModel viewModel{service};

    viewModel.setSearchText(QStringLiteral("needle"));
    viewModel.setPriorityFilterIndex(4);
    QCOMPARE(viewModel.count(), 1);
    const QString stableId = idAt(viewModel, 0);

    QVERIFY(viewModel.archiveTask(stableId));
    QCOMPARE(viewModel.searchText(), QStringLiteral("needle"));
    QCOMPARE(viewModel.priorityFilterIndex(), 4);
    QCOMPARE(viewModel.count(), 0);

    viewModel.setShowArchived(true);
    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(idAt(viewModel, 0), stableId);
    QCOMPARE(reasonForId(viewModel, stableId), QStringLiteral("已归档"));
    QVERIFY(viewModel.restoreTask(stableId));
    QCOMPARE(viewModel.count(), 0);

    viewModel.setShowArchived(false);
    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(idAt(viewModel, 0), stableId);
}

void TaskViewModelsTest::listSchedulesMinuteRefreshForTimeSensitiveReasons()
{
    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskListViewModel viewModel{service};

    const auto timers = viewModel.findChildren<QTimer *>(
        QString{}, Qt::FindDirectChildrenOnly);
    QCOMPARE(timers.size(), 1);
    QCOMPARE(timers.constFirst()->interval(), 60'000);
    QVERIFY(timers.constFirst()->isActive());
}

void TaskViewModelsTest::listArchivesAndRestoresByStableTaskId()
{
    // 排序或筛选改变行号后，归档与恢复仍必须只依赖稳定 TaskId。
    const Task stored = task(QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
                             QStringLiteral("archive me"), TaskStatus::Todo,
                             1700000001000);
    FakeTaskRepository repository{{stored}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskListViewModel viewModel{service};

    QVERIFY(viewModel.archiveTask(idAt(viewModel, 0)));
    QCOMPARE(viewModel.count(), 0);
    QCOMPARE(repository.findById(stored.id())->status(), TaskStatus::Archived);

    viewModel.setShowArchived(true);
    QCOMPARE(viewModel.count(), 1);
    QVERIFY(viewModel.restoreTask(idAt(viewModel, 0)));
    QCOMPARE(viewModel.count(), 0);

    viewModel.setShowArchived(false);
    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(repository.findById(stored.id())->status(), TaskStatus::Todo);
}

void TaskViewModelsTest::listExposesAndClearsChineseErrors()
{
    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskListViewModel viewModel{service};
    QSignalSpy errorSpy{&viewModel, &TaskListViewModel::errorOccurred};

    repository.setReadFailure(true);
    viewModel.reload();

    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(!viewModel.errorMessage().isEmpty());
    QVERIFY(viewModel.errorMessage().contains(QStringLiteral("数据")));

    viewModel.clearError();
    QVERIFY(viewModel.errorMessage().isEmpty());
}

void TaskViewModelsTest::listProjectsBlockingAndUnlockInformation()
{
    const Task predecessor = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("需求分析"), TaskStatus::Todo, 1700000001000);
    const Task successor = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("编码"), TaskStatus::Todo, 1700000002000);
    const Task completed = task(
        QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
        QStringLiteral("已完成"), TaskStatus::Done, 1700000003000);
    FakeTaskRepository repository{{successor, completed, predecessor}};
    FakeTaskDependencyRepository dependencyRepository{{
        TaskDependency{predecessor.id(), successor.id()},
    }};
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskListViewModel viewModel{service};

    const int predecessorRow = rowForId(
        viewModel, predecessor.id().toString(QUuid::WithoutBraces));
    const int successorRow = rowForId(
        viewModel, successor.id().toString(QUuid::WithoutBraces));
    const int completedRow = rowForId(
        viewModel, completed.id().toString(QUuid::WithoutBraces));
    QVERIFY(predecessorRow >= 0);
    QVERIFY(successorRow >= 0);
    QVERIFY(completedRow >= 0);

    QCOMPARE(viewModel.data(viewModel.index(successorRow),
                            TaskListViewModel::BlockedRole).toBool(), true);
    QVERIFY(viewModel.data(viewModel.index(successorRow),
                           TaskListViewModel::BlockingReasonTextRole).toString()
                .contains(QStringLiteral("需求分析")));
    QCOMPARE(viewModel.data(viewModel.index(successorRow),
                            TaskListViewModel::PredecessorCountRole).toInt(), 1);
    QCOMPARE(viewModel.data(viewModel.index(predecessorRow),
                            TaskListViewModel::UnlockCountRole).toInt(), 1);
    QCOMPARE(viewModel.data(viewModel.index(successorRow),
                            TaskListViewModel::CanEditDependenciesRole).toBool(), true);
    QCOMPARE(viewModel.data(viewModel.index(completedRow),
                            TaskListViewModel::CanEditDependenciesRole).toBool(), false);

    QVERIFY(service.replaceTaskPredecessors(successor.id(), {}).ok());
    const int refreshedRow = rowForId(
        viewModel, successor.id().toString(QUuid::WithoutBraces));
    QCOMPARE(viewModel.data(viewModel.index(refreshedRow),
                            TaskListViewModel::BlockedRole).toBool(), false);
    QCOMPARE(viewModel.data(viewModel.index(refreshedRow),
                            TaskListViewModel::PredecessorCountRole).toInt(), 0);
}

void TaskViewModelsTest::dependencyDraftCancelDoesNotChangeModel()
{
    const Task target = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("目标任务"), TaskStatus::Todo, 1700000001000);
    const Task candidate = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("普通候选"), TaskStatus::Todo, 1700000002000,
        TaskPriority::High);
    const Task completed = task(
        QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
        QStringLiteral("完成候选"), TaskStatus::Done, 1700000003000);
    const Task selectedArchived = task(
        QStringLiteral("{44444444-4444-4444-4444-444444444444}"),
        QStringLiteral("原有归档前置"), TaskStatus::Archived, 1700000004000);
    const Task unselectedArchived = task(
        QStringLiteral("{55555555-5555-5555-5555-555555555555}"),
        QStringLiteral("不可新增归档"), TaskStatus::Archived, 1700000005000);
    FakeTaskRepository repository{{target, candidate, completed,
                                   selectedArchived, unselectedArchived}};
    FakeTaskDependencyRepository dependencyRepository{{
        TaskDependency{selectedArchived.id(), target.id()},
    }};
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskDependencyViewModel editor{service};

    QVERIFY(editor.beginEdit(target.id().toString(QUuid::WithoutBraces)));
    QCOMPARE(editor.count(), 3);
    QCOMPARE(editor.selectedCount(), 1);
    QVERIFY(!editor.dirty());

    const QString candidateId = candidate.id().toString(QUuid::WithoutBraces);
    const int candidateRow = dependencyRowForId(editor, candidateId);
    const int archivedRow = dependencyRowForId(
        editor, selectedArchived.id().toString(QUuid::WithoutBraces));
    QVERIFY(candidateRow >= 0);
    QVERIFY(archivedRow >= 0);
    QCOMPARE(editor.data(editor.index(candidateRow),
                         TaskDependencyViewModel::ShortIdRole).toString(),
             QStringLiteral("22222222"));
    QCOMPARE(editor.data(editor.index(candidateRow),
                         TaskDependencyViewModel::PriorityTextRole).toString(),
             QStringLiteral("高"));
    QCOMPARE(editor.data(editor.index(archivedRow),
                         TaskDependencyViewModel::SelectedRole).toBool(), true);
    QCOMPARE(editor.data(editor.index(archivedRow),
                         TaskDependencyViewModel::SelectableRole).toBool(), true);

    QVERIFY(editor.setPredecessorSelected(candidateId, true));
    QVERIFY(editor.dirty());
    QCOMPARE(editor.selectedCount(), 2);
    editor.cancel();

    QVERIFY(!editor.dirty());
    QCOMPARE(editor.selectedCount(), 1);
    QCOMPARE(dependencyRepository.replaceCount(), 0);
    const QList<TaskDependency> expectedDependencies{
        TaskDependency{selectedArchived.id(), target.id()},
    };
    QCOMPARE(dependencyRepository.dependencies(), expectedDependencies);
}

void TaskViewModelsTest::dependencyDraftSavesStableTaskIds()
{
    const Task target = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("目标任务"), TaskStatus::Todo, 1700000001000);
    const Task first = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("第一前置"), TaskStatus::Todo, 1700000002000);
    const Task second = task(
        QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
        QStringLiteral("第二前置"), TaskStatus::Done, 1700000003000);
    FakeTaskRepository repository{{second, target, first}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskDependencyViewModel editor{service};
    QSignalSpy savedSpy{&editor, &TaskDependencyViewModel::saved};

    QVERIFY(editor.beginEdit(target.id().toString(QUuid::WithoutBraces)));
    QVERIFY(editor.setPredecessorSelected(
        first.id().toString(QUuid::WithoutBraces), true));
    QVERIFY(editor.setPredecessorSelected(
        second.id().toString(QUuid::WithoutBraces), true));
    QVERIFY(editor.save());

    QCOMPARE(savedSpy.count(), 1);
    QCOMPARE(dependencyRepository.replaceCount(), 1);
    QCOMPARE(dependencyRepository.dependencies().size(), 2);
    QVERIFY(std::any_of(dependencyRepository.dependencies().cbegin(),
                        dependencyRepository.dependencies().cend(),
                        [&first, &target](const TaskDependency &dependency) {
                            return dependency.predecessorId == first.id()
                                && dependency.successorId == target.id();
                        }));
    QVERIFY(std::any_of(dependencyRepository.dependencies().cbegin(),
                        dependencyRepository.dependencies().cend(),
                        [&second, &target](const TaskDependency &dependency) {
                            return dependency.predecessorId == second.id()
                                && dependency.successorId == target.id();
                        }));
    QVERIFY(!editor.dirty());
    QVERIFY(!editor.canSave());
}

void TaskViewModelsTest::dependencyDraftMapsCompleteCyclePath()
{
    const Task first = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("需求"), TaskStatus::Todo, 1700000001000);
    const Task second = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("设计"), TaskStatus::Archived, 1700000002000);
    const Task third = task(
        QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
        QStringLiteral("编码"), TaskStatus::Todo, 1700000003000);
    FakeTaskRepository repository{{first, second, third}};
    FakeTaskDependencyRepository dependencyRepository{{
        TaskDependency{first.id(), second.id()},
        TaskDependency{second.id(), third.id()},
    }};
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskDependencyViewModel editor{service};

    QVERIFY(editor.beginEdit(first.id().toString(QUuid::WithoutBraces)));
    QVERIFY(editor.setPredecessorSelected(
        third.id().toString(QUuid::WithoutBraces), true));
    QVERIFY(!editor.save());

    QVERIFY(editor.errorMessage().contains(QStringLiteral("循环依赖")));
    QVERIFY(editor.errorMessage().contains(QStringLiteral("需求")));
    QVERIFY(editor.errorMessage().contains(QStringLiteral("设计")));
    QVERIFY(editor.errorMessage().contains(QStringLiteral("编码")));
    QVERIFY(editor.errorMessage().contains(QStringLiteral("→")));
    QCOMPARE(dependencyRepository.replaceCount(), 0);
    QVERIFY(editor.dirty());
}

void TaskViewModelsTest::editorCreatesACompleteTypedDraft()
{
    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    const QTimeZone timeZone = QTimeZone::fromSecondsAheadOfUtc(8 * 60 * 60);
    TaskEditorViewModel editor{service, timeZone};

    editor.beginCreate();
    QCOMPARE(editor.statusOptions().size(), 5);
    QCOMPARE(editor.priorityOptions().size(), 4);
    QVERIFY(!editor.canSave());

    editor.setTitle(QStringLiteral("  MVVM 大作业  "));
    editor.setDescription(QStringLiteral("实现任务编辑草稿"));
    editor.setStatusIndex(static_cast<int>(TaskStatus::Done));
    editor.setPriorityIndex(static_cast<int>(TaskPriority::Urgent));
    QVERIFY(editor.setDeadlineSelection(2030, 6, 15, 8, 30));
    QVERIFY(editor.setEstimatedDuration(0, 1, 30));
    QVERIFY(editor.hasDeadline());
    QCOMPARE(editor.deadlineDisplayText(), QStringLiteral("2030-06-15 08:30"));
    QCOMPARE(editor.deadlineYear(), 2030);
    QCOMPARE(editor.deadlineMonth(), 6);
    QCOMPARE(editor.deadlineDay(), 15);
    QCOMPARE(editor.deadlineHour(), 8);
    QCOMPARE(editor.deadlineMinute(), 30);
    QVERIFY(editor.hasEstimatedDuration());
    QCOMPARE(editor.estimatedDurationDisplayText(), QStringLiteral("1小时 30分钟"));
    QVERIFY(editor.dirty());
    QVERIFY(editor.canSave());

    QSignalSpy savedSpy{&editor, &TaskEditorViewModel::saved};
    QVERIFY(editor.save());

    QCOMPARE(savedSpy.count(), 1);
    QCOMPARE(repository.tasks().size(), 1);
    const Task &created = repository.tasks().constFirst();
    QCOMPARE(created.title(), QStringLiteral("MVVM 大作业"));
    QCOMPARE(created.description(), QStringLiteral("实现任务编辑草稿"));
    QCOMPARE(created.status(), TaskStatus::Done);
    QCOMPARE(created.priority(), TaskPriority::Urgent);
    QCOMPARE(created.estimatedMinutes(), std::optional<int>{90});
    QCOMPARE(created.deadline()->toTimeZone(timeZone).toString(
                 QStringLiteral("yyyy-MM-dd HH:mm")),
             QStringLiteral("2030-06-15 08:30"));
    QVERIFY(!editor.dirty());
    QVERIFY(!editor.canSave());
}

void TaskViewModelsTest::editorCancelLeavesTheStoredTaskUnchanged()
{
    // 取消时 Repository 保持原值，直接证明编辑草稿与领域实体相互隔离。
    const Task stored = task(QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
                             QStringLiteral("original"), TaskStatus::Todo,
                             1700000001000);
    FakeTaskRepository repository{{stored}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskEditorViewModel editor{service};

    QVERIFY(editor.beginEdit(stored.id().toString(QUuid::WithoutBraces)));
    editor.setTitle(QStringLiteral("unsaved change"));
    QVERIFY(editor.dirty());
    QSignalSpy cancelledSpy{&editor, &TaskEditorViewModel::cancelled};

    editor.cancel();

    QCOMPARE(cancelledSpy.count(), 1);
    QCOMPARE(repository.findById(stored.id())->title(), QStringLiteral("original"));
}

void TaskViewModelsTest::editorRejectsInvalidSelectionsAndMapsServiceErrors()
{
    const Task active = task(QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
                             QStringLiteral("active"), TaskStatus::InProgress,
                             1700000001000);
    FakeTaskRepository repository{{active}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskEditorViewModel editor{service};

    editor.beginCreate();
    editor.setTitle(QStringLiteral("second active"));
    QSignalSpy deadlineSpy{&editor, &TaskEditorViewModel::deadlineChanged};
    QSignalSpy durationSpy{&editor, &TaskEditorViewModel::estimatedDurationChanged};

    // 命令入口会防御无效选择；失败既不产生通知，也不污染原草稿。
    QVERIFY(!editor.setDeadlineSelection(2030, 2, 30, 8, 5));
    QVERIFY(!editor.hasDeadline());
    QCOMPARE(deadlineSpy.count(), 0);
    QVERIFY(!editor.setEstimatedDuration(0, 0, 0));
    QVERIFY(!editor.hasEstimatedDuration());
    QCOMPARE(durationSpy.count(), 0);

    QVERIFY(editor.setDeadlineSelection(2030, 2, 3, 8, 5));
    QVERIFY(editor.setEstimatedDuration(0, 0, 30));
    editor.setStatusIndex(static_cast<int>(TaskStatus::InProgress));
    QVERIFY(editor.canSave());
    QVERIFY(!editor.save());
    QVERIFY(editor.errorMessage().contains(QStringLiteral("已有任务")));

    editor.setStatusIndex(static_cast<int>(TaskStatus::Todo));
    QVERIFY(editor.errorMessage().isEmpty());
    QVERIFY(editor.save());
}

void TaskViewModelsTest::editorSupportsDurationBoundariesAndClear()
{
    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskEditorViewModel editor{service};

    editor.beginCreate();
    QCOMPARE(editor.minimumEstimatedMinutes(), 1);
    QCOMPARE(editor.maximumEstimatedMinutes(), 525600);
    QCOMPARE(editor.estimatedDurationDisplayText(), QStringLiteral("未设置"));

    QSignalSpy durationSpy{&editor, &TaskEditorViewModel::estimatedDurationChanged};
    QVERIFY(editor.setEstimatedDuration(0, 0, 1));
    QCOMPARE(editor.estimatedDays(), 0);
    QCOMPARE(editor.estimatedHours(), 0);
    QCOMPARE(editor.estimatedMinutePart(), 1);

    QVERIFY(editor.setEstimatedDuration(2, 3, 4));
    QCOMPARE(editor.estimatedDays(), 2);
    QCOMPARE(editor.estimatedHours(), 3);
    QCOMPARE(editor.estimatedMinutePart(), 4);
    QCOMPARE(editor.estimatedDurationDisplayText(), QStringLiteral("2天 3小时 4分钟"));

    QVERIFY(editor.setEstimatedDuration(365, 0, 0));
    QCOMPARE(editor.estimatedDays(), 365);
    QCOMPARE(editor.estimatedHours(), 0);
    QCOMPARE(editor.estimatedMinutePart(), 0);
    QVERIFY(!editor.setEstimatedDuration(365, 0, 1));
    QCOMPARE(editor.estimatedDays(), 365);

    editor.clearEstimatedDuration();
    QVERIFY(!editor.hasEstimatedDuration());
    QCOMPARE(editor.estimatedDurationDisplayText(), QStringLiteral("未设置"));
    QCOMPARE(durationSpy.count(), 4);
}

void TaskViewModelsTest::editorConvertsInjectedTimeZoneAndRejectsDstTransitions()
{
    const QTimeZone newYork{QByteArray{"America/New_York"}};
    if (!newYork.isValid()) {
        QSKIP("The test platform does not provide the America/New_York time zone.");
    }

    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskEditorViewModel editor{service, newYork};
    editor.beginCreate();
    editor.setTitle(QStringLiteral("DST-safe deadline"));
    QSignalSpy deadlineSpy{&editor, &TaskEditorViewModel::deadlineChanged};

    QVERIFY(editor.setDeadlineSelection(2024, 2, 29, 23, 59));
    QCOMPARE(editor.deadlineDisplayText(), QStringLiteral("2024-02-29 23:59"));

    // 春季跳时不存在，秋季回拨时间有歧义；两者都不能替换已选截止时间。
    QVERIFY(!editor.setDeadlineSelection(2024, 3, 10, 2, 30));
    QVERIFY(!editor.setDeadlineSelection(2024, 11, 3, 1, 30));
    QCOMPARE(editor.deadlineDisplayText(), QStringLiteral("2024-02-29 23:59"));
    QCOMPARE(deadlineSpy.count(), 1);

    QVERIFY(editor.save());
    const QDateTime expectedUtc{QDate{2024, 3, 1}, QTime{4, 59}, QTimeZone::UTC};
    QCOMPARE(repository.tasks().constFirst().deadline(),
             std::optional<QDateTime>{expectedUtc});
}

void TaskViewModelsTest::editorPreservesUnchangedDeadlinePrecision()
{
    const QDateTime preciseDeadline{QDate{2032, 5, 6},
                                    QTime{7, 8, 45, 678},
                                    QTimeZone::UTC};
    const Task stored{QUuid::fromString(
                          QStringLiteral("{11111111-1111-1111-1111-111111111111}")),
                      QStringLiteral("preserve precision"),
                      QStringLiteral("description"),
                      TaskPriority::Normal,
                      TaskStatus::Todo,
                      std::nullopt,
                      preciseDeadline,
                      30,
                      utcTime(1700000000000),
                      utcTime(1700000001000)};
    FakeTaskRepository repository{{stored}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskEditorViewModel editor{service, QTimeZone{QTimeZone::UTC}};

    QVERIFY(editor.beginEdit(stored.id().toString(QUuid::WithoutBraces)));
    editor.setTitle(QStringLiteral("title changed only"));
    QVERIFY(editor.save());
    QCOMPARE(repository.findById(stored.id())->deadline(),
             std::optional<QDateTime>{preciseDeadline});

    // 用户明确重新选择后，选择器精度为分钟，秒和毫秒应归零。
    QVERIFY(editor.setDeadlineSelection(2032, 5, 6, 7, 8));
    QVERIFY(editor.save());
    QCOMPARE(repository.findById(stored.id())->deadline()->time(), QTime(7, 8));
}

void TaskViewModelsTest::editorRejectsSaveWhenNothingChanged()
{
    // 即使绕过 QML 按钮直接调用命令，无改动草稿也不能触发持久化。
    const Task stored = task(QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
                             QStringLiteral("unchanged"), TaskStatus::Todo,
                             1700000001000);
    FakeTaskRepository repository{{stored}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskEditorViewModel editor{service};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};
    QSignalSpy savedSpy{&editor, &TaskEditorViewModel::saved};

    QVERIFY(editor.beginEdit(stored.id().toString(QUuid::WithoutBraces)));
    QVERIFY(!editor.dirty());
    QVERIFY(!editor.canSave());
    QVERIFY(!editor.save());

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(savedSpy.count(), 0);
    QCOMPARE(repository.findById(stored.id())->title(), QStringLiteral("unchanged"));
    QCOMPARE(editor.errorMessage(), QStringLiteral("没有需要保存的更改。"));
}

void TaskViewModelsTest::editorSuccessfullyUpdatesAStoredTask()
{
    const Task stored = task(QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
                             QStringLiteral("before"), TaskStatus::Todo,
                             1700000001000);
    FakeTaskRepository repository{{stored}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    const QTimeZone timeZone = QTimeZone::fromSecondsAheadOfUtc(8 * 60 * 60);
    TaskEditorViewModel editor{service, timeZone};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};
    QSignalSpy savedSpy{&editor, &TaskEditorViewModel::saved};

    QVERIFY(editor.beginEdit(stored.id().toString(QUuid::WithoutBraces)));
    editor.setTitle(QStringLiteral("after"));
    editor.setDescription(QStringLiteral("updated description"));
    editor.setPriorityIndex(static_cast<int>(TaskPriority::High));
    QVERIFY(editor.setDeadlineSelection(2031, 3, 4, 9, 45));
    QVERIFY(editor.setEstimatedDuration(0, 2, 0));
    QVERIFY(editor.canSave());

    QVERIFY(editor.save());

    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(savedSpy.count(), 1);
    const auto updated = repository.findById(stored.id());
    QVERIFY(updated.has_value());
    QCOMPARE(updated->title(), QStringLiteral("after"));
    QCOMPARE(updated->description(), QStringLiteral("updated description"));
    QCOMPARE(updated->priority(), TaskPriority::High);
    QCOMPARE(updated->estimatedMinutes(), std::optional<int>{120});
    QCOMPARE(updated->deadline()->toTimeZone(timeZone).toString(
                 QStringLiteral("yyyy-MM-dd HH:mm")),
             QStringLiteral("2031-03-04 09:45"));
    QVERIFY(!editor.dirty());
    QVERIFY(!editor.canSave());
    QVERIFY(editor.errorMessage().isEmpty());
}

void TaskViewModelsTest::editorCreationPredecessorPickerUsesIsolatedCheckpoints()
{
    const Task active = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("需求确认"), TaskStatus::Todo, 1700000001000);
    const Task archived = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("已归档候选"), TaskStatus::Archived, 1700000002000);
    FakeTaskRepository repository{{archived, active}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskEditorViewModel editor{service};

    QVERIFY(editor.beginCreate());
    QCOMPARE(editor.predecessorCandidateCount(), 1);
    QCOMPARE(editor.data(editor.index(0), TaskEditorViewModel::CandidateTaskIdRole)
                 .toString(),
             active.id().toString(QUuid::WithoutBraces));
    QVERIFY(editor.canConfigurePredecessors());

    // 弹窗勾选只改变工作副本，清空后取消必须恢复打开前的主草稿选择。
    editor.beginPredecessorSelection();
    QVERIFY(editor.setCreationPredecessorSelected(
        active.id().toString(QUuid::WithoutBraces), true));
    QCOMPARE(editor.selectedPredecessorCount(), 1);
    QVERIFY(editor.data(editor.index(0), TaskEditorViewModel::CandidateSelectedRole)
                .toBool());
    editor.clearCreationPredecessors();
    QCOMPARE(editor.selectedPredecessorCount(), 0);
    editor.cancelPredecessorSelection();
    QCOMPARE(editor.selectedPredecessorCount(), 0);

    editor.beginPredecessorSelection();
    QVERIFY(editor.setCreationPredecessorSelected(
        active.id().toString(QUuid::WithoutBraces), true));
    editor.acceptPredecessorSelection();
    QCOMPARE(editor.selectedPredecessorCount(), 1);
    QCOMPARE(editor.predecessorSummaryText(), QStringLiteral("已选择 1 项"));
    editor.setTitle(QStringLiteral("实现任务页面"));
    QVERIFY(editor.canSave());

    // ViewModel提供即时提示，但最终同一规则仍由TaskService权威校验。
    editor.setStatusIndex(static_cast<int>(TaskStatus::Done));
    QVERIFY(!editor.canSave());
    QVERIFY(editor.validationMessage().contains(QStringLiteral("必须为待办")));
    QCOMPARE(editor.selectedPredecessorCount(), 1);
    editor.setStatusIndex(static_cast<int>(TaskStatus::Todo));
    QVERIFY(editor.canSave());

    editor.cancel();
    QCOMPARE(editor.predecessorCandidateCount(), 0);
    QCOMPARE(editor.selectedPredecessorCount(), 0);
    QVERIFY(editor.title().isEmpty());
    QVERIFY(!editor.dirty());
}

void TaskViewModelsTest::editorAtomicCreationFailurePreservesTheWholeDraft()
{
    const Task predecessor = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("接口设计"), TaskStatus::Todo, 1700000001000);
    FakeTaskRepository repository{{predecessor}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskEditorViewModel editor{service};
    QSignalSpy savedSpy{&editor, &TaskEditorViewModel::saved};

    QVERIFY(editor.beginCreate());
    editor.setTitle(QStringLiteral("实现接口"));
    editor.beginPredecessorSelection();
    QVERIFY(editor.setCreationPredecessorSelected(
        predecessor.id().toString(QUuid::WithoutBraces), true));
    editor.acceptPredecessorSelection();
    creationRepository.setWriteFailure(true);

    QVERIFY(!editor.save());
    QCOMPARE(repository.tasks().size(), 1);
    QVERIFY(dependencyRepository.dependencies().isEmpty());
    QCOMPARE(editor.title(), QStringLiteral("实现接口"));
    QCOMPARE(editor.selectedPredecessorCount(), 1);
    QVERIFY(editor.dirty());
    QCOMPARE(savedSpy.count(), 0);

    creationRepository.setWriteFailure(false);
    QVERIFY(editor.save());
    QCOMPARE(repository.tasks().size(), 2);
    QCOMPARE(dependencyRepository.dependencies().size(), 1);
    QCOMPARE(savedSpy.count(), 1);
}

void TaskViewModelsTest::graphProjectsNodesEdgesAndSelectionDetails()
{
    const Task predecessor = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("需求确认"), TaskStatus::Todo, 1700000001000);
    const Task successor = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("接口实现"), TaskStatus::Todo, 1700000002000,
        TaskPriority::High);
    const Task isolated = task(
        QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
        QStringLiteral("独立任务"), TaskStatus::Done, 1700000003000);
    FakeTaskRepository repository{{isolated, successor, predecessor}};
    FakeTaskDependencyRepository dependencyRepository{{
        {predecessor.id(), successor.id()},
    }};
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskGraphViewModel graph{service};

    QCOMPARE(graph.rowCount(), 3);
    QVERIFY(!graph.empty());
    QVERIFY(graph.contentWidth() > 0.0);
    QVERIFY(graph.contentHeight() > 0.0);
    const int predecessorRow = graphRowForId(
        graph, predecessor.id().toString(QUuid::WithoutBraces));
    const int successorRow = graphRowForId(
        graph, successor.id().toString(QUuid::WithoutBraces));
    QVERIFY(predecessorRow >= 0);
    QVERIFY(successorRow >= 0);

    const qreal predecessorX = graph.data(
        graph.index(predecessorRow), TaskGraphViewModel::NodeXRole).toReal();
    const qreal successorX = graph.data(
        graph.index(successorRow), TaskGraphViewModel::NodeXRole).toReal();
    QVERIFY(successorX > predecessorX);
    QCOMPARE(graph.data(graph.index(successorRow),
                        TaskGraphViewModel::BlockedRole).toBool(), true);

    // 层级投影后所有节点矩形都不得重叠，QML无需再次执行布局算法。
    for (int first = 0; first < graph.rowCount(); ++first) {
        const QRectF firstRect{
            graph.data(graph.index(first), TaskGraphViewModel::NodeXRole).toReal(),
            graph.data(graph.index(first), TaskGraphViewModel::NodeYRole).toReal(),
            graph.data(graph.index(first), TaskGraphViewModel::NodeWidthRole).toReal(),
            graph.data(graph.index(first), TaskGraphViewModel::NodeHeightRole).toReal(),
        };
        for (int second = first + 1; second < graph.rowCount(); ++second) {
            const QRectF secondRect{
                graph.data(graph.index(second), TaskGraphViewModel::NodeXRole).toReal(),
                graph.data(graph.index(second), TaskGraphViewModel::NodeYRole).toReal(),
                graph.data(graph.index(second), TaskGraphViewModel::NodeWidthRole).toReal(),
                graph.data(graph.index(second), TaskGraphViewModel::NodeHeightRole).toReal(),
            };
            QVERIFY(!firstRect.intersects(secondRect));
        }
    }

    QAbstractItemModel *edges = graph.edges();
    QCOMPARE(edges->rowCount(), 1);
    const int startXRole = roleForName(*edges, QByteArrayLiteral("startX"));
    const int endXRole = roleForName(*edges, QByteArrayLiteral("endX"));
    const int arrowTipXRole = roleForName(*edges, QByteArrayLiteral("arrowTipX"));
    const int satisfiedRole = roleForName(*edges, QByteArrayLiteral("satisfied"));
    const int highlightedRole = roleForName(*edges, QByteArrayLiteral("highlighted"));
    QVERIFY(startXRole >= 0 && endXRole >= 0 && arrowTipXRole >= 0);
    QVERIFY(edges->data(edges->index(0, 0), endXRole).toReal()
            > edges->data(edges->index(0, 0), startXRole).toReal());
    QCOMPARE(edges->data(edges->index(0, 0), arrowTipXRole),
             edges->data(edges->index(0, 0), endXRole));
    QVERIFY(!edges->data(edges->index(0, 0), satisfiedRole).toBool());

    QVERIFY(graph.selectTask(successor.id().toString(QUuid::WithoutBraces)));
    QCOMPARE(graph.selectedTaskTitle(), QStringLiteral("接口实现"));
    QCOMPARE(graph.selectedStatusText(), QStringLiteral("待办"));
    QCOMPARE(graph.selectedPriorityText(), QStringLiteral("高"));
    QCOMPARE(graph.selectedPredecessorCount(), 1);
    QCOMPARE(graph.selectedSuccessorCount(), 0);
    QVERIFY(graph.canEditSelectedDependencies());
    QVERIFY(graph.selectedBlockingReason().contains(QStringLiteral("需求确认")));
    QVERIFY(edges->data(edges->index(0, 0), highlightedRole).toBool());
}

void TaskViewModelsTest::graphReloadPreservesVisibleSelectionAndClearsHiddenSelection()
{
    const Task predecessor = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("A"), TaskStatus::Todo, 1700000001000);
    const Task successor = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("B"), TaskStatus::Todo, 1700000002000);
    const Task isolated = task(
        QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
        QStringLiteral("C"), TaskStatus::Done, 1700000003000);
    FakeTaskRepository repository{{predecessor, successor, isolated}};
    FakeTaskDependencyRepository dependencyRepository{{
        {predecessor.id(), successor.id()},
    }};
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository};
    TaskGraphViewModel graph{service};

    const QString successorId = successor.id().toString(QUuid::WithoutBraces);
    QVERIFY(graph.selectTask(successorId));
    graph.reload();
    QCOMPARE(graph.selectedTaskId(), successorId);

    // 先归档前置时组件仍有活动后继，因此选中节点应继续可见。
    QVERIFY(service.archiveTask(predecessor.id()).ok());
    QCOMPARE(graph.selectedTaskId(), successorId);
    // 两端都归档后成为纯归档组件，Model隐藏该组件且ViewModel清除过期选择。
    QVERIFY(service.archiveTask(successor.id()).ok());
    QVERIFY(graph.selectedTaskId().isEmpty());
    QCOMPARE(graph.rowCount(), 1);
    QCOMPARE(graph.data(graph.index(0), TaskGraphViewModel::TitleRole).toString(),
             QStringLiteral("C"));
}

QTEST_GUILESS_MAIN(TaskViewModelsTest)

#include "tst_TaskViewModels.moc"
