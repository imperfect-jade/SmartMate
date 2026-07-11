#include "TaskEditorViewModel.h"

#include "TaskErrorMapper.h"
#include "domain/TaskConstraints.h"
#include "services/TaskService.h"

#include <QDateTime>
#include <QStringList>

#include <utility>

namespace smartmate::viewmodel {

namespace {
// ComboBox 索引只在展示层转换为领域枚举；生成的草稿仍须由 TaskService
// 执行完整业务校验。
[[nodiscard]] model::TaskDraft makeDraft(const QString &title,
                                         const QString &description,
                                         const int statusIndex,
                                         const int priorityIndex,
                                         std::optional<QDateTime> deadline,
                                         std::optional<int> estimatedMinutes)
{
    model::TaskDraft draft;
    draft.title = title;
    draft.description = description;
    draft.status = static_cast<model::TaskStatus>(statusIndex);
    draft.priority = static_cast<model::TaskPriority>(priorityIndex);
    draft.deadline = std::move(deadline);
    draft.estimatedMinutes = estimatedMinutes;
    return draft;
}
}

TaskEditorViewModel::TaskEditorViewModel(model::TaskService &taskService, QObject *parent)
    : TaskEditorViewModel(taskService, QTimeZone::systemTimeZone(), parent)
{
}

TaskEditorViewModel::TaskEditorViewModel(model::TaskService &taskService,
                                         QTimeZone timeZone,
                                         QObject *parent)
    : QObject(parent)
    , m_taskService(taskService)
    , m_timeZone(std::move(timeZone))
{
    rememberCurrentDraft();
    updateFormState();
}

QString TaskEditorViewModel::taskId() const
{
    return m_taskId;
}

bool TaskEditorViewModel::editMode() const noexcept
{
    return m_editMode;
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

int TaskEditorViewModel::statusIndex() const noexcept
{
    return m_statusIndex;
}

void TaskEditorViewModel::setStatusIndex(const int statusIndex)
{
    if (m_statusIndex == statusIndex) {
        return;
    }
    m_statusIndex = statusIndex;
    emit statusIndexChanged();
    setErrorMessage({});
    updateFormState();
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
    return deadline.has_value()
        ? deadline->toString(QStringLiteral("yyyy-MM-dd HH:mm"))
        : QStringLiteral("未设置");
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

QStringList TaskEditorViewModel::statusOptions() const
{
    return {
        QStringLiteral("待办"),
        QStringLiteral("进行中"),
        QStringLiteral("已完成"),
        QStringLiteral("已取消"),
        QStringLiteral("已归档"),
    };
}

QStringList TaskEditorViewModel::priorityOptions() const
{
    return {
        QStringLiteral("低"),
        QStringLiteral("普通"),
        QStringLiteral("高"),
        QStringLiteral("紧急"),
    };
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

void TaskEditorViewModel::beginCreate()
{
    // 新建和编辑都只替换 ViewModel 草稿，不提前写入 Model。
    replaceDraft(Snapshot{}, {}, false);
}

bool TaskEditorViewModel::beginEdit(const QString &taskId)
{
    const auto id = QUuid::fromString(taskId.trimmed());
    if (id.isNull()) {
        setErrorMessage(taskErrorMessage(model::TaskError::NotFound));
        return false;
    }

    const auto result = m_taskService.findTask(id);
    if (!result.ok()) {
        setErrorMessage(taskErrorMessage(result.error));
        return false;
    }

    const auto &task = *result.value;
    Snapshot draft;
    draft.title = task.title();
    draft.description = task.description();
    draft.statusIndex = static_cast<int>(task.status());
    draft.priorityIndex = static_cast<int>(task.priority());
    draft.deadline = task.deadline();
    draft.estimatedMinutes = task.estimatedMinutes();

    replaceDraft(draft, task.id().toString(QUuid::WithoutBraces), true);
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
    if (totalMinutes < model::TaskConstraints::minimumEstimatedMinutes
        || totalMinutes > model::TaskConstraints::maximumEstimatedMinutes) {
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

bool TaskEditorViewModel::save()
{
    // 命令入口自身也必须守住状态，不能只依赖 QML 将保存按钮设为不可用。
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
        result = m_taskService.createTask(*draft);
    }

    if (!result.ok()) {
        setErrorMessage(taskErrorMessage(result.error));
        return false;
    }

    m_taskId = result.value->id().toString(QUuid::WithoutBraces);
    m_editMode = true;
    emit modeChanged();
    rememberCurrentDraft();
    updateFormState();
    setErrorMessage({});
    emit saved(m_taskId);
    return true;
}

void TaskEditorViewModel::cancel()
{
    setErrorMessage({});
    emit cancelled();
}

TaskEditorViewModel::Snapshot TaskEditorViewModel::currentSnapshot() const
{
    return {
        m_title,
        m_description,
        m_statusIndex,
        m_priorityIndex,
        m_deadline,
        m_estimatedMinutes,
    };
}

void TaskEditorViewModel::replaceDraft(const Snapshot &draft, const QString &taskId,
                                       const bool editMode)
{
    m_taskId = taskId;
    m_editMode = editMode;
    m_title = draft.title;
    m_description = draft.description;
    m_statusIndex = draft.statusIndex;
    m_priorityIndex = draft.priorityIndex;
    m_deadline = draft.deadline;
    m_estimatedMinutes = draft.estimatedMinutes;
    m_original = draft;

    emit modeChanged();
    emit titleChanged();
    emit descriptionChanged();
    emit statusIndexChanged();
    emit priorityIndexChanged();
    emit deadlineChanged();
    emit estimatedDurationChanged();
    setErrorMessage({});
    updateFormState();
}

void TaskEditorViewModel::rememberCurrentDraft()
{
    m_original = currentSnapshot();
}

void TaskEditorViewModel::updateFormState()
{
    // 类型化选择器已经消除了格式解析；标题、预计范围、状态等业务规则仍由
    // TaskService::validateDraft 统一给出结论。
    QString validationMessage;
    const auto validation = m_taskService.validateDraft(
        makeDraft(m_title, m_description, m_statusIndex, m_priorityIndex,
                  m_deadline, m_estimatedMinutes));
    if (!validation.ok()) {
        validationMessage = taskErrorMessage(validation.error);
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
    emit formStateChanged();
}

void TaskEditorViewModel::setErrorMessage(const QString &message)
{
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

    return makeDraft(m_title, m_description, m_statusIndex, m_priorityIndex,
                     m_deadline, m_estimatedMinutes);
}

std::optional<QDateTime> TaskEditorViewModel::displayedDeadline() const
{
    if (!m_deadline.has_value()) {
        return std::nullopt;
    }
    return m_deadline->toTimeZone(m_timeZone);
}

} // namespace smartmate::viewmodel
