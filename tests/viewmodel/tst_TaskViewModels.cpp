#include "AppViewModel.h"
#include "TaskEditorViewModel.h"
#include "TaskListViewModel.h"
#include "fakes/FakeTaskRepository.h"

#include "domain/Task.h"
#include "services/TaskService.h"

#include <QDateTime>
#include <QSet>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QTimeZone>

#include <optional>
#include <utility>

using smartmate::model::Task;
using smartmate::model::TaskPriority;
using smartmate::model::TaskService;
using smartmate::model::TaskStatus;
using smartmate::tests::FakeTaskRepository;
using smartmate::viewmodel::AppViewModel;
using smartmate::viewmodel::TaskEditorViewModel;
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
    // 编辑草稿、命令入口与业务校验委托。
    void editorCreatesACompleteTypedDraft();
    void editorCancelLeavesTheStoredTaskUnchanged();
    void editorRejectsInvalidSelectionsAndMapsServiceErrors();
    void editorSupportsDurationBoundariesAndClear();
    void editorConvertsInjectedTimeZoneAndRejectsDstTransitions();
    void editorPreservesUnchangedDeadlinePrecision();
    void editorRejectsSaveWhenNothingChanged();
    void editorSuccessfullyUpdatesAStoredTask();
};

void TaskViewModelsTest::appViewModelOwnsBindableChildren()
{
    FakeTaskRepository repository;
    TaskService service{repository};
    AppViewModel app{service};

    QCOMPARE(app.applicationName(), QStringLiteral("SmartMate"));
    QVERIFY(app.taskList() != nullptr);
    QVERIFY(app.taskEditor() != nullptr);
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
    TaskService service{repository};
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
    TaskService service{repository};
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
    TaskService service{repository};
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
    TaskService service{repository};
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
    TaskService service{repository};
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
    TaskService service{repository};
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
    TaskService service{repository};
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
    TaskService service{repository};
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
    TaskService service{repository};
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

void TaskViewModelsTest::editorCreatesACompleteTypedDraft()
{
    FakeTaskRepository repository;
    TaskService service{repository};
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
    TaskService service{repository};
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
    TaskService service{repository};
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
    TaskService service{repository};
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
    TaskService service{repository};
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
    TaskService service{repository};
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
    TaskService service{repository};
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
    TaskService service{repository};
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

QTEST_GUILESS_MAIN(TaskViewModelsTest)

#include "tst_TaskViewModels.moc"
