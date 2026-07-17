#include "view/widgets/task/TaskPage.h"
#include "view/widgets/task/DeadlinePickerDialog.h"
#include "view/widgets/task/DurationPickerDialog.h"
#include "view/widgets/task/TaskCategoryDialog.h"
#include "view/widgets/task/TaskCreationPredecessorDialog.h"
#include "view/widgets/task/TaskDependencyDialog.h"
#include "view/widgets/task/TaskDetailsDialog.h"
#include "view/widgets/task/TaskEditorDialog.h"
#include "view/widgets/task/TaskItemDelegate.h"
#include "view/widgets/theme/WidgetTheme.h"
#include "viewmodel/contracts/TaskCategoryContract.h"
#include "viewmodel/contracts/TaskDependencyContract.h"
#include "viewmodel/contracts/TaskDetailsContract.h"
#include "viewmodel/contracts/TaskEditorContract.h"
#include "viewmodel/contracts/TaskFocusContract.h"
#include "viewmodel/contracts/TaskListContract.h"

#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFrame>
#include <QFont>
#include <QLabel>
#include <QImage>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSpinBox>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyleOptionViewItem>
#include <QTest>
#include <QTimer>

#include <algorithm>

using namespace smartmate;

namespace {

constexpr auto categoryId = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
constexpr auto taskId = "11111111-1111-1111-1111-111111111111";
constexpr auto predecessorId = "22222222-2222-2222-2222-222222222222";

void answerNextConfirmation(const QMessageBox::StandardButton answer)
{
    QTimer::singleShot(0, [answer] {
        auto *messageBox = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        Q_ASSERT(messageBox);
        QAbstractButton *button = messageBox->button(answer);
        Q_ASSERT(button);
        button->click();
    });
}

class FakeTaskList final : public viewmodel::TaskListContract {
public:
    FakeTaskList() : TaskListContract(nullptr) {}
    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : rows; }
    int count() const noexcept override { return rows; }
    QVariant data(const QModelIndex &index, int role) const override {
        if (!index.isValid() || index.row() < 0 || index.row() >= rows) return {};
        switch (role) {
        case TaskIdRole: return QString::fromLatin1(taskId);
        case TitleRole: return QStringLiteral("契约任务");
        case DescriptionRole: return QStringLiteral("仅由 Fake Contract 提供");
        case StatusRole: return taskStatus;
        case StatusTextRole: return statusTexts.value(taskStatus, QStringLiteral("待办"));
        case PriorityRole: return taskPriority;
        case PriorityTextRole: return QStringLiteral("高");
        case DeadlineTextRole: return deadlineText;
        case EstimatedMinutesRole: return estimatedMinutes;
        case OrderReasonTextRole: return QStringLiteral("高优先");
        case OverdueRole: return overdue;
        case BlockedRole: return blocked;
        case BlockingReasonTextRole: return blocked ? QStringLiteral("等待前置任务") : QString{};
        case PredecessorCountRole: return predecessorCount;
        case UnlockCountRole: return unlockCount;
        case HasCategoryRole: return hasCategory;
        case CategoryNameRole: return QStringLiteral("学习");
        case CategoryAccentRole: return QStringLiteral("#7c3aed");
        case CanStartRole: return canStart;
        case CanEditTaskRole: case CanEditDependenciesRole: case CanCancelRole:
        case BulkSelectableRole: return true;
        default: return false;
        }
    }
    bool showArchived() const noexcept override { return archived; }
    QString searchText() const override { return search; }
    int priorityFilterIndex() const noexcept override { return priority; }
    QStringList priorityFilterOptions() const override { return {QStringLiteral("全部优先级"), QStringLiteral("低"), QStringLiteral("普通"), QStringLiteral("高"), QStringLiteral("紧急")}; }
    QVariantList categoryFilterOptions() const override { return {
        QVariantMap{{QStringLiteral("name"), QStringLiteral("全部类别")}, {QStringLiteral("mode"), 0}, {QStringLiteral("categoryId"), QString{}}},
        QVariantMap{{QStringLiteral("name"), QStringLiteral("未分类")}, {QStringLiteral("mode"), 1}, {QStringLiteral("categoryId"), QString{}}},
        QVariantMap{{QStringLiteral("name"), QStringLiteral("学习")}, {QStringLiteral("mode"), 2}, {QStringLiteral("categoryId"), QString::fromLatin1(categoryId)}}}; }
    int categoryFilterMode() const noexcept override { return categoryMode; }
    QString categoryFilterCategoryId() const override { return category; }
    bool hasActiveFilters() const override { return !search.isEmpty() || priority != 0 || categoryMode != 0; }
    bool bulkSelectionMode() const noexcept override { return bulk; }
    int bulkSelectedCount() const noexcept override { return selected ? 1 : 0; }
    int bulkSelectableVisibleCount() const override { return 1; }
    bool allVisibleSelected() const override { return selected; }
    bool canBulkArchive() const noexcept override { return bulk && selected && !archived; }
    bool canBulkRestore() const override { return bulk && selected && archived; }
    bool canBulkDelete() const noexcept override { return bulk && selected && archived; }
    void setShowArchived(bool value) override { ++showArchivedCalls; archived = value; emit showArchivedChanged(); }
    void setSearchText(const QString &value) override { ++searchCalls; search = value; emit searchTextChanged(); }
    void setPriorityFilterIndex(int value) override { ++priorityCalls; priority = value; emit priorityFilterIndexChanged(); }
    bool setCategoryFilter(int mode, const QString &id) override { ++categoryCalls; categoryMode = mode; category = id; emit categoryFilterChanged(); emit hasActiveFiltersChanged(); return true; }
    void reload() override {}
    void clearFilters() override { ++clearFilterCalls; search.clear(); priority = 0; categoryMode = 0; category.clear(); emit searchTextChanged(); emit priorityFilterIndexChanged(); emit categoryFilterChanged(); emit hasActiveFiltersChanged(); }
    bool startTask(const QString &id) override { ++startCalls; lastId = id; return true; }
    bool cancelTask(const QString &id) override { ++cancelCalls; lastId = id; return true; }
    bool completeTask(const QString &id) override { ++completeCalls; lastId = id; return true; }
    bool redoTask(const QString &id) override { ++redoCalls; lastId = id; return true; }
    bool archiveTask(const QString &id) override { ++archiveCalls; lastId = id; return true; }
    bool restoreTask(const QString &id) override { ++restoreCalls; lastId = id; return true; }
    bool deleteArchivedTask(const QString &id) override { ++deleteCalls; lastId = id; return true; }
    void beginBulkSelection() override { bulk = true; emit bulkSelectionChanged(); }
    bool toggleBulkSelection(const QString &id) override { lastId = id; selected = !selected; emit bulkSelectionChanged(); return true; }
    void toggleSelectAllVisible() override { selected = !selected; emit bulkSelectionChanged(); }
    void clearBulkSelection() override { selected = false; emit bulkSelectionChanged(); }
    void cancelBulkSelection() override { selected = false; bulk = false; emit bulkSelectionChanged(); }
    bool archiveSelectedTasks() override { ++bulkArchiveCalls; return true; }
    bool restoreSelectedTasks() override { ++bulkRestoreCalls; return true; }
    bool deleteSelectedArchivedTasks() override { ++bulkDeleteCalls; return true; }

    void pushSearch(QString value) { search = std::move(value); emit searchTextChanged(); }
    void setRows(const int value) { beginResetModel(); rows = value; endResetModel(); emit countChanged(); }
    int rows{1};
    bool archived{false}, bulk{false}, selected{false}, overdue{false}, blocked{false};
    bool canStart{true};
    bool hasCategory{false};
    int taskStatus{0}, taskPriority{2}, estimatedMinutes{0};
    int predecessorCount{0}, unlockCount{0};
    QString deadlineText;
    QStringList statusTexts{QStringLiteral("待办"), QStringLiteral("进行中"),
                            QStringLiteral("已完成"), QStringLiteral("已取消"),
                            QStringLiteral("已归档")};
    QString search, lastId, category; int priority{0}, categoryMode{0};
    int searchCalls{0}, priorityCalls{0}, categoryCalls{0}, showArchivedCalls{0}, startCalls{0};
    int cancelCalls{0}, completeCalls{0}, redoCalls{0}, archiveCalls{0}, restoreCalls{0}, deleteCalls{0};
    int bulkArchiveCalls{0}, bulkRestoreCalls{0}, bulkDeleteCalls{0};
    int clearFilterCalls{0};
};

class FakeFocus final : public viewmodel::TaskFocusContract {
public:
    FakeFocus() : TaskFocusContract(nullptr) {}
    FocusState focusState() const noexcept override { return state; }
    QString focusTaskId() const override { return id; }
    QString focusTitle() const override { return QStringLiteral("契约任务"); }
    QString focusDescription() const override { return QStringLiteral("描述"); }
    QString focusStatusText() const override { return QStringLiteral("待办"); }
    QString focusPriorityText() const override { return QStringLiteral("高"); }
    QString focusDeadlineText() const override { return {}; }
    int focusEstimatedMinutes() const noexcept override { return 30; }
    QString focusEstimatedText() const override { return QStringLiteral("预计 30 分钟"); }
    QString focusReasonText() const override { return QStringLiteral("高优先"); }
    bool focusOverdue() const noexcept override { return overdue; }
    bool focusCanStart() const noexcept override { return true; }
    bool focusCanComplete() const noexcept override { return false; }
    QString focusCategoryName() const override { return categoryName; }
    QString focusCategoryAccent() const override { return categoryAccent; }
    bool focusHasCategory() const noexcept override { return hasCategory; }
    FocusState state{FocusState::Suggested};
    QString id{QString::fromLatin1(taskId)};
    bool overdue{false};
    bool hasCategory{false};
    QString categoryName{QStringLiteral("学习")};
    QString categoryAccent{QStringLiteral("#2563eb")};
};

class FakeDetails final : public viewmodel::TaskDetailsContract {
public:
    FakeDetails() : TaskDetailsContract(nullptr) {}
    QString selectedTaskId() const override { return id; }
    QString selectedTitle() const override { return id.isEmpty() ? QString{} : title; }
    QString selectedDescription() const override { return description; }
    QString selectedStatusText() const override { return statusText; }
    viewmodel::TaskStatusVisual selectedStatusVisual() const noexcept override { return statusVisual; }
    QString selectedPriorityText() const override { return priorityText; }
    viewmodel::TaskPriorityVisual selectedPriorityVisual() const noexcept override { return priorityVisual; }
    QString selectedDeadlineText() const override { return deadline; }
    int selectedEstimatedMinutes() const noexcept override { return estimatedMinutes; }
    QString selectedCreatedAtText() const override { return createdAt; }
    QString selectedUpdatedAtText() const override { return updatedAt; }
    QString selectedReasonText() const override { return reason; }
    QString selectedBlockingReasonText() const override { return blockingReason; }
    int selectedPredecessorCount() const noexcept override { return predecessorCount; }
    int selectedUnlockCount() const noexcept override { return unlockCount; }
    bool selectedCanEditTask() const noexcept override { return canEditTask; }
    bool selectedCanEditDependencies() const noexcept override { return canEditDependencies; }
    QString selectedCategoryName() const override { return categoryName; }
    QString selectedCategoryAccent() const override { return categoryAccent; }
    bool selectedHasCategory() const noexcept override { return hasCategory; }
    bool selectedOverdue() const noexcept override { return overdue; }
    bool selectTask(const QString &value) override { ++selectCalls; id = value; emit selectionChanged(); return !id.isEmpty(); }
    void clearSelection() override { ++clearCalls; id.clear(); emit selectionChanged(); }
    void pushProjection() { emit selectionChanged(); }
    QString id;
    QString title{QStringLiteral("契约任务")};
    QString description{QStringLiteral("描述")};
    QString statusText{QStringLiteral("待办")};
    QString priorityText{QStringLiteral("高")};
    QString deadline;
    QString createdAt{QStringLiteral("2026-01-01 10:00")};
    QString updatedAt{QStringLiteral("2026-01-01 10:00")};
    QString reason{QStringLiteral("高优先")};
    QString blockingReason;
    QString categoryName;
    QString categoryAccent{QStringLiteral("#94a3b8")};
    int estimatedMinutes{30};
    int predecessorCount{0};
    int unlockCount{0};
    int selectCalls{0};
    int clearCalls{0};
    viewmodel::TaskStatusVisual statusVisual{viewmodel::TaskStatusVisual::Todo};
    viewmodel::TaskPriorityVisual priorityVisual{viewmodel::TaskPriorityVisual::High};
    bool overdue{false}, hasCategory{false};
    bool canEditTask{true}, canEditDependencies{true};
};

class FakeEditor final : public viewmodel::TaskEditorContract {
public:
    FakeEditor() : TaskEditorContract(nullptr) {}
    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : 1; }
    QVariant data(const QModelIndex &index, int role) const override {
        if (!index.isValid() || index.row() != 0) return {};
        switch (role) {
        case CandidateTaskIdRole: return QString::fromLatin1(predecessorId);
        case CandidateShortIdRole: return QStringLiteral("22222222");
        case CandidateTitleRole: return QStringLiteral("前置候选");
        case CandidateStatusTextRole: return QStringLiteral("待办");
        case CandidatePriorityTextRole: return QStringLiteral("高");
        case CandidateCategoryNameRole: return QStringLiteral("学习");
        case CandidateCategoryAccentRole: return QStringLiteral("#2563eb");
        case CandidateHasCategoryRole: return true;
        case CandidateSelectedRole: return pickerSelected;
        default: return {};
        }
    }
    QString taskId() const override { return {}; }
    bool editMode() const noexcept override { return edit; }
    bool sessionActive() const noexcept override { return active; }
    QString title() const override { return titleValue; }
    void setTitle(const QString &value) override { ++titleWrites; titleValue = value; emit titleChanged(); emit formStateChanged(); }
    QString description() const override { return descriptionValue; }
    void setDescription(const QString &value) override { ++descriptionWrites; descriptionValue = value; emit descriptionChanged(); }
    QString currentStatusText() const override { return QStringLiteral("待办"); }
    int priorityIndex() const noexcept override { return priority; }
    void setPriorityIndex(int value) override { priority = value; emit priorityIndexChanged(); }
    bool hasDeadline() const noexcept override { return false; }
    QString deadlineDisplayText() const override { return QStringLiteral("未设置"); }
    int deadlineYear() const override { return 2026; } int deadlineMonth() const override { return 1; }
    int deadlineDay() const override { return 1; } int deadlineHour() const override { return 10; }
    int deadlineMinute() const override { return 0; }
    bool hasEstimatedDuration() const noexcept override { return false; }
    QString estimatedDurationDisplayText() const override { return QStringLiteral("未设置"); }
    int estimatedDays() const noexcept override { return 0; } int estimatedHours() const noexcept override { return 0; }
    int estimatedMinutePart() const noexcept override { return 0; }
    int minimumEstimatedMinutes() const noexcept override { return 1; }
    int maximumEstimatedMinutes() const noexcept override { return 525600; }
    QStringList priorityOptions() const override { return {QStringLiteral("低"), QStringLiteral("普通"), QStringLiteral("高"), QStringLiteral("紧急")}; }
    QVariantList categoryOptions() const override { return {QVariantMap{{QStringLiteral("name"), QStringLiteral("未分类")}, {QStringLiteral("categoryId"), QString{}}}}; }
    QString selectedCategoryId() const override { return {}; }
    void setSelectedCategoryId(const QString &) override {}
    QString selectedCategoryName() const override { return QStringLiteral("未分类"); }
    QString selectedCategoryAccent() const override { return QStringLiteral("#94a3b8"); }
    bool hasCategory() const noexcept override { return false; }
    bool dirty() const noexcept override { return !titleValue.isEmpty(); }
    bool canSave() const noexcept override { return dirty(); }
    QString validationMessage() const override { return {}; }
    int predecessorCandidateCount() const noexcept override { return 1; }
    int selectedPredecessorCount() const noexcept override { return acceptedSelected ? 1 : 0; }
    QString predecessorSummaryText() const override { return acceptedSelected ? QStringLiteral("已选择 1 项") : QStringLiteral("未选择前置任务"); }
    bool canConfigurePredecessors() const noexcept override { return !edit; }
    bool beginCreate() override { ++beginCreateCalls; edit = false; active = true; emit modeChanged(); emit sessionActiveChanged(); return true; }
    bool beginEdit(const QString &) override { ++beginEditCalls; edit = true; active = true; emit modeChanged(); emit sessionActiveChanged(); return true; }
    bool setDeadlineSelection(int year, int month, int day, int hour, int minute) override { ++deadlineWrites; deadlineSelection = {year, month, day, hour, minute}; return true; }
    void clearDeadline() override {}
    bool setEstimatedDuration(int days, int hours, int minutes) override { ++durationWrites; durationSelection = {days, hours, minutes}; return true; }
    void clearEstimatedDuration() override {}
    void beginPredecessorSelection() override { ++beginPredecessorCalls; pickerSelected = acceptedSelected; emit predecessorSelectionChanged(); }
    bool setCreationPredecessorSelected(const QString &id, bool value) override { ++predecessorWrites; lastPredecessorId = id; pickerSelected = value; emit dataChanged(index(0), index(0), {CandidateSelectedRole}); emit predecessorSelectionChanged(); return true; }
    void acceptPredecessorSelection() override { ++acceptPredecessorCalls; acceptedSelected = pickerSelected; emit predecessorSelectionChanged(); }
    void cancelPredecessorSelection() override { ++cancelPredecessorCalls; pickerSelected = acceptedSelected; emit predecessorSelectionChanged(); }
    void clearCreationPredecessors() override { ++clearPredecessorCalls; pickerSelected = false; acceptedSelected = false; emit predecessorSelectionChanged(); }
    bool save() override { if (!canSave()) return false; active = false; emit sessionActiveChanged(); emit saved(QStringLiteral("id")); return true; }
    void cancel() override { active = false; emit sessionActiveChanged(); emit cancelled(); }
    void pushTitle(QString value) { titleValue = std::move(value); emit titleChanged(); }
    void pushDescription(QString value) { descriptionValue = std::move(value); emit descriptionChanged(); }
    bool edit{false}, active{false}, pickerSelected{false}, acceptedSelected{false}; int priority{1};
    QString titleValue, descriptionValue, lastPredecessorId;
    int beginCreateCalls{0}, beginEditCalls{0}, titleWrites{0}, descriptionWrites{0};
    int beginPredecessorCalls{0}, predecessorWrites{0}, acceptPredecessorCalls{0};
    int cancelPredecessorCalls{0}, clearPredecessorCalls{0};
    int deadlineWrites{0}, durationWrites{0};
    QList<int> deadlineSelection, durationSelection;
};

class FakeCategory final : public viewmodel::TaskCategoryContract {
public:
    FakeCategory() : TaskCategoryContract(nullptr) {}
    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : 1; }
    QVariant data(const QModelIndex &index, int role) const override {
        if (!index.isValid() || index.row() != 0) return {};
        switch (role) {
        case CategoryIdRole: return QString::fromLatin1(categoryId);
        case NameRole: return QStringLiteral("学习");
        case ColorIndexRole: return 1;
        case AccentRole: return QStringLiteral("#2563eb");
        case TaskCountRole: return 2;
        default: return {};
        }
    }
    int count() const noexcept override { return 1; }
    bool empty() const noexcept override { return false; }
    bool editMode() const noexcept override { return editing; }
    QString editingCategoryId() const override { return editing ? QString::fromLatin1(categoryId) : QString{}; }
    QString draftName() const override { return name; }
    void setDraftName(const QString &value) override { ++nameWrites; name = value; emit draftChanged(); }
    int draftColorIndex() const noexcept override { return color; }
    void setDraftColorIndex(int value) override { ++colorWrites; color = value; emit draftChanged(); }
    QStringList colorOptions() const override { return {QStringLiteral("蓝色"), QStringLiteral("绿色")}; }
    QStringList colorAccents() const override { return {QStringLiteral("#2563eb"), QStringLiteral("#16a34a")}; }
    bool dirty() const noexcept override { return !name.isEmpty(); }
    bool canSave() const noexcept override { return dirty(); }
    void reload() override { ++reloadCalls; }
    void beginCreate() override { ++beginCreateCalls; editing = false; name.clear(); emit draftChanged(); }
    bool beginEdit(const QString &id) override { ++beginEditCalls; lastId = id; editing = true; name = QStringLiteral("学习"); emit draftChanged(); return true; }
    bool save() override { ++saveCalls; editing = true; emit saved(QString::fromLatin1(categoryId)); return true; }
    bool deleteCategory(const QString &id) override { ++deleteCalls; lastId = id; emit deleted(id, 2); return true; }
    void cancel() override { ++cancelCalls; beginCreate(); emit cancelled(); }

    bool editing{false}; int color{0}; QString name, lastId;
    int reloadCalls{0}, beginCreateCalls{0}, beginEditCalls{0}, saveCalls{0};
    int deleteCalls{0}, cancelCalls{0}, nameWrites{0}, colorWrites{0};
};

class FakeDependency final : public viewmodel::TaskDependencyContract {
public:
    FakeDependency() : TaskDependencyContract(nullptr) {}
    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : 1; }
    QVariant data(const QModelIndex &index, int role) const override {
        if (!index.isValid() || index.row() != 0) return {};
        switch (role) {
        case TaskIdRole: return QString::fromLatin1(predecessorId);
        case ShortIdRole: return QStringLiteral("22222222");
        case TitleRole: return QStringLiteral("前置候选");
        case StatusTextRole: return QStringLiteral("待办");
        case PriorityTextRole: return QStringLiteral("高");
        case SelectedRole: return selected;
        case ArchivedRole: return false;
        case SelectableRole: return selectable;
        case CategoryNameRole: return QStringLiteral("学习");
        case CategoryAccentRole: return QStringLiteral("#2563eb");
        case HasCategoryRole: return true;
        default: return {};
        }
    }
    QString taskId() const override { return targetId; }
    QString taskTitle() const override { return QStringLiteral("目标任务"); }
    int count() const noexcept override { return 1; }
    int selectedCount() const noexcept override { return selected ? 1 : 0; }
    bool dirty() const noexcept override { return selected; }
    bool canSave() const noexcept override { return selected; }
    bool beginEdit(const QString &id) override { ++beginEditCalls; targetId = id; emit contextChanged(); return beginSucceeds; }
    bool setPredecessorSelected(const QString &id, bool value) override { ++selectionCalls; lastId = id; selected = value; emit dataChanged(index(0), index(0), {SelectedRole}); emit selectionChanged(); emit formStateChanged(); return true; }
    bool save() override { ++saveCalls; if (!saveSucceeds) { emit notificationRaised({common::UiSeverity::Error, QStringLiteral("依赖操作失败"), QStringLiteral("循环依赖：目标任务 → 前置候选 → 目标任务")}); return false; } selected = false; emit formStateChanged(); emit saved(targetId); return true; }
    void cancel() override { ++cancelCalls; selected = false; emit selectionChanged(); emit formStateChanged(); emit cancelled(); }

    bool selectable{true}, selected{false}, beginSucceeds{true}, saveSucceeds{true};
    QString targetId, lastId;
    int beginEditCalls{0}, selectionCalls{0}, saveCalls{0}, cancelCalls{0};
};

} // namespace

class TaskWidgetsTest final : public QObject {
    Q_OBJECT
private slots:
    void initialBindingAndUserCommandsUseContracts();
    void programmaticUpdatesDoNotWriteBack();
    void textInputPreservesOrderAndCursor();
    void typedPickersPreserveValuesAndContractBounds();
    void categoryAndDependencyDialogsUseStableContractCommands();
    void creationPredecessorSelectionCommitsOrRestoresLocalDraft();
    void filtersDetailsDragAndConfirmationsPreserveStableCommands();
    void emptyLayoutAndDedicatedDragHandleRemainStable();
    void editorPickersCommitOnceAndFitMinimumWindow();
    void themedHierarchyAndResponsiveEditorRemainStable();
    void pickerPresentationAndResponsiveLayoutRemainTyped();
    void taskCardRendersSemanticColorsAndTimeProjection();
    void detailsDialogUsesThemedResponsiveVisualHierarchy();
};

void TaskWidgetsTest::initialBindingAndUserCommandsUseContracts()
{
    FakeTaskList tasks; FakeFocus focus; FakeDetails details; FakeEditor editor;
    FakeCategory categories; FakeDependency dependencies;
    view::widgets::TaskPage page{{tasks, focus, details, editor,
                                  categories, dependencies}};
    page.resize(900, 650); page.show();

    auto *search = page.findChild<QLineEdit *>(QStringLiteral("taskSearchField"));
    QVERIFY(search);
    QTest::keyClicks(search, QStringLiteral("MVVM"));
    QCOMPARE(tasks.search, QStringLiteral("MVVM"));
    QVERIFY(tasks.searchCalls > 0);

    auto *priority = page.findChild<QComboBox *>(QStringLiteral("priorityFilterComboBox"));
    QVERIFY(priority);
    priority->activated(3);
    QCOMPARE(tasks.priority, 3);
    auto *category = page.findChild<QComboBox *>(QStringLiteral("categoryFilterComboBox"));
    QVERIFY(category);
    category->activated(2);
    QCOMPARE(tasks.categoryMode, 2);
    QCOMPARE(tasks.category, QString::fromLatin1(categoryId));
    QCOMPARE(tasks.categoryCalls, 1);

    auto *focusAction = page.findChild<QPushButton *>(QStringLiteral("focusPrimaryActionButton"));
    QTest::mouseClick(focusAction, Qt::LeftButton);
    QCOMPARE(tasks.startCalls, 1);
    QCOMPARE(tasks.lastId, focus.id);

    auto *newTask = page.findChild<QPushButton *>(QStringLiteral("newTaskButton"));
    QTest::mouseClick(newTask, Qt::LeftButton);
    QCOMPARE(editor.beginCreateCalls, 1);
    auto *title = page.findChild<QLineEdit *>(QStringLiteral("taskTitleField"));
    QTRY_VERIFY(title && title->isVisible());
    QTest::keyClicks(title, QStringLiteral("New task"));
    QCOMPARE(editor.titleValue, QStringLiteral("New task"));
    static_cast<QDialog *>(title->window())->reject();

    tasks.setShowArchived(true);
    auto *bulk = page.findChild<QPushButton *>(QStringLiteral("bulkManagementButton"));
    QTest::mouseClick(bulk, Qt::LeftButton);
    auto *list = page.findChild<QListView *>(QStringLiteral("taskListView"));
    list->setCurrentIndex(tasks.index(0));
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                      list->visualRect(tasks.index(0)).center());
    QVERIFY(tasks.selected);
    auto *restore = page.findChild<QPushButton *>(QStringLiteral("bulkRestoreButton"));
    QTest::mouseClick(restore, Qt::LeftButton);
    QCOMPARE(tasks.bulkRestoreCalls, 1);
}

void TaskWidgetsTest::programmaticUpdatesDoNotWriteBack()
{
    FakeTaskList tasks; FakeFocus focus; FakeDetails details; FakeEditor editor;
    FakeCategory categories; FakeDependency dependencies;
    view::widgets::TaskPage page{{tasks, focus, details, editor,
                                  categories, dependencies}};
    page.resize(900, 650); page.show();
    const int searchCalls = tasks.searchCalls;
    tasks.pushSearch(QStringLiteral("服务刷新"));
    QCOMPARE(page.findChild<QLineEdit *>(QStringLiteral("taskSearchField"))->text(),
             QStringLiteral("服务刷新"));
    QCOMPARE(tasks.searchCalls, searchCalls);

    editor.beginCreate();
    auto *title = page.findChild<QLineEdit *>(QStringLiteral("taskTitleField"));
    QTRY_VERIFY(title && title->isVisible());
    const int titleWrites = editor.titleWrites;
    editor.pushTitle(QStringLiteral("程序回填"));
    QCOMPARE(title->text(), QStringLiteral("程序回填"));
    QCOMPARE(editor.titleWrites, titleWrites);
}

void TaskWidgetsTest::textInputPreservesOrderAndCursor()
{
    FakeEditor editor;
    view::widgets::TaskEditorDialog dialog{editor};
    dialog.resize(700, 650);
    QVERIFY(editor.beginCreate());
    QTRY_VERIFY(dialog.isVisible());

    auto *title = dialog.findChild<QLineEdit *>(QStringLiteral("taskTitleField"));
    auto *description = dialog.findChild<QPlainTextEdit *>(
        QStringLiteral("taskDescriptionArea"));
    QVERIFY(title);
    QVERIFY(description);

    title->setFocus();
    QTest::keyClicks(title, QStringLiteral("1234"));
    QCOMPARE(title->text(), QStringLiteral("1234"));
    QCOMPARE(editor.titleValue, QStringLiteral("1234"));
    QCOMPARE(title->cursorPosition(), 4);

    description->setFocus();
    QTest::keyClicks(description, QStringLiteral("1234"));
    QCOMPARE(description->toPlainText(), QStringLiteral("1234"));
    QCOMPARE(editor.descriptionValue, QStringLiteral("1234"));
    QCOMPARE(description->textCursor().position(), 4);

    const int descriptionWrites = editor.descriptionWrites;
    editor.pushDescription(QStringLiteral("程序性描述回填"));
    QCOMPARE(description->toPlainText(), QStringLiteral("程序性描述回填"));
    QCOMPARE(editor.descriptionWrites, descriptionWrites);
}

void TaskWidgetsTest::typedPickersPreserveValuesAndContractBounds()
{
    view::widgets::DeadlinePickerDialog deadline;
    deadline.setSelection(2028, 2, 29, 23, 45);
    QCOMPARE(deadline.selectedYear(), 2028);
    QCOMPARE(deadline.selectedMonth(), 2);
    QCOMPARE(deadline.selectedDay(), 29);
    QCOMPARE(deadline.selectedHour(), 23);
    QCOMPARE(deadline.selectedMinute(), 45);

    view::widgets::DurationPickerDialog duration{1, 1500};
    duration.setDuration(0, 0, 0);
    QCOMPARE(duration.totalMinutes(), 1);
    duration.setDuration(2, 0, 0);
    QCOMPARE(duration.totalMinutes(), 1500);
    QCOMPARE(duration.selectedDays(), 1);
    QCOMPARE(duration.selectedHours(), 1);
    QCOMPARE(duration.selectedMinutes(), 0);

    auto *hours = duration.findChild<QSpinBox *>(QStringLiteral("durationHoursSpinBox"));
    auto *minutes = duration.findChild<QSpinBox *>(QStringLiteral("durationMinutesSpinBox"));
    QVERIFY(hours && minutes);
    QCOMPARE(hours->maximum(), 1);
    QCOMPARE(minutes->maximum(), 0);
}

void TaskWidgetsTest::categoryAndDependencyDialogsUseStableContractCommands()
{
    FakeCategory categories;
    view::widgets::TaskCategoryDialog categoryDialog{categories};
    categoryDialog.openManager();
    QTRY_VERIFY(categoryDialog.isVisible());
    QCOMPARE(categories.reloadCalls, 1);

    auto *name = categoryDialog.findChild<QLineEdit *>(QStringLiteral("categoryNameField"));
    auto *saveCategory = categoryDialog.findChild<QPushButton *>(QStringLiteral("saveCategoryButton"));
    QVERIFY(name && saveCategory);
    QTest::keyClicks(name, QStringLiteral("Study"));
    QTest::mouseClick(saveCategory, Qt::LeftButton);
    QCOMPARE(categories.saveCalls, 1);

    auto *categoryList = categoryDialog.findChild<QListView *>(QStringLiteral("categoryListView"));
    auto *deleteCategory = categoryDialog.findChild<QPushButton *>(QStringLiteral("deleteSelectedCategoryButton"));
    QVERIFY(categoryList && deleteCategory);
    categoryList->setCurrentIndex(categories.index(0));
    answerNextConfirmation(QMessageBox::Cancel);
    QTest::mouseClick(deleteCategory, Qt::LeftButton);
    QCOMPARE(categories.deleteCalls, 0);
    answerNextConfirmation(QMessageBox::Ok);
    QTest::mouseClick(deleteCategory, Qt::LeftButton);
    QCOMPARE(categories.deleteCalls, 1);
    QCOMPARE(categories.lastId, QString::fromLatin1(categoryId));
    categoryDialog.close();

    FakeDependency dependencies;
    view::widgets::TaskDependencyDialog dependencyDialog{dependencies};
    QVERIFY(dependencyDialog.openTask(QString::fromLatin1(taskId)));
    QTRY_VERIFY(dependencyDialog.isVisible());
    QCOMPARE(dependencies.targetId, QString::fromLatin1(taskId));
    auto *candidateList = dependencyDialog.findChild<QListView *>(QStringLiteral("dependencyCandidateList"));
    QVERIFY(candidateList);
    QTest::mouseClick(candidateList->viewport(), Qt::LeftButton, Qt::NoModifier,
                      candidateList->visualRect(dependencies.index(0)).center());
    QCOMPARE(dependencies.selectionCalls, 1);
    QCOMPARE(dependencies.lastId, QString::fromLatin1(predecessorId));

    dependencies.saveSucceeds = false;
    auto *saveDependencies = dependencyDialog.findChild<QPushButton *>(QStringLiteral("saveDependenciesButton"));
    QTest::mouseClick(saveDependencies, Qt::LeftButton);
    QCOMPARE(dependencies.saveCalls, 1);
    QVERIFY(dependencyDialog.isVisible());
    auto *notification = dependencyDialog.findChild<QLabel *>(QStringLiteral("dependencyNotificationLabel"));
    QVERIFY(notification->text().contains(QStringLiteral("循环依赖")));

    dependencies.saveSucceeds = true;
    QTest::mouseClick(saveDependencies, Qt::LeftButton);
    QCOMPARE(dependencies.saveCalls, 2);
    QTRY_VERIFY(!dependencyDialog.isVisible());
    QCOMPARE(dependencies.cancelCalls, 0);
}

void TaskWidgetsTest::creationPredecessorSelectionCommitsOrRestoresLocalDraft()
{
    FakeEditor editor;
    view::widgets::TaskCreationPredecessorDialog dialog{editor};
    dialog.openSelection();
    QTRY_VERIFY(dialog.isVisible());
    auto *list = dialog.findChild<QListView *>(QStringLiteral("creationPredecessorCandidateList"));
    QVERIFY(list);
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                      list->visualRect(editor.index(0)).center());
    QCOMPARE(editor.lastPredecessorId, QString::fromLatin1(predecessorId));
    QVERIFY(editor.pickerSelected);
    dialog.reject();
    QCOMPARE(editor.cancelPredecessorCalls, 1);
    QVERIFY(!editor.acceptedSelected);

    dialog.openSelection();
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                      list->visualRect(editor.index(0)).center());
    auto *accept = dialog.findChild<QPushButton *>(QStringLiteral("acceptCreationPredecessorsButton"));
    QVERIFY(accept);
    QTest::mouseClick(accept, Qt::LeftButton);
    QCOMPARE(editor.acceptPredecessorCalls, 1);
    QVERIFY(editor.acceptedSelected);
    QTRY_VERIFY(!dialog.isVisible());
}

void TaskWidgetsTest::filtersDetailsDragAndConfirmationsPreserveStableCommands()
{
    FakeTaskList tasks; FakeFocus focus; FakeDetails details; FakeEditor editor;
    FakeCategory categories; FakeDependency dependencies;
    tasks.rows = 0;
    tasks.search = QStringLiteral("无结果");
    view::widgets::TaskPage page{{tasks, focus, details, editor,
                                  categories, dependencies}};
    page.resize(900, 620);
    page.show();

    auto *empty = page.findChild<QLabel *>(QStringLiteral("taskEmptyStateLabel"));
    QTRY_VERIFY(empty && empty->isVisible());
    QVERIFY(empty->text().contains(QStringLiteral("搜索和筛选")));
    QTest::mouseClick(page.findChild<QPushButton *>(QStringLiteral("clearFiltersButton")),
                      Qt::LeftButton);
    QCOMPARE(tasks.clearFilterCalls, 1);

    tasks.setRows(1);
    auto *list = page.findChild<view::widgets::TaskListView *>(
        QStringLiteral("taskListView"));
    QVERIFY(list);
    list->activated(tasks.index(0));
    QTRY_COMPARE(details.selectCalls, 1);
    QCOMPARE(details.id, QString::fromLatin1(taskId));

    QMimeData mime;
    mime.setData(QStringLiteral("application/x-smartmate-task-id"),
                 QByteArray(taskId));
    auto *focusPanel = page.findChild<view::widgets::TaskFocusPanel *>(
        QStringLiteral("focusTaskSlot"));
    QVERIFY(focusPanel);
    QDragEnterEvent enter{{10, 10}, Qt::MoveAction, &mime,
                          Qt::LeftButton, Qt::NoModifier};
    QApplication::sendEvent(focusPanel, &enter);
    QDropEvent drop{{10.0, 10.0}, Qt::MoveAction, &mime,
                    Qt::LeftButton, Qt::NoModifier};
    QApplication::sendEvent(focusPanel, &drop);
    QCOMPARE(tasks.startCalls, 1);
    QCOMPARE(tasks.lastId, QString::fromLatin1(taskId));

    auto *delegate = qobject_cast<view::widgets::TaskItemDelegate *>(
        list->itemDelegate());
    QVERIFY(delegate);
    answerNextConfirmation(QMessageBox::Cancel);
    delegate->cancelRequested(QString::fromLatin1(taskId), QStringLiteral("契约任务"));
    QCOMPARE(tasks.cancelCalls, 0);
    answerNextConfirmation(QMessageBox::Ok);
    delegate->cancelRequested(QString::fromLatin1(taskId), QStringLiteral("契约任务"));
    QCOMPARE(tasks.cancelCalls, 1);

    tasks.bulk = true;
    tasks.selected = true;
    tasks.archived = false;
    emit tasks.bulkSelectionChanged();
    answerNextConfirmation(QMessageBox::Cancel);
    QTest::mouseClick(page.findChild<QPushButton *>(QStringLiteral("bulkArchiveButton")),
                      Qt::LeftButton);
    QCOMPARE(tasks.bulkArchiveCalls, 0);
    answerNextConfirmation(QMessageBox::Ok);
    QTest::mouseClick(page.findChild<QPushButton *>(QStringLiteral("bulkArchiveButton")),
                      Qt::LeftButton);
    QCOMPARE(tasks.bulkArchiveCalls, 1);

    tasks.archived = true;
    emit tasks.bulkSelectionChanged();
    answerNextConfirmation(QMessageBox::Ok);
    QTest::mouseClick(page.findChild<QPushButton *>(QStringLiteral("bulkDeleteButton")),
                      Qt::LeftButton);
    QCOMPARE(tasks.bulkDeleteCalls, 1);
}

void TaskWidgetsTest::emptyLayoutAndDedicatedDragHandleRemainStable()
{
    FakeTaskList tasks; FakeFocus focus; FakeDetails details; FakeEditor editor;
    FakeCategory categories; FakeDependency dependencies;
    tasks.rows = 0;
    focus.state = viewmodel::TaskFocusContract::FocusState::NoTasks;
    focus.id.clear();
    view::widgets::TaskPage page{{tasks, focus, details, editor,
                                  categories, dependencies}};
    page.resize(900, 620);
    page.show();

    auto *focusPanel = page.findChild<view::widgets::TaskFocusPanel *>(
        QStringLiteral("focusTaskSlot"));
    auto *search = page.findChild<QLineEdit *>(QStringLiteral("taskSearchField"));
    auto *content = page.findChild<QStackedWidget *>(QStringLiteral("taskContentStack"));
    auto *empty = page.findChild<QLabel *>(QStringLiteral("taskEmptyStateLabel"));
    auto *list = page.findChild<view::widgets::TaskListView *>(
        QStringLiteral("taskListView"));
    QVERIFY(focusPanel && search && content && empty && list);
    QTRY_VERIFY(page.isVisible());
    QCOMPARE(content->currentWidget(), static_cast<QWidget *>(empty));
    QVERIFY(content->height() > empty->sizeHint().height());
    const int focusTop = focusPanel->mapTo(&page, QPoint{}).y();
    const int toolbarTop = search->mapTo(&page, QPoint{}).y();

    tasks.setRows(1);
    QTRY_COMPARE(content->currentWidget(), static_cast<QWidget *>(list));
    QCOMPARE(focusPanel->mapTo(&page, QPoint{}).y(), focusTop);
    QCOMPARE(search->mapTo(&page, QPoint{}).y(), toolbarTop);
    tasks.setRows(0);
    QTRY_COMPARE(content->currentWidget(), static_cast<QWidget *>(empty));
    QCOMPARE(focusPanel->mapTo(&page, QPoint{}).y(), focusTop);
    QCOMPARE(search->mapTo(&page, QPoint{}).y(), toolbarTop);

    tasks.setRows(1);
    focus.state = viewmodel::TaskFocusContract::FocusState::Suggested;
    focus.id = QString::fromLatin1(taskId);
    emit focus.focusTaskChanged();
    auto *delegate = qobject_cast<view::widgets::TaskItemDelegate *>(list->itemDelegate());
    QVERIFY(delegate);
    const QModelIndex index = tasks.index(0);
    QTRY_VERIFY(!list->visualRect(index).isEmpty());
    const QRect handle = delegate->dragHandleRect(list->visualRect(index), index);
    QVERIFY(!handle.isEmpty());

    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                      handle.center());
    QCOMPARE(details.selectCalls, 0);
    QCOMPARE(tasks.startCalls, 0);

    QSignalSpy dragStarted(list, &view::widgets::TaskListView::taskDragStarted);
    QTest::mousePress(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                      handle.center());
    const QPoint smallMove = handle.center() + QPoint{
        std::max(1, QApplication::startDragDistance() - 1), 0};
    QMouseEvent belowThreshold{QEvent::MouseMove, QPointF{smallMove},
        QPointF{list->viewport()->mapToGlobal(smallMove)}, Qt::NoButton,
        Qt::LeftButton, Qt::NoModifier};
    QApplication::sendEvent(list->viewport(), &belowThreshold);
    QCOMPARE(dragStarted.count(), 0);
    const QPoint dragMove = handle.center()
        + QPoint{QApplication::startDragDistance() + 4, 0};
    QMouseEvent aboveThreshold{QEvent::MouseMove, QPointF{dragMove},
        QPointF{list->viewport()->mapToGlobal(dragMove)}, Qt::NoButton,
        Qt::LeftButton, Qt::NoModifier};
    QApplication::sendEvent(list->viewport(), &aboveThreshold);
    QTRY_COMPARE(dragStarted.count(), 1);
    QCOMPARE(dragStarted.at(0).at(0).toString(), QString::fromLatin1(taskId));

    tasks.bulk = true;
    emit tasks.bulkSelectionChanged();
    QVERIFY(delegate->dragHandleRect(list->visualRect(index), index).isEmpty());
    tasks.bulk = false;
    tasks.canStart = false;
    emit tasks.dataChanged(index, index, {viewmodel::TaskListContract::CanStartRole});
    QVERIFY(delegate->dragHandleRect(list->visualRect(index), index).isEmpty());

    QMimeData mime;
    mime.setData(QStringLiteral("application/x-smartmate-task-id"), QByteArray(taskId));
    focus.state = viewmodel::TaskFocusContract::FocusState::InProgress;
    emit focus.focusTaskChanged();
    QDragEnterEvent rejectedEnter{{10, 10}, Qt::MoveAction, &mime,
                                  Qt::LeftButton, Qt::NoModifier};
    QApplication::sendEvent(focusPanel, &rejectedEnter);
    QVERIFY(!rejectedEnter.isAccepted());
    QDropEvent rejectedDrop{{10.0, 10.0}, Qt::MoveAction, &mime,
                            Qt::LeftButton, Qt::NoModifier};
    QApplication::sendEvent(focusPanel, &rejectedDrop);
    QCOMPARE(tasks.startCalls, 0);
}

void TaskWidgetsTest::editorPickersCommitOnceAndFitMinimumWindow()
{
    FakeTaskList tasks; FakeFocus focus; FakeDetails details; FakeEditor editor;
    FakeCategory categories; FakeDependency dependencies;
    view::widgets::TaskPage page{{tasks, focus, details, editor,
                                  categories, dependencies}};
    QFont enlarged = page.font();
    enlarged.setPointSizeF(enlarged.pointSizeF() * 1.1);
    page.setFont(enlarged);
    page.resize(900, 620);
    page.show();
    QVERIFY(editor.beginCreate());

    auto *dialog = page.findChild<view::widgets::TaskEditorDialog *>(
        QStringLiteral("taskEditorDialog"));
    QTRY_VERIFY(dialog && dialog->isVisible());
    QVERIFY(dialog->width() <= page.width());
    QVERIFY(dialog->height() <= page.height());

    QTimer::singleShot(0, [] {
        auto *picker = qobject_cast<view::widgets::DeadlinePickerDialog *>(
            QApplication::activeModalWidget());
        Q_ASSERT(picker);
        picker->setSelection(2028, 2, 29, 23, 45);
        picker->accept();
    });
    QTest::mouseClick(dialog->findChild<QPushButton *>(
                          QStringLiteral("openDeadlinePickerButton")),
                      Qt::LeftButton);
    QCOMPARE(editor.deadlineWrites, 1);
    QCOMPARE(editor.deadlineSelection, QList<int>({2028, 2, 29, 23, 45}));

    QTimer::singleShot(0, [] {
        auto *picker = qobject_cast<view::widgets::DeadlinePickerDialog *>(
            QApplication::activeModalWidget());
        Q_ASSERT(picker);
        picker->reject();
    });
    QTest::mouseClick(dialog->findChild<QPushButton *>(
                          QStringLiteral("openDeadlinePickerButton")),
                      Qt::LeftButton);
    QCOMPARE(editor.deadlineWrites, 1);

    QTimer::singleShot(0, [] {
        auto *picker = qobject_cast<view::widgets::DurationPickerDialog *>(
            QApplication::activeModalWidget());
        Q_ASSERT(picker);
        picker->setDuration(2, 3, 4);
        picker->accept();
    });
    QTest::mouseClick(dialog->findChild<QPushButton *>(
                          QStringLiteral("openDurationPickerButton")),
                      Qt::LeftButton);
    QCOMPARE(editor.durationWrites, 1);
    QCOMPARE(editor.durationSelection, QList<int>({2, 3, 4}));

    QTimer::singleShot(0, [] {
        auto *picker = qobject_cast<view::widgets::DurationPickerDialog *>(
            QApplication::activeModalWidget());
        Q_ASSERT(picker);
        picker->reject();
    });
    QTest::mouseClick(dialog->findChild<QPushButton *>(
                          QStringLiteral("openDurationPickerButton")),
                      Qt::LeftButton);
    QCOMPARE(editor.durationWrites, 1);
}

void TaskWidgetsTest::themedHierarchyAndResponsiveEditorRemainStable()
{
    FakeTaskList tasks; FakeFocus focus; FakeDetails details; FakeEditor editor;
    FakeCategory categories; FakeDependency dependencies;
    focus.hasCategory = true;
    focus.overdue = true;
    view::widgets::TaskPage page{{tasks, focus, details, editor,
                                  categories, dependencies}};
    page.resize(900, 650);
    page.show();
    QCoreApplication::processEvents();

    auto *list = page.findChild<view::widgets::TaskListView *>(
        QStringLiteral("taskListView"));
    auto *content = page.findChild<QStackedWidget *>(QStringLiteral("taskContentStack"));
    QVERIFY(list && content);
    QCOMPARE(list->frameShape(), QFrame::NoFrame);
    QVERIFY(list->viewport()->testAttribute(Qt::WA_TranslucentBackground));
    QVERIFY(content->testAttribute(Qt::WA_TranslucentBackground));
    QVERIFY(list->styleSheet().contains(QStringLiteral("background: transparent")));

    auto *delegate = qobject_cast<view::widgets::TaskItemDelegate *>(list->itemDelegate());
    QVERIFY(delegate);
    const auto renderedCardSurface = [&page, list, delegate, &tasks](const int accentIndex) {
        const view::widgets::WidgetTheme theme =
            view::widgets::WidgetTheme::fromAccentIndex(accentIndex);
        page.setStyleSheet({});
        page.setPalette(theme.palette());
        page.setStyleSheet(theme.styleSheet());
        QCoreApplication::processEvents();

        QImage image(QSize{520, 112}, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        QStyleOptionViewItem option;
        option.rect = image.rect();
        option.palette = list->palette();
        option.font = list->font();
        option.widget = list;
        delegate->paint(&painter, option, tasks.index(0));
        painter.end();
        return image.pixelColor(500, 95);
    };
    const auto greenTheme = view::widgets::WidgetTheme::fromAccentIndex(0);
    const auto blueTheme = view::widgets::WidgetTheme::fromAccentIndex(1);
    QCOMPARE(renderedCardSurface(0), greenTheme.surface);
    QCOMPARE(renderedCardSurface(1), blueTheme.surface);
    QVERIFY(renderedCardSurface(0) != QColor(Qt::black));

    auto *icon = page.findChild<QLabel *>(QStringLiteral("focusStateIconText"));
    auto *eyebrow = page.findChild<QLabel *>(QStringLiteral("focusEyebrow"));
    auto *title = page.findChild<QLabel *>(QStringLiteral("focusTaskTitle"));
    auto *category = page.findChild<QLabel *>(QStringLiteral("focusCategoryBadge"));
    auto *overdue = page.findChild<QLabel *>(QStringLiteral("focusOverdueBadge"));
    auto *reminder = page.findChild<QLabel *>(QStringLiteral("focusOverdueReminder"));
    QVERIFY(icon && eyebrow && title && category && overdue && reminder);
    QCOMPARE(icon->text(), QStringLiteral("▶"));
    QVERIFY(eyebrow->text().contains(QStringLiteral("推荐任务")));
    QCOMPARE(title->text(), QStringLiteral("契约任务"));
    QCOMPARE(category->text(), QStringLiteral("学习"));
    QVERIFY(category->isVisible());
    QVERIFY(overdue->isVisible());
    QVERIFY(reminder->isVisible());

    focus.state = viewmodel::TaskFocusContract::FocusState::InProgress;
    emit focus.focusTaskChanged();
    QVERIFY(eyebrow->text().contains(QStringLiteral("正在进行")));
    QCOMPARE(page.findChild<QPushButton *>(QStringLiteral("focusPrimaryActionButton"))->text(),
             QStringLiteral("完成任务"));
    focus.state = viewmodel::TaskFocusContract::FocusState::AllBlocked;
    focus.id.clear();
    focus.hasCategory = false;
    focus.overdue = false;
    emit focus.focusTaskChanged();
    QCOMPARE(icon->text(), QStringLiteral("!"));
    QVERIFY(!category->isVisible());
    QVERIFY(!overdue->isVisible());
    focus.state = viewmodel::TaskFocusContract::FocusState::NoTasks;
    emit focus.focusTaskChanged();
    QCOMPARE(icon->text(), QStringLiteral("+"));

    QVERIFY(editor.beginCreate());
    auto *dialog = page.findChild<view::widgets::TaskEditorDialog *>(
        QStringLiteral("taskEditorDialog"));
    QTRY_VERIFY(dialog && dialog->isVisible());
    QVERIFY(dialog->findChild<QFrame *>(QStringLiteral("taskEditorHeader")));
    QVERIFY(dialog->findChild<QFrame *>(QStringLiteral("taskEditorBasicSection")));
    QVERIFY(dialog->findChild<QFrame *>(QStringLiteral("taskEditorPlanningSection")));
    QVERIFY(dialog->findChild<QFrame *>(QStringLiteral("taskEditorScheduleSection")));
    QVERIFY(dialog->findChild<QFrame *>(QStringLiteral("taskEditorFooter")));
    auto *scroll = dialog->findChild<QScrollArea *>(QStringLiteral("taskEditorScrollView"));
    QVERIFY(scroll);
    QCOMPARE(scroll->horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    QVERIFY(dialog->findChild<QLabel *>(QStringLiteral("taskEditorHeaderTitle"))
                ->text().contains(QStringLiteral("新建")));

    auto *statusField = dialog->findChild<QWidget *>(QStringLiteral("taskEditorStatusField"));
    auto *priorityField = dialog->findChild<QWidget *>(QStringLiteral("taskEditorPriorityField"));
    auto *categoryField = dialog->findChild<QWidget *>(QStringLiteral("taskEditorCategoryField"));
    QVERIFY(statusField && priorityField && categoryField);
    dialog->resize(700, 650);
    QCoreApplication::processEvents();
    QCOMPARE(statusField->geometry().top(), priorityField->geometry().top());
    QVERIFY(categoryField->geometry().top() > statusField->geometry().top());
    dialog->resize(560, 600);
    QCoreApplication::processEvents();
    QVERIFY(priorityField->geometry().top() > statusField->geometry().top());
    QVERIFY(categoryField->geometry().top() > priorityField->geometry().top());
    editor.cancel();
    QTRY_VERIFY(!dialog->isVisible());

    const int startCalls = tasks.startCalls;
    focus.state = viewmodel::TaskFocusContract::FocusState::Suggested;
    focus.id = QString::fromLatin1(taskId);
    emit focus.focusTaskChanged();
    QTest::mouseClick(page.findChild<QPushButton *>(
                          QStringLiteral("focusPrimaryActionButton")),
                      Qt::LeftButton);
    QCOMPARE(tasks.startCalls, startCalls + 1);
    QCOMPARE(tasks.lastId, QString::fromLatin1(taskId));
}

void TaskWidgetsTest::pickerPresentationAndResponsiveLayoutRemainTyped()
{
    const view::widgets::WidgetTheme theme =
        view::widgets::WidgetTheme::fromAccentIndex(0);
    view::widgets::DeadlinePickerDialog deadline;
    deadline.setPalette(theme.palette());
    deadline.setStyleSheet(theme.styleSheet());
    deadline.setSelection(2028, 2, 29, 23, 45);
    deadline.show();
    QCoreApplication::processEvents();
    QVERIFY(deadline.findChild<QFrame *>(QStringLiteral("pickerHeader")));
    QVERIFY(deadline.findChild<QFrame *>(QStringLiteral("deadlineCalendarCard")));
    QVERIFY(deadline.findChild<QFrame *>(QStringLiteral("deadlineTimeCard")));
    QVERIFY(deadline.findChild<QFrame *>(QStringLiteral("pickerFooter")));
    auto *monthTitle = deadline.findChild<QLabel *>(QStringLiteral("deadlineMonthTitle"));
    QVERIFY(monthTitle && monthTitle->text().contains(QStringLiteral("2028"))
            && monthTitle->text().contains(QStringLiteral("2")));
    auto *deadlineScroll = deadline.findChild<QScrollArea *>(
        QStringLiteral("deadlinePickerScrollView"));
    QVERIFY(deadlineScroll);
    QCOMPARE(deadlineScroll->horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    QCOMPARE(deadline.findChild<QPushButton *>(
                 QStringLiteral("confirmDeadlineSelectionButton"))->text(),
             QStringLiteral("确定"));
    QCOMPARE(deadline.findChild<QPushButton *>(
                 QStringLiteral("cancelDeadlineSelectionButton"))->text(),
             QStringLiteral("取消"));
    QSignalSpy deadlineAccepted(&deadline, &QDialog::accepted);
    QTest::mouseClick(deadline.findChild<QPushButton *>(
                          QStringLiteral("confirmDeadlineSelectionButton")),
                      Qt::LeftButton);
    QCOMPARE(deadlineAccepted.count(), 1);
    QCOMPARE(deadline.selectedYear(), 2028);
    QCOMPARE(deadline.selectedMonth(), 2);
    QCOMPARE(deadline.selectedDay(), 29);

    view::widgets::DurationPickerDialog duration{1, 1500};
    duration.setPalette(theme.palette());
    duration.setStyleSheet(theme.styleSheet());
    duration.setDuration(0, 2, 30);
    duration.resize(520, 400);
    duration.show();
    QCoreApplication::processEvents();
    auto fields = duration.findChildren<QFrame *>(QStringLiteral("durationValueCard"));
    QCOMPARE(fields.size(), 3);
    QCOMPARE(fields.at(0)->geometry().top(), fields.at(1)->geometry().top());
    QCOMPARE(fields.at(1)->geometry().top(), fields.at(2)->geometry().top());
    auto *summary = duration.findChild<QLabel *>(QStringLiteral("durationSummaryLabel"));
    QVERIFY(summary && summary->text().contains(QStringLiteral("150")));
    auto *durationScroll = duration.findChild<QScrollArea *>(
        QStringLiteral("durationPickerScrollView"));
    QVERIFY(durationScroll);
    QCOMPARE(durationScroll->horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    duration.resize(420, 400);
    QCoreApplication::processEvents();
    QVERIFY(fields.at(1)->geometry().top() > fields.at(0)->geometry().top());
    QVERIFY(fields.at(2)->geometry().top() > fields.at(1)->geometry().top());
    QSignalSpy durationRejected(&duration, &QDialog::rejected);
    QTest::mouseClick(duration.findChild<QPushButton *>(
                          QStringLiteral("cancelDurationSelectionButton")),
                      Qt::LeftButton);
    QCOMPARE(durationRejected.count(), 1);
    QCOMPARE(duration.totalMinutes(), 150);
}

void TaskWidgetsTest::taskCardRendersSemanticColorsAndTimeProjection()
{
    FakeTaskList tasks; FakeFocus focus; FakeDetails details; FakeEditor editor;
    FakeCategory categories; FakeDependency dependencies;
    tasks.deadlineText = QStringLiteral("2028-02-29 23:45");
    tasks.estimatedMinutes = 90;
    tasks.predecessorCount = 2;
    tasks.unlockCount = 1;
    tasks.hasCategory = true;
    view::widgets::TaskPage page{{tasks, focus, details, editor,
                                  categories, dependencies}};
    const view::widgets::WidgetTheme theme =
        view::widgets::WidgetTheme::fromAccentIndex(0);
    QCOMPARE(theme.statusColor(viewmodel::TaskStatusVisual::InProgress),
             theme.inProgress);
    QCOMPARE(theme.priorityColor(viewmodel::TaskPriorityVisual::Low),
             theme.textSecondary);
    QCOMPARE(theme.priorityColor(viewmodel::TaskPriorityVisual::Normal),
             theme.todo);
    QCOMPARE(theme.priorityColor(viewmodel::TaskPriorityVisual::High),
             theme.warning);
    QCOMPARE(theme.priorityColor(viewmodel::TaskPriorityVisual::Urgent),
             theme.danger);
    page.setPalette(theme.palette());
    page.setStyleSheet(theme.styleSheet());
    page.resize(900, 650);
    page.show();
    QCoreApplication::processEvents();
    auto *list = page.findChild<view::widgets::TaskListView *>(
        QStringLiteral("taskListView"));
    auto *delegate = qobject_cast<view::widgets::TaskItemDelegate *>(list->itemDelegate());
    QVERIFY(list && delegate);
    QCOMPARE(delegate->sizeHint({}, tasks.index(0)).height(), 138);

    const auto render = [&] {
        QImage image(QSize{600, 138}, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        QStyleOptionViewItem option;
        option.rect = image.rect();
        option.palette = list->palette();
        option.font = list->font();
        option.widget = list;
        delegate->paint(&painter, option, tasks.index(0));
        painter.end();
        return image;
    };
    const auto containsColor = [](const QImage &image, const QRect &area,
                                  const QColor &color) {
        const QRect bounded = area.intersected(image.rect());
        for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
            for (int x = bounded.left(); x <= bounded.right(); ++x) {
                if (image.pixelColor(x, y) == color) return true;
            }
        }
        return false;
    };

    for (int status = 0; status <= 4; ++status) {
        tasks.taskStatus = status;
        tasks.blocked = false;
        const QImage image = render();
        QCOMPARE(image.pixelColor(4, 69), theme.statusColor(
            static_cast<viewmodel::TaskStatusVisual>(status)));
    }
    tasks.blocked = true;
    const QImage blockedImage = render();
    QCOMPARE(blockedImage.pixelColor(4, 69), theme.warning);
    QVERIFY(containsColor(blockedImage, QRect{60, 96, 380, 24}, theme.warning));

    tasks.blocked = false;
    tasks.taskPriority = 3;
    tasks.overdue = true;
    const QImage highlightedImage = render();
    QVERIFY(containsColor(highlightedImage, QRect{60, 38, 380, 24}, theme.danger));
    QVERIFY(containsColor(highlightedImage, QRect{60, 38, 380, 24},
                          QColor(QStringLiteral("#7c3aed"))));

    tasks.overdue = false;
    tasks.estimatedMinutes = 0;
    const QImage withoutEstimate = render();
    tasks.estimatedMinutes = 90;
    const QImage withEstimate = render();
    QVERIFY(withoutEstimate.copy(QRect{60, 68, 380, 22})
            != withEstimate.copy(QRect{60, 68, 380, 22}));
}

void TaskWidgetsTest::detailsDialogUsesThemedResponsiveVisualHierarchy()
{
    FakeDetails details;
    details.description = QStringLiteral(
        "这是一段较长的任务说明，用于验证详情内容根据文字自然增高，"
        "并在窄窗口与较大字号下保持完整、可读且不会与时间信息重叠。");
    details.deadline = QStringLiteral("2028-02-29 23:45");
    details.estimatedMinutes = 90;
    details.predecessorCount = 3;
    details.unlockCount = 2;
    details.hasCategory = true;
    details.categoryName = QStringLiteral("学习");
    details.categoryAccent = QStringLiteral("#7c3aed");
    details.overdue = true;
    details.blockingReason = QStringLiteral("等待“准备材料”完成或取消");

    view::widgets::TaskDetailsDialog dialog{details};
    const auto greenTheme = view::widgets::WidgetTheme::fromAccentIndex(0);
    dialog.setPalette(greenTheme.palette());
    dialog.setStyleSheet(greenTheme.styleSheet());
    dialog.resize(640, 620);
    QVERIFY(dialog.openTask(QString::fromLatin1(taskId)));
    QTRY_VERIFY(dialog.isVisible());

    auto *header = dialog.findChild<QFrame *>(QStringLiteral("taskDetailsHeader"));
    auto *descriptionSection = dialog.findChild<QFrame *>(
        QStringLiteral("taskDetailsDescriptionSection"));
    auto *scheduleSection = dialog.findChild<QFrame *>(
        QStringLiteral("taskDetailsScheduleSection"));
    auto *insightSection = dialog.findChild<QFrame *>(
        QStringLiteral("taskDetailsInsightSection"));
    auto *footer = dialog.findChild<QFrame *>(QStringLiteral("taskDetailsFooter"));
    QVERIFY(header && descriptionSection && scheduleSection && insightSection && footer);
    auto *scroll = dialog.findChild<QScrollArea *>(
        QStringLiteral("taskDetailsScrollView"));
    QVERIFY(scroll);
    QCOMPARE(scroll->horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    QCOMPARE(dialog.findChildren<QFrame *>(
                 QStringLiteral("taskDetailsMetadataItem")).size(), 4);
    QCOMPARE(dialog.findChildren<QFrame *>(
                 QStringLiteral("taskDetailsMetric")).size(), 2);

    auto *statusBadge = dialog.findChild<QLabel *>(
        QStringLiteral("taskDetailsStatusBadge"));
    auto *priorityBadge = dialog.findChild<QLabel *>(
        QStringLiteral("taskDetailsPriorityBadge"));
    auto *categoryBadge = dialog.findChild<QLabel *>(
        QStringLiteral("taskDetailsCategoryBadge"));
    auto *overdueBadge = dialog.findChild<QLabel *>(
        QStringLiteral("taskDetailsOverdueBadge"));
    QVERIFY(statusBadge && priorityBadge && categoryBadge && overdueBadge);
    QVERIFY(statusBadge->styleSheet().contains(
        greenTheme.statusColor(viewmodel::TaskStatusVisual::Todo).name()));
    QVERIFY(priorityBadge->styleSheet().contains(
        greenTheme.priorityColor(viewmodel::TaskPriorityVisual::High).name()));
    QVERIFY(categoryBadge->styleSheet().contains(QStringLiteral("#7c3aed")));
    QVERIFY(overdueBadge->isVisible());
    QVERIFY(overdueBadge->styleSheet().contains(greenTheme.danger.name()));

    auto *blockingBlock = dialog.findChild<QFrame *>(
        QStringLiteral("taskDetailsBlockingBlock"));
    auto *recommendationBlock = dialog.findChild<QFrame *>(
        QStringLiteral("taskDetailsRecommendationBlock"));
    QVERIFY(blockingBlock->isVisible());
    QVERIFY(!recommendationBlock->isVisible());
    QVERIFY(blockingBlock->styleSheet().contains(greenTheme.warning.name()));
    details.blockingReason.clear();
    details.pushProjection();
    QVERIFY(!blockingBlock->isVisible());
    QVERIFY(recommendationBlock->isVisible());

    for (int value = 0; value <= 4; ++value) {
        details.statusVisual = static_cast<viewmodel::TaskStatusVisual>(value);
        details.pushProjection();
        QVERIFY(statusBadge->styleSheet().contains(
            greenTheme.statusColor(details.statusVisual).name()));
    }
    for (int value = 0; value <= 3; ++value) {
        details.priorityVisual = static_cast<viewmodel::TaskPriorityVisual>(value);
        details.pushProjection();
        QVERIFY(priorityBadge->styleSheet().contains(
            greenTheme.priorityColor(details.priorityVisual).name()));
    }

    auto fields = dialog.findChildren<QFrame *>(
        QStringLiteral("taskDetailsMetadataItem"));
    QCoreApplication::processEvents();
    QCOMPARE(fields.at(0)->geometry().top(), fields.at(1)->geometry().top());
    QCOMPARE(fields.at(2)->geometry().top(), fields.at(3)->geometry().top());
    QFont enlarged = dialog.font();
    enlarged.setPointSizeF(enlarged.pointSizeF() * 1.25);
    dialog.setFont(enlarged);
    dialog.resize(480, 500);
    QCoreApplication::processEvents();
    QVERIFY(fields.at(1)->geometry().top() > fields.at(0)->geometry().top());
    QVERIFY(fields.at(2)->geometry().top() > fields.at(1)->geometry().top());
    QVERIFY(fields.at(3)->geometry().top() > fields.at(2)->geometry().top());
    QVERIFY(descriptionSection->geometry().bottom()
            < scheduleSection->geometry().top());
    QVERIFY(scheduleSection->geometry().bottom() < insightSection->geometry().top());

    const auto blueTheme = view::widgets::WidgetTheme::fromAccentIndex(1);
    details.statusVisual = viewmodel::TaskStatusVisual::InProgress;
    dialog.setPalette(blueTheme.palette());
    dialog.setStyleSheet(blueTheme.styleSheet());
    details.pushProjection();
    QCoreApplication::processEvents();
    QVERIFY(statusBadge->styleSheet().contains(blueTheme.inProgress.name()));

    QSignalSpy editSpy(&dialog, &view::widgets::TaskDetailsDialog::editRequested);
    QTest::mouseClick(dialog.findChild<QPushButton *>(
                          QStringLiteral("editSelectedTaskButton")),
                      Qt::LeftButton);
    QCOMPARE(editSpy.count(), 1);
    QCOMPARE(editSpy.constFirst().constFirst().toString(),
             QString::fromLatin1(taskId));
    QCOMPARE(details.clearCalls, 1);

    QVERIFY(dialog.openTask(QString::fromLatin1(taskId)));
    QTRY_VERIFY(dialog.isVisible());
    QTest::mouseClick(dialog.findChild<QPushButton *>(
                          QStringLiteral("closeTaskDetailsButton")),
                      Qt::LeftButton);
    QCOMPARE(details.clearCalls, 2);
    QCOMPARE(editSpy.count(), 1);
}

QTEST_MAIN(TaskWidgetsTest)
#include "tst_TaskWidgets.moc"
