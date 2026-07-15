#include "TaskCategoryViewModel.h"

#include "TaskCategoryPresentation.h"
#include "domain/TaskCategoryConstraints.h"
#include "services/TaskCategoryResult.h"
#include "services/TaskCategoryService.h"

#include <QUuid>

namespace smartmate::viewmodel {
namespace {

[[nodiscard]] QString categoryErrorMessage(const model::TaskCategoryError error)
{
    switch (error) {
    case model::TaskCategoryError::None: return {};
    case model::TaskCategoryError::EmptyName:
        return QStringLiteral("类别名称不能为空。");
    case model::TaskCategoryError::NameTooLong:
        return QStringLiteral("类别名称不能超过%1个字符。")
            .arg(model::TaskCategoryConstraints::maximumNameLength);
    case model::TaskCategoryError::DuplicateName:
        return QStringLiteral("已存在同名类别。");
    case model::TaskCategoryError::InvalidColor:
        return QStringLiteral("所选类别颜色无效。");
    case model::TaskCategoryError::NotFound:
        return QStringLiteral("类别不存在或已被删除。");
    case model::TaskCategoryError::PersistenceFailure:
        return QStringLiteral("类别数据访问失败，请稍后重试。");
    }
    return QStringLiteral("类别操作失败。");
}

} // namespace

TaskCategoryViewModel::TaskCategoryViewModel(
    model::TaskCategoryService *categoryService,
    TaskPlanProjectionSource &planSource,
    TaskCategoryProjectionSource &categorySource,
    QObject *parent)
    : TaskCategoryContract(parent)
    , m_categoryService(categoryService)
    , m_planSource(planSource)
    , m_categorySource(categorySource)
{
    connect(&m_planSource, &TaskPlanProjectionSource::projectionChanged,
            this, &TaskCategoryViewModel::applyProjection);
    connect(&m_planSource, &TaskPlanProjectionSource::refreshFailed,
            this, &TaskCategoryViewModel::applyProjection);
    connect(&m_categorySource, &TaskCategoryProjectionSource::categoriesChanged,
            this, &TaskCategoryViewModel::applyProjection);
    connect(&m_categorySource, &TaskCategoryProjectionSource::refreshFailed,
            this, &TaskCategoryViewModel::applyProjection);
    applyProjection();
    beginCreate();
}

int TaskCategoryViewModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_categories.size();
}

QVariant TaskCategoryViewModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_categories.size()) {
        return {};
    }
    const auto &category = m_categories.at(index.row());
    switch (role) {
    case CategoryIdRole: return category.id.toString(QUuid::WithoutBraces);
    case NameRole: return category.name;
    case ColorIndexRole: return taskCategoryColorIndex(category.color);
    case AccentRole: return taskCategoryAccent(category.color);
    case TaskCountRole: return m_taskCounts.value(category.id);
    default: return {};
    }
}

QHash<int, QByteArray> TaskCategoryViewModel::roleNames() const
{
    return {{CategoryIdRole, "categoryId"}, {NameRole, "name"},
            {ColorIndexRole, "colorIndex"}, {AccentRole, "accent"},
            {TaskCountRole, "taskCount"}};
}

int TaskCategoryViewModel::count() const noexcept { return m_categories.size(); }
bool TaskCategoryViewModel::empty() const noexcept { return m_categories.isEmpty(); }
bool TaskCategoryViewModel::editMode() const noexcept { return m_editMode; }
QString TaskCategoryViewModel::editingCategoryId() const
{
    return m_editMode && !m_editingCategoryId.isNull()
        ? m_editingCategoryId.toString(QUuid::WithoutBraces)
        : QString{};
}
QString TaskCategoryViewModel::draftName() const { return m_draftName; }
int TaskCategoryViewModel::draftColorIndex() const noexcept
{ return taskCategoryColorIndex(m_draftColor); }
QStringList TaskCategoryViewModel::colorOptions() const
{ return taskCategoryColorOptions(); }
QStringList TaskCategoryViewModel::colorAccents() const
{
    QStringList accents;
    for (int index = 0; index < taskCategoryColorOptions().size(); ++index) {
        const auto color = taskCategoryColorFromIndex(index);
        if (color.has_value()) {
            accents.append(taskCategoryAccent(*color));
        }
    }
    return accents;
}
bool TaskCategoryViewModel::dirty() const noexcept
{
    return m_editMode
        ? m_draftName != m_originalName || m_draftColor != m_originalColor
        : !m_draftName.isEmpty() || m_draftColor != model::TaskCategoryColor::Blue;
}
bool TaskCategoryViewModel::canSave() const noexcept
{ return !m_draftName.trimmed().isEmpty() && (!m_editMode || dirty()); }
QString TaskCategoryViewModel::errorMessage() const { return m_errorMessage; }

void TaskCategoryViewModel::setDraftName(const QString &name)
{
    if (m_draftName == name) return;
    m_draftName = name;
    emit draftChanged();
}

void TaskCategoryViewModel::setDraftColorIndex(const int index)
{
    const auto color = taskCategoryColorFromIndex(index);
    if (!color.has_value() || m_draftColor == *color) return;
    m_draftColor = *color;
    emit draftChanged();
}

void TaskCategoryViewModel::reload()
{
    m_planSource.refresh();
    m_categorySource.refresh();
    applyProjection();
}

void TaskCategoryViewModel::applyProjection()
{
    if (m_categorySource.lastError() != model::TaskCategoryError::None) {
        setErrorMessage(categoryErrorMessage(m_categorySource.lastError()));
        return;
    }
    if (m_planSource.lastError() != model::TaskError::None) {
        setErrorMessage(QStringLiteral("任务数据访问失败，无法统计类别使用数量。"));
        return;
    }
    QHash<model::TaskCategoryId, int> counts;
    for (const model::Task &task : m_planSource.projection().tasks) {
        if (task.categoryId().has_value()) ++counts[*task.categoryId()];
    }

    setErrorMessage({});
    // 类别行和聚合计数必须在同一次模型重置中发布，Widget 不会观察到错配状态。
    beginResetModel();
    m_categories = m_categorySource.categories();
    m_taskCounts = std::move(counts);
    endResetModel();
    if (!m_editingCategoryId.isNull() && rowForCategory(m_editingCategoryId) < 0) {
        beginCreate();
    }
    emit countChanged();
}

void TaskCategoryViewModel::beginCreate()
{
    m_editMode = false;
    m_editingCategoryId = model::TaskCategoryId{};
    m_draftName.clear();
    m_draftColor = model::TaskCategoryColor::Blue;
    m_originalName.clear();
    m_originalColor = m_draftColor;
    setErrorMessage({});
    emit draftChanged();
}

bool TaskCategoryViewModel::beginEdit(const QString &categoryId)
{
    const model::TaskCategoryId id{QUuid::fromString(categoryId.trimmed())};
    const int row = rowForCategory(id);
    if (id.isNull() || row < 0) {
        setErrorMessage(categoryErrorMessage(model::TaskCategoryError::NotFound));
        return false;
    }
    const auto &category = m_categories.at(row);
    m_editMode = true;
    m_editingCategoryId = category.id;
    m_draftName = category.name;
    m_draftColor = category.color;
    m_originalName = category.name;
    m_originalColor = category.color;
    setErrorMessage({});
    emit draftChanged();
    return true;
}

bool TaskCategoryViewModel::save()
{
    if (!m_categoryService) {
        setErrorMessage(QStringLiteral("类别服务尚未初始化。"));
        return false;
    }
    const auto color = taskCategoryColorFromIndex(draftColorIndex());
    if (!color.has_value()) {
        setErrorMessage(categoryErrorMessage(model::TaskCategoryError::InvalidColor));
        return false;
    }
    model::TaskCategoryResult result = m_editMode
        ? m_categoryService->updateCategory(
              m_editingCategoryId, model::TaskCategoryDraft{m_draftName, *color})
        : m_categoryService->createCategory(
              model::TaskCategoryDraft{m_draftName, *color});
    if (!result.ok()) {
        setErrorMessage(categoryErrorMessage(result.error));
        return false;
    }
    // 只有 Service 命令成功才更新原值检查点并发送 saved；实际列表刷新由 Service 通知触发。
    const QString id = result.value->id.toString(QUuid::WithoutBraces);
    m_editMode = true;
    m_editingCategoryId = result.value->id;
    m_draftName = result.value->name;
    m_draftColor = result.value->color;
    m_originalName = m_draftName;
    m_originalColor = m_draftColor;
    setErrorMessage({});
    emit draftChanged();
    emit saved(id);
    return true;
}

bool TaskCategoryViewModel::deleteCategory(const QString &categoryId)
{
    if (!m_categoryService) {
        setErrorMessage(QStringLiteral("类别服务尚未初始化。"));
        return false;
    }
    const model::TaskCategoryId id{QUuid::fromString(categoryId.trimmed())};
    if (id.isNull()) {
        setErrorMessage(categoryErrorMessage(model::TaskCategoryError::NotFound));
        return false;
    }
    const auto result = m_categoryService->deleteCategory(id);
    if (!result.ok()) {
        setErrorMessage(categoryErrorMessage(result.error));
        return false;
    }
    const int unassigned = result.value->unassignedTaskCount;
    // deleted 是对话流程结果；类别和任务投影刷新仍依赖 Service 的两类失效通知。
    if (m_editingCategoryId == id) beginCreate();
    setErrorMessage({});
    emit deleted(categoryId, unassigned);
    return true;
}

void TaskCategoryViewModel::cancel()
{
    beginCreate();
    emit cancelled();
}

void TaskCategoryViewModel::clearError() { setErrorMessage({}); }

int TaskCategoryViewModel::rowForCategory(const model::TaskCategoryId &id) const
{
    for (int row = 0; row < m_categories.size(); ++row) {
        if (m_categories.at(row).id == id) return row;
    }
    return -1;
}

QString TaskCategoryViewModel::categoryErrorText(const int error) const
{
    return categoryErrorMessage(static_cast<model::TaskCategoryError>(error));
}

void TaskCategoryViewModel::setErrorMessage(const QString &message)
{
    // 一次性通知与可重读错误属性分开；重复属性值不重复发 errorMessageChanged。
    if (!message.isEmpty()) {
        emit notificationRaised({smartmate::common::UiSeverity::Error,
                                 QStringLiteral("类别操作失败"),
                                 message});
    }
    if (m_errorMessage == message) return;
    m_errorMessage = message;
    emit errorMessageChanged();
}

} // namespace smartmate::viewmodel
