#include "TaskEditorViewModel.h"

#include "TaskErrorMapper.h"
#include "TaskPresentationFormatter.h"
#include "TaskCategoryPresentation.h"
#include "domain/TaskConstraints.h"
#include "domain/TaskCreationRequest.h"
#include "services/TaskService.h"

#include <QDateTime>
#include <QStringList>

#include <algorithm>
#include <utility>

namespace smartmate::viewmodel {

namespace {
// 优先级索引只在展示层转换为领域枚举；生成的草稿仍须由 TaskService
// 执行完整业务校验。任务状态不属于编辑草稿，必须经由显式状态命令修改。
[[nodiscard]] model::TaskDraft makeDraft(const QString &title,
                                         const QString &description,
                                         const model::TaskPriority priority,
                                         std::optional<QDateTime> deadline,
                                         std::optional<int> estimatedMinutes,
                                         std::optional<model::TaskCategoryId> categoryId)
{
    model::TaskDraft draft;
    draft.title = title;
    draft.description = description;
    draft.priority = priority;
    draft.deadline = std::move(deadline);
    draft.estimatedMinutes = estimatedMinutes;
    draft.categoryId = categoryId;
    return draft;
}
}

TaskEditorViewModel::TaskEditorViewModel(
    model::TaskService &taskService,
    TaskCategoryProjectionSource &categorySource,
    QObject *parent)
    : TaskEditorViewModel(taskService, categorySource,
                          QTimeZone::systemTimeZone(), parent)
{
}

TaskEditorViewModel::TaskEditorViewModel(model::TaskService &taskService,
                                         TaskCategoryProjectionSource &categorySource,
                                         QTimeZone timeZone,
                                         QObject *parent)
    : TaskEditorContract(parent)
    , m_taskService(taskService)
    , m_categorySource(categorySource)
    , m_priorityIndex(taskPriorityIndex(model::TaskPriority::Normal))
    , m_timeZone(std::move(timeZone))
{
    connect(&m_categorySource, &TaskCategoryProjectionSource::categoriesChanged,
            this, &TaskEditorViewModel::applyCategories);
    connect(&m_categorySource, &TaskCategoryProjectionSource::refreshFailed,
            this, [this] {
                setErrorMessage(QStringLiteral("类别数据访问失败，请稍后重试。"));
            });
    connect(&m_categorySource, &TaskCategoryProjectionSource::refreshSucceeded,
            this, [this] {
                if (m_errorMessage
                    == QStringLiteral("类别数据访问失败，请稍后重试。")) {
                    setErrorMessage({});
                }
            });
    applyCategories();
    rememberCurrentDraft();
    updateFormState();
}

int TaskEditorViewModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_predecessorCandidates.size());
}

QVariant TaskEditorViewModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= m_predecessorCandidates.size()) {
        return {};
    }

    const model::Task &candidate = m_predecessorCandidates.at(index.row());
    switch (role) {
    case CandidateTaskIdRole:
        return candidate.id().toString(QUuid::WithoutBraces);
    case CandidateShortIdRole:
        return candidate.id().toString(QUuid::WithoutBraces).left(8);
    case CandidateTitleRole:
        return candidate.title();
    case CandidateStatusTextRole:
        return taskStatusText(candidate.status());
    case CandidatePriorityTextRole:
        return taskPriorityText(candidate.priority());
    case CandidateCategoryNameRole: {
        if (!candidate.categoryId().has_value()) return QString{};
        const auto iterator = std::find_if(
            m_categorySource.categories().cbegin(),
            m_categorySource.categories().cend(), [&](const auto &category) {
                return category.id == *candidate.categoryId();
            });
        return iterator == m_categorySource.categories().cend() ? QString{} : iterator->name;
    }
    case CandidateCategoryAccentRole: {
        if (!candidate.categoryId().has_value()) return QString{};
        const auto iterator = std::find_if(
            m_categorySource.categories().cbegin(),
            m_categorySource.categories().cend(), [&](const auto &category) {
                return category.id == *candidate.categoryId();
            });
        return iterator == m_categorySource.categories().cend()
            ? QString{} : taskCategoryAccent(iterator->color);
    }
    case CandidateHasCategoryRole: {
        if (!candidate.categoryId().has_value()) return false;
        return std::any_of(m_categorySource.categories().cbegin(),
                           m_categorySource.categories().cend(),
                           [&](const auto &category) {
                               return category.id == *candidate.categoryId();
                           });
    }
    case CandidateSelectedRole:
        return (m_predecessorPickerActive ? m_pickerPredecessors
                                          : m_selectedCreationPredecessors)
            .contains(candidate.id());
    default:
        return {};
    }
}

QHash<int, QByteArray> TaskEditorViewModel::roleNames() const
{
    return {
        {CandidateTaskIdRole, "candidateTaskId"},
        {CandidateShortIdRole, "candidateShortId"},
        {CandidateTitleRole, "candidateTitle"},
        {CandidateStatusTextRole, "candidateStatusText"},
        {CandidatePriorityTextRole, "candidatePriorityText"},
        {CandidateCategoryNameRole, "candidateCategoryName"},
        {CandidateCategoryAccentRole, "candidateCategoryAccent"},
        {CandidateHasCategoryRole, "candidateHasCategory"},
        {CandidateSelectedRole, "candidateSelected"},
    };
}

QString TaskEditorViewModel::taskId() const
{
    return m_taskId;
}

bool TaskEditorViewModel::editMode() const noexcept
{
    return m_editMode;
}

bool TaskEditorViewModel::sessionActive() const noexcept
{
    return m_sessionActive;
}

QString TaskEditorViewModel::title() const
{
    return m_title;
}

void TaskEditorViewModel::setTitle(const QString &title)
{
    if (m_title == title) {
        return;
    }
    m_title = title;
    // 字段通知先让 Widget 同步文本，随后 formStateChanged 更新校验和保存资格。
    emit titleChanged();
    setErrorMessage({});
    updateFormState();
}

QString TaskEditorViewModel::description() const
{
    return m_description;
}

void TaskEditorViewModel::setDescription(const QString &description)
{
    if (m_description == description) {
        return;
    }
    m_description = description;
    emit descriptionChanged();
    setErrorMessage({});
    updateFormState();
}

QString TaskEditorViewModel::currentStatusText() const
{
    return taskStatusText(m_currentStatus);
}

int TaskEditorViewModel::priorityIndex() const noexcept
{
    return m_priorityIndex;
}

void TaskEditorViewModel::setPriorityIndex(const int priorityIndex)
{
    if (m_priorityIndex == priorityIndex) {
        return;
    }
    m_priorityIndex = priorityIndex;
    emit priorityIndexChanged();
    setErrorMessage({});
    updateFormState();
}

bool TaskEditorViewModel::hasDeadline() const noexcept
{
    return m_deadline.has_value();
}

QString TaskEditorViewModel::deadlineDisplayText() const
{
    const auto deadline = displayedDeadline();
    return taskDateTimeText(deadline.value_or(QDateTime{}),
                            QStringLiteral("未设置"));
}

int TaskEditorViewModel::deadlineYear() const
{
    const auto deadline = displayedDeadline();
    return deadline.has_value() ? deadline->date().year() : 0;
}

int TaskEditorViewModel::deadlineMonth() const
{
    const auto deadline = displayedDeadline();
    return deadline.has_value() ? deadline->date().month() : 0;
}

int TaskEditorViewModel::deadlineDay() const
{
    const auto deadline = displayedDeadline();
    return deadline.has_value() ? deadline->date().day() : 0;
}

int TaskEditorViewModel::deadlineHour() const
{
    const auto deadline = displayedDeadline();
    return deadline.has_value() ? deadline->time().hour() : 0;
}

int TaskEditorViewModel::deadlineMinute() const
{
    const auto deadline = displayedDeadline();
    return deadline.has_value() ? deadline->time().minute() : 0;
}

bool TaskEditorViewModel::hasEstimatedDuration() const noexcept
{
    return m_estimatedMinutes.has_value();
}

QString TaskEditorViewModel::estimatedDurationDisplayText() const
{
    if (!m_estimatedMinutes.has_value()) {
        return QStringLiteral("未设置");
    }

    QStringList parts;
    if (estimatedDays() > 0) {
        parts.append(QStringLiteral("%1天").arg(estimatedDays()));
    }
    if (estimatedHours() > 0) {
        parts.append(QStringLiteral("%1小时").arg(estimatedHours()));
    }
    if (estimatedMinutePart() > 0) {
        parts.append(QStringLiteral("%1分钟").arg(estimatedMinutePart()));
    }
    return parts.join(QLatin1Char(' '));
}

int TaskEditorViewModel::estimatedDays() const noexcept
{
    return m_estimatedMinutes.value_or(0) / (24 * 60);
}

int TaskEditorViewModel::estimatedHours() const noexcept
{
    return (m_estimatedMinutes.value_or(0) % (24 * 60)) / 60;
}

int TaskEditorViewModel::estimatedMinutePart() const noexcept
{
    return m_estimatedMinutes.value_or(0) % 60;
}

int TaskEditorViewModel::minimumEstimatedMinutes() const noexcept
{
    return model::TaskConstraints::minimumEstimatedMinutes;
}

int TaskEditorViewModel::maximumEstimatedMinutes() const noexcept
{
    return model::TaskConstraints::maximumEstimatedMinutes;
}

QStringList TaskEditorViewModel::priorityOptions() const
{
    return taskPriorityOptions();
}

QVariantList TaskEditorViewModel::categoryOptions() const
{
    QVariantList options;
    options.reserve(m_categorySource.categories().size() + 1);
    options.append(QVariantMap{{QStringLiteral("categoryId"), QString{}},
                               {QStringLiteral("name"), QStringLiteral("未分类")},
                               {QStringLiteral("accent"), taskUncategorizedAccent()}});
    for (const auto &category : m_categorySource.categories()) {
        options.append(QVariantMap{
            {QStringLiteral("categoryId"), category.id.toString(QUuid::WithoutBraces)},
            {QStringLiteral("name"), category.name},
            {QStringLiteral("accent"), taskCategoryAccent(category.color)}});
    }
    return options;
}

QString TaskEditorViewModel::selectedCategoryId() const
{
    return m_categoryId.has_value()
        ? m_categoryId->toString(QUuid::WithoutBraces) : QString{};
}

void TaskEditorViewModel::setSelectedCategoryId(const QString &categoryId)
{
    const QString trimmed = categoryId.trimmed();
    std::optional<model::TaskCategoryId> selected;
    if (!trimmed.isEmpty()) {
        const model::TaskCategoryId id{QUuid::fromString(trimmed)};
        const bool exists = std::any_of(
            m_categorySource.categories().cbegin(),
            m_categorySource.categories().cend(),
            [&id](const auto &category) { return category.id == id; });
        if (id.isNull() || !exists) {
            setErrorMessage(QStringLiteral("所选类别不存在或已被删除。"));
            return;
        }
        selected = id;
    }
    if (m_categoryId == selected) return;
    m_categoryId = selected;
    emit categoryChanged();
    updateFormState();
    setErrorMessage({});
}

const model::TaskCategory *TaskEditorViewModel::selectedCategory() const
{
    if (!m_categoryId.has_value()) return nullptr;
    const auto iterator = std::find_if(
        m_categorySource.categories().cbegin(),
        m_categorySource.categories().cend(), [&](const auto &category) {
            return category.id == *m_categoryId;
        });
    return iterator == m_categorySource.categories().cend() ? nullptr : &*iterator;
}

QString TaskEditorViewModel::selectedCategoryName() const
{
    const auto *category = selectedCategory();
    return category ? category->name : QStringLiteral("未分类");
}

QString TaskEditorViewModel::selectedCategoryAccent() const
{
    const auto *category = selectedCategory();
    return category ? taskCategoryAccent(category->color)
                    : taskUncategorizedAccent();
}

bool TaskEditorViewModel::hasCategory() const noexcept
{
    return m_categoryId.has_value();
}

bool TaskEditorViewModel::dirty() const noexcept
{
    return m_dirty;
}

bool TaskEditorViewModel::canSave() const noexcept
{
    return m_canSave;
}

QString TaskEditorViewModel::validationMessage() const
{
    return m_validationMessage;
}

QString TaskEditorViewModel::errorMessage() const
{
    return m_errorMessage;
}

int TaskEditorViewModel::predecessorCandidateCount() const noexcept
{
    return static_cast<int>(m_predecessorCandidates.size());
}

int TaskEditorViewModel::selectedPredecessorCount() const noexcept
{
    return static_cast<int>((m_predecessorPickerActive
                                 ? m_pickerPredecessors
                                 : m_selectedCreationPredecessors)
                                .size());
}

QString TaskEditorViewModel::predecessorSummaryText() const
{
    const QSet<model::TaskId> &selection = m_predecessorPickerActive
        ? m_pickerPredecessors
        : m_selectedCreationPredecessors;
    if (selection.isEmpty()) {
        return QStringLiteral("未设置");
    }
    return QStringLiteral("已选择 %1 项").arg(selection.size());
}

bool TaskEditorViewModel::canConfigurePredecessors() const noexcept
{
    return !m_editMode;
}

bool TaskEditorViewModel::beginCreate()
{
    // 候选资格由 Model 给出；ViewModel 只保存用于多选弹窗的展示快照。
    const auto candidates = m_taskService.listEligibleCreationPredecessors();
    if (!candidates.ok()) {
        setErrorMessage(taskErrorMessage(candidates.error));
        return false;
    }

    replaceCandidates(*candidates.value);
    replaceDraft(defaultSnapshot(), {}, false, model::TaskStatus::Todo);
    setSessionActive(true);
    return true;
}

bool TaskEditorViewModel::beginEdit(const QString &taskId)
{
    const auto id = QUuid::fromString(taskId.trimmed());
    if (id.isNull()) {
        setErrorMessage(taskErrorMessage(model::TaskError::NotFound));
        return false;
    }

    // 编辑资格由Model返回结构化结果；ViewModel只映射错误并决定是否建立草稿。
    const auto result = m_taskService.findEditableTask(id);
    if (!result.ok()) {
        setErrorMessage(taskErrorMessage(result.error));
        return false;
    }

    const auto &task = *result.value;
    Snapshot draft;
    draft.title = task.title();
    draft.description = task.description();
    draft.priorityIndex = taskPriorityIndex(task.priority());
    draft.deadline = task.deadline();
    draft.estimatedMinutes = task.estimatedMinutes();
    draft.categoryId = task.categoryId();

    replaceCandidates({});
    replaceDraft(draft, task.id().toString(QUuid::WithoutBraces), true,
                 task.status());
    setSessionActive(true);
    return true;
}

bool TaskEditorViewModel::setDeadlineSelection(const int year,
                                               const int month,
                                               const int day,
                                               const int hour,
                                               const int minute)
{
    // Reject 同时拒绝 DST 跳时产生的不存在时间与回拨产生的歧义时间，
    // 避免界面选择在保存时悄悄变成另一个时刻。
    const QDateTime selected{QDate{year, month, day},
                             QTime{hour, minute},
                             m_timeZone,
                             QDateTime::TransitionResolution::Reject};
    if (!selected.isValid()) {
        setErrorMessage(QStringLiteral("所选截止时间无效，请重新选择。"));
        return false;
    }

    if (m_deadline != selected) {
        m_deadline = selected;
        emit deadlineChanged();
        updateFormState();
    }
    setErrorMessage({});
    return true;
}

void TaskEditorViewModel::clearDeadline()
{
    if (!m_deadline.has_value()) {
        setErrorMessage({});
        return;
    }
    m_deadline.reset();
    emit deadlineChanged();
    setErrorMessage({});
    updateFormState();
}

bool TaskEditorViewModel::setEstimatedDuration(const int days,
                                               const int hours,
                                               const int minutes)
{
    // 组件范围属于选择器协议；总分钟的最终领域边界仍由 TaskService 校验。
    if (days < 0 || days > 365 || hours < 0 || hours > 23
        || minutes < 0 || minutes > 59) {
        setErrorMessage(taskErrorMessage(model::TaskError::InvalidEstimate));
        return false;
    }

    const int totalMinutes = days * 24 * 60 + hours * 60 + minutes;
    if (!m_taskService.validateEstimatedMinutes(totalMinutes).ok()) {
        setErrorMessage(taskErrorMessage(model::TaskError::InvalidEstimate));
        return false;
    }

    if (m_estimatedMinutes != totalMinutes) {
        m_estimatedMinutes = totalMinutes;
        emit estimatedDurationChanged();
        updateFormState();
    }
    setErrorMessage({});
    return true;
}

void TaskEditorViewModel::clearEstimatedDuration()
{
    if (!m_estimatedMinutes.has_value()) {
        setErrorMessage({});
        return;
    }
    m_estimatedMinutes.reset();
    emit estimatedDurationChanged();
    setErrorMessage({});
    updateFormState();
}

void TaskEditorViewModel::beginPredecessorSelection()
{
    if (!canConfigurePredecessors()) {
        return;
    }
    // 复制检查点形成可撤销子会话；弹窗取消不会污染主创建草稿。
    m_pickerPredecessors = m_selectedCreationPredecessors;
    m_predecessorPickerActive = true;
    notifyCandidateSelectionChanged();
}

bool TaskEditorViewModel::setCreationPredecessorSelected(const QString &taskId,
                                                         const bool selected)
{
    if (!m_predecessorPickerActive) {
        return false;
    }

    const model::TaskId id = QUuid::fromString(taskId.trimmed());
    const int row = candidateRow(id);
    if (id.isNull() || row < 0) {
        return false;
    }

    const bool alreadySelected = m_pickerPredecessors.contains(id);
    const bool changed = alreadySelected != selected;
    if (selected) {
        m_pickerPredecessors.insert(id);
    } else {
        m_pickerPredecessors.remove(id);
    }
    if (changed) {
        emit dataChanged(index(row), index(row), {CandidateSelectedRole});
        emit predecessorSelectionChanged();
    }
    return true;
}

void TaskEditorViewModel::acceptPredecessorSelection()
{
    if (!m_predecessorPickerActive) {
        return;
    }

    m_predecessorPickerActive = false;
    if (m_selectedCreationPredecessors == m_pickerPredecessors) {
        return;
    }
    m_selectedCreationPredecessors = m_pickerPredecessors;
    emit predecessorSelectionChanged();
    setErrorMessage({});
    updateFormState();
}

void TaskEditorViewModel::cancelPredecessorSelection()
{
    if (!m_predecessorPickerActive) {
        return;
    }
    const bool selectionChanged = m_pickerPredecessors != m_selectedCreationPredecessors;
    m_pickerPredecessors = m_selectedCreationPredecessors;
    m_predecessorPickerActive = false;
    if (selectionChanged) {
        notifyCandidateSelectionChanged();
        emit predecessorSelectionChanged();
    }
}

void TaskEditorViewModel::clearCreationPredecessors()
{
    if (m_predecessorPickerActive) {
        if (m_pickerPredecessors.isEmpty()) {
            return;
        }
        m_pickerPredecessors.clear();
        emit predecessorSelectionChanged();
        notifyCandidateSelectionChanged();
        return;
    }
    if (m_selectedCreationPredecessors.isEmpty()) {
        return;
    }
    m_selectedCreationPredecessors.clear();
    m_pickerPredecessors.clear();
    emit predecessorSelectionChanged();
    notifyCandidateSelectionChanged();
    setErrorMessage({});
    updateFormState();
}

bool TaskEditorViewModel::save()
{
    // 命令入口自身也必须守住状态，不能只依赖 Widget 将保存按钮设为不可用。
    updateFormState();
    if (!m_dirty) {
        setErrorMessage(QStringLiteral("没有需要保存的更改。"));
        return false;
    }
    if (!m_canSave) {
        setErrorMessage(m_validationMessage);
        return false;
    }

    const auto draft = buildTaskDraft();
    if (!draft.has_value()) {
        return false;
    }

    model::TaskResult result;
    if (m_editMode) {
        const auto id = QUuid::fromString(m_taskId);
        if (id.isNull()) {
            setErrorMessage(taskErrorMessage(model::TaskError::NotFound));
            return false;
        }
        result = m_taskService.updateTask(id, *draft);
    } else {
        // 创建草稿与全部前置稳定 ID 一次交给 Service，禁止逐边保存产生部分成功。
        QList<model::TaskId> predecessorIds = m_selectedCreationPredecessors.values();
        std::sort(predecessorIds.begin(), predecessorIds.end(),
                  [](const model::TaskId &left, const model::TaskId &right) {
                      return left.toString(QUuid::WithoutBraces)
                          < right.toString(QUuid::WithoutBraces);
                  });
        result = m_taskService.createTask(model::TaskCreationRequest{
            *draft,
            std::move(predecessorIds),
        });
    }

    if (!result.ok()) {
        setErrorMessage(taskErrorMessage(result.error));
        return false;
    }

    // 成功后以 Service 返回快照建立新检查点；先关闭会话状态，再发送 saved 流程信号。
    m_taskId = result.value->id().toString(QUuid::WithoutBraces);
    m_editMode = true;
    if (m_currentStatus != result.value->status()) {
        m_currentStatus = result.value->status();
        emit currentStatusTextChanged();
    }
    emit modeChanged();
    rememberCurrentDraft();
    updateFormState();
    setErrorMessage({});
    setSessionActive(false);
    emit saved(m_taskId);
    return true;
}

void TaskEditorViewModel::cancel()
{
    setErrorMessage({});
    // 主表单取消后立即丢弃字段、候选与已接受依赖，下一次打开不会继承旧草稿。
    replaceCandidates({});
    replaceDraft(defaultSnapshot(), {}, false, model::TaskStatus::Todo);
    setSessionActive(false);
    emit cancelled();
}

void TaskEditorViewModel::setSessionActive(const bool active)
{
    if (m_sessionActive == active) {
        return;
    }
    m_sessionActive = active;
    emit sessionActiveChanged();
}

TaskEditorViewModel::Snapshot TaskEditorViewModel::currentSnapshot() const
{
    return {
        m_title,
        m_description,
        m_priorityIndex,
        m_deadline,
        m_estimatedMinutes,
        m_categoryId,
        m_selectedCreationPredecessors,
    };
}

TaskEditorViewModel::Snapshot TaskEditorViewModel::defaultSnapshot()
{
    Snapshot snapshot;
    snapshot.priorityIndex = taskPriorityIndex(model::TaskPriority::Normal);
    return snapshot;
}

void TaskEditorViewModel::replaceDraft(const Snapshot &draft, const QString &taskId,
                                       const bool editMode,
                                       const model::TaskStatus currentStatus)
{
    // 一次替换全部字段后集中通知，Widget 可用 QSignalBlocker 做程序性回填。
    m_taskId = taskId;
    m_editMode = editMode;
    m_title = draft.title;
    m_description = draft.description;
    m_currentStatus = currentStatus;
    m_priorityIndex = draft.priorityIndex;
    m_deadline = draft.deadline;
    m_estimatedMinutes = draft.estimatedMinutes;
    m_categoryId = draft.categoryId;
    m_selectedCreationPredecessors = draft.predecessorIds;
    m_pickerPredecessors = m_selectedCreationPredecessors;
    m_predecessorPickerActive = false;
    m_original = draft;

    emit modeChanged();
    emit titleChanged();
    emit descriptionChanged();
    emit currentStatusTextChanged();
    emit priorityIndexChanged();
    emit deadlineChanged();
    emit estimatedDurationChanged();
    emit categoryChanged();
    emit predecessorSelectionChanged();
    notifyCandidateSelectionChanged();
    setErrorMessage({});
    updateFormState();
}

void TaskEditorViewModel::replaceCandidates(QList<model::Task> candidates)
{
    // 候选集合整体变化使用 reset；单项勾选只用 dataChanged。
    beginResetModel();
    m_predecessorCandidates = std::move(candidates);
    endResetModel();
    emit predecessorCandidatesChanged();
}

void TaskEditorViewModel::notifyCandidateSelectionChanged()
{
    if (!m_predecessorCandidates.isEmpty()) {
        emit dataChanged(index(0), index(m_predecessorCandidates.size() - 1),
                         {CandidateSelectedRole});
    }
}

void TaskEditorViewModel::rememberCurrentDraft()
{
    m_original = currentSnapshot();
}

void TaskEditorViewModel::updateFormState()
{
    // 类型化选择器已经消除了格式解析；标题、预计范围等业务规则仍由
    // TaskService::validateDraft 统一给出结论。
    QString validationMessage;
    const auto priority = taskPriorityFromIndex(m_priorityIndex);
    if (!priority.has_value()) {
        validationMessage = taskErrorMessage(model::TaskError::InvalidPriority);
    } else {
        const auto validation = m_taskService.validateDraft(
            makeDraft(m_title, m_description, *priority,
                      m_deadline, m_estimatedMinutes, m_categoryId));
        if (!validation.ok()) {
            validationMessage = taskErrorMessage(validation.error);
        }
    }
    const bool dirty = currentSnapshot() != m_original;
    const bool canSave = dirty && validationMessage.isEmpty();
    if (m_dirty == dirty && m_canSave == canSave
        && m_validationMessage == validationMessage) {
        return;
    }

    m_dirty = dirty;
    m_canSave = canSave;
    m_validationMessage = validationMessage;
    // 一个聚合通知覆盖三个派生 getter，保证 dirty/canSave/文案来自同一次计算。
    emit formStateChanged();
}

void TaskEditorViewModel::setErrorMessage(const QString &message)
{
    // 一次性错误通知与 errorMessage 可观察属性分开，重复属性值不重复通知绑定。
    if (!message.isEmpty()) {
        emit notificationRaised({smartmate::common::UiSeverity::Error,
                                 QStringLiteral("任务编辑失败"),
                                 message});
    }
    if (m_errorMessage == message) {
        return;
    }
    m_errorMessage = message;
    emit errorMessageChanged();
}

std::optional<model::TaskDraft> TaskEditorViewModel::buildTaskDraft()
{
    updateFormState();
    if (!m_validationMessage.isEmpty()) {
        setErrorMessage(m_validationMessage);
        return std::nullopt;
    }

    const auto priority = taskPriorityFromIndex(m_priorityIndex);
    if (!priority.has_value()) {
        setErrorMessage(taskErrorMessage(model::TaskError::InvalidPriority));
        return std::nullopt;
    }
    return makeDraft(m_title, m_description, *priority,
                     m_deadline, m_estimatedMinutes, m_categoryId);
}

std::optional<QDateTime> TaskEditorViewModel::displayedDeadline() const
{
    if (!m_deadline.has_value()) {
        return std::nullopt;
    }
    return m_deadline->toTimeZone(m_timeZone);
}

int TaskEditorViewModel::candidateRow(const model::TaskId &taskId) const
{
    for (int row = 0; row < m_predecessorCandidates.size(); ++row) {
        if (m_predecessorCandidates.at(row).id() == taskId) {
            return row;
        }
    }
    return -1;
}

void TaskEditorViewModel::applyCategories()
{
    emit categoryOptionsChanged();

    // 若数据库中的原类别已被管理命令删除，持久化快照已经自动转为未分类；
    // 同步原始草稿可避免把该外部变化误判为用户编辑。
    if (m_editMode && m_original.categoryId.has_value()) {
        const auto originalId = *m_original.categoryId;
        const bool originalStillExists = std::any_of(
            m_categorySource.categories().cbegin(),
            m_categorySource.categories().cend(),
            [&originalId](const auto &category) {
                return category.id == originalId;
            });
        if (!originalStillExists) {
            m_original.categoryId.reset();
        }
    }

    // 删除类别是管理操作例外；若当前草稿引用了被删除类别，只清空类别字段，
    // 其他输入仍留在本地，避免用户丢失尚未保存的编辑内容。
    if (m_categoryId.has_value() && selectedCategory() == nullptr) {
        m_categoryId.reset();
        emit categoryChanged();
        updateFormState();
        setErrorMessage(QStringLiteral("原选择的类别已删除，任务将保存为未分类。"));
    }
    if (!m_predecessorCandidates.isEmpty()) {
        emit dataChanged(index(0), index(m_predecessorCandidates.size() - 1),
                         {CandidateCategoryNameRole, CandidateCategoryAccentRole,
                          CandidateHasCategoryRole});
    }
}

} // namespace smartmate::viewmodel
