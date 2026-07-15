#include "AppViewModel.h"
#include "TaskDependencyViewModel.h"
#include "TaskEditorViewModel.h"
#include "TaskGraphViewModel.h"
#include "TaskListViewModel.h"
#include "TaskProjectionSources.h"
#include "TaskPresentationFormatter.h"
#include "TaskFocusViewModel.h"
#include "TaskDetailsViewModel.h"
#include "TaskCategoryViewModel.h"
#include "fakes/FakeTaskDependencyRepository.h"
#include "fakes/FakeTaskCreationRepository.h"
#include "fakes/FakeTaskBatchTransitionRepository.h"
#include "fakes/FakeTaskDeletionRepository.h"
#include "fakes/FakeTaskRepository.h"
#include "fakes/FakeTaskCategoryRepository.h"

#include "domain/Task.h"
#include "services/TaskService.h"
#include "services/TaskCategoryService.h"

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
using smartmate::model::TaskCategory;
using smartmate::model::TaskCategoryColor;
using smartmate::model::TaskCategoryService;
using smartmate::model::TaskStatus;
using smartmate::tests::FakeTaskRepository;
using smartmate::tests::FakeTaskCreationRepository;
using smartmate::tests::FakeTaskBatchTransitionRepository;
using smartmate::tests::FakeTaskDeletionRepository;
using smartmate::tests::FakeTaskDependencyRepository;
using smartmate::tests::FakeTaskCategoryRepository;
using smartmate::viewmodel::AppViewModel;
using smartmate::viewmodel::TaskDependencyViewModel;
using smartmate::viewmodel::TaskEditorViewModel;
using smartmate::viewmodel::TaskGraphViewModel;
using smartmate::viewmodel::TaskListViewModel;
using smartmate::viewmodel::TaskGraphStatusFilter;
using smartmate::viewmodel::TaskPriorityVisual;
using smartmate::viewmodel::TaskStatusVisual;
using smartmate::viewmodel::TaskFocusViewModel;
using smartmate::viewmodel::TaskDetailsViewModel;
using smartmate::viewmodel::TaskCategoryViewModel;
using smartmate::viewmodel::TaskCategoryProjectionSource;
using smartmate::viewmodel::TaskPlanProjectionSource;

namespace {

struct ProjectionSources {
    explicit ProjectionSources(TaskService &service,
                               TaskCategoryService *categoryService = nullptr)
        : plan(service, categoryService)
        , categories(categoryService)
    {
    }

    ProjectionSources(TaskService &service, TaskCategoryService &categoryService)
        : ProjectionSources(service, &categoryService)
    {
    }

    TaskPlanProjectionSource plan;
    TaskCategoryProjectionSource categories;
};

// 各ViewModel测试只借用删除端口；端口生命周期覆盖全部局部Service。
FakeTaskDeletionRepository deletionRepository;
FakeTaskCategoryRepository categoryRepository;

/// 为既有ViewModel测试按其局部任务仓库重建原子状态端口，避免各用例重复样板。
[[nodiscard]] FakeTaskBatchTransitionRepository &batchTransitionsFor(
    FakeTaskRepository &repository)
{
    static std::optional<FakeTaskBatchTransitionRepository> batchRepository;
    batchRepository.emplace(repository);
    return *batchRepository;
}

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
                        std::optional<QDateTime> deadline = std::nullopt,
                        std::optional<smartmate::model::TaskCategoryId> categoryId =
                            std::nullopt)
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
                utcTime(updatedMilliseconds),
                categoryId};
}

[[nodiscard]] TaskCategory category(const QString &id,
                                    QString name,
                                    const TaskCategoryColor color)
{
    return {QUuid::fromString(id), std::move(name), color,
            utcTime(1700000000000), utcTime(1700000000000)};
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
    void listProjectsOverdueStateIndependentlyFromOrderReason();
    void listProjectsAndExecutesStateActionsByStableTaskId();
    void onlyTodoTasksCanOpenEditor();
    void listProjectsAndForwardsPermanentDeletionByStableTaskId();
    void listMaintainsBulkSelectionByStableIdAndResetsAtDisplayBoundaries();
    void listExecutesAtomicBulkCommandsAndPreservesFailedSelection();
    void listMapsBatchConflictIdsToTaskTitles();
    void listExposesAndClearsChineseErrors();
    void listProjectsBlockingAndUnlockInformation();
    void listFocusProjectionIgnoresFiltersAndTracksInProgress();
    void listSelectsStableTaskDetails();
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
    void editorRejectsStaleDraftAfterTaskLeavesTodo();
    void editorCreationPredecessorPickerUsesIsolatedCheckpoints();
    void editorAtomicCreationFailurePreservesTheWholeDraft();
    // 图像素布局与箭头几何属于ViewModel，拓扑语义仍取自Model快照。
    void graphProjectsNodesEdgesAndSelectionDetails();
    void graphMinimizesSimpleLayerCrossing();
    void graphRoutesLongEdgesDeterministically();
    void graphReloadPreservesVisibleSelectionAndClearsHiddenSelection();
    // 类别目录、任务筛选和分类子图只投影Service结果，不在QML复制业务规则。
    void categoryViewModelProjectsCrudAndUsageCounts();
    void categoryDeletionKeepsOpenEditorDraftClean();
    void categoryViewModelRetainsUsageCountsWhenTaskReadFails();
    void editorPersistsStableCategorySelection();
    void listCombinesCategoryWithExistingFilters();
    void graphProjectsCoreAndCrossCategoryContext();
    void graphCategoryFilterFailureRetainsPreviousStateAndSnapshot();
};

void TaskViewModelsTest::appViewModelOwnsBindableChildren()
{
    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskCategoryRepository categories;
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categories};
    TaskCategoryService categoryService{categories};
    AppViewModel app{service, categoryService};

    QVERIFY(app.taskList() != nullptr);
    QVERIFY(app.taskFocus() != nullptr);
    QVERIFY(app.taskDetails() != nullptr);
    QVERIFY(app.taskEditor() != nullptr);
    QVERIFY(app.taskDependencies() != nullptr);
    QVERIFY(app.taskGraph() != nullptr);
    QVERIFY(app.taskCategories() != nullptr);
    QVERIFY(app.appearanceSettings() != nullptr);
    // 七个类别消费者共享 AppViewModel 持有的唯一目录读取源。
    QCOMPARE(categories.findAllCount(), 1);
}

#include "tst_TaskListViewModel.inc"
#include "tst_TaskDependencyViewModel.inc"
#include "tst_TaskEditorViewModel.inc"
#include "tst_TaskGraphViewModel.inc"
#include "tst_TaskCategoryViewModels.inc"

QTEST_GUILESS_MAIN(TaskViewModelsTest)

#include "tst_TaskViewModels.moc"
