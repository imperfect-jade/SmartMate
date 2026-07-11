#include "AppViewModel.h"
#include "TaskEditorViewModel.h"
#include "TaskListViewModel.h"
#include "fakes/FakeTaskRepository.h"

#include "domain/Task.h"
#include "services/TaskService.h"

#include <QDateTime>
#include <QSignalSpy>
#include <QTest>
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
                        const TaskPriority priority = TaskPriority::Normal)
{
    return Task{QUuid::fromString(id),
                std::move(title),
                QStringLiteral("description"),
                priority,
                status,
                status == TaskStatus::Archived
                    ? std::optional<TaskStatus>{TaskStatus::Todo}
                    : std::nullopt,
                std::nullopt,
                30,
                utcTime(1700000000000),
                utcTime(updatedMilliseconds)};
}

[[nodiscard]] QString idAt(const TaskListViewModel &viewModel, const int row)
{
    return viewModel.data(viewModel.index(row), TaskListViewModel::TaskIdRole).toString();
}

} // namespace

// 测试链路使用 FakeRepository -> TaskService -> ViewModel，既覆盖可绑定状态，
// 又不依赖 QML 窗口或真实数据库。
class TaskViewModelsTest final : public QObject {
    Q_OBJECT

private slots:
    // 组合关系与列表投影。
    void appViewModelOwnsBindableChildren();
    void listProjectsActiveAndArchivedTasksInStableOrder();
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

void TaskViewModelsTest::listProjectsActiveAndArchivedTasksInStableOrder()
{
    // Repository 返回顺序不是 UI 契约；ViewModel 必须自行形成确定性投影。
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
    QCOMPARE(idAt(viewModel, 1), QStringLiteral("22222222-2222-2222-2222-222222222222"));
    QCOMPARE(idAt(viewModel, 2), QStringLiteral("33333333-3333-3333-3333-333333333333"));
    QCOMPARE(viewModel.data(viewModel.index(0), TaskListViewModel::PriorityTextRole).toString(),
             QStringLiteral("紧急"));

    QSignalSpy countSpy{&viewModel, &TaskListViewModel::countChanged};
    viewModel.setShowArchived(true);

    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(idAt(viewModel, 0), QStringLiteral("44444444-4444-4444-4444-444444444444"));
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

QTEST_APPLESS_MAIN(TaskViewModelsTest)

#include "tst_TaskViewModels.moc"
