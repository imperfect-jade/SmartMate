#include "TaskCategoryViewModel.h"

#include "TaskCategoryPresentation.h"
#include "services/TaskCategoryResult.h"
#include "services/TaskCategoryService.h"
#include "services/TaskService.h"

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
        return QStringLiteral("类别名称不能超过50个字符。");
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
    model::TaskService &taskService,
    model::TaskCategoryService *categoryService,
    QObject *parent)
    : QAbstractListModel(parent)
    , m_taskService(taskService)
    , m_categoryService(categoryService)
{
    if (m_categoryService) {
        connect(m_categoryService, &model::TaskCategoryService::categoriesChanged,
                this, &TaskCategoryViewModel::reload);
        connect(m_categoryService,
                &model::TaskCategoryService::taskCategoryAssignmentsChanged,
                this, &TaskCategoryViewModel::reload);
    }
    connect(&m_taskService, &model::TaskService::tasksChanged,
            this, &TaskCategoryViewModel::reload);
    reload();
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
        accents.append(taskCategoryAccent(
            static_cast<model::TaskCategoryColor>(index)));
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
    QList<model::TaskCategory> categories;
    if (m_categoryService) {
        const auto categoryResult = m_categoryService->listCategories();
        if (!categoryResult.ok()) {
            setErrorMessage(categoryErrorMessage(categoryResult.error));
            return;
        }
        categories = *categoryResult.value;
    }

    QHash<model::TaskCategoryId, int> counts;
    const auto taskResult = m_taskService.listTasks();
    if (!taskResult.ok()) {
        // 删除确认依赖准确使用数；读取失败时保留旧投影，不能把未知误报为零。
        setErrorMessage(QStringLiteral("任务数据访问失败，无法统计类别使用数量。"));
        return;
    }
    for (const model::Task &task : *taskResult.value) {
        if (task.categoryId().has_value()) ++counts[*task.categoryId()];
    }

    setErrorMessage({});
    beginResetModel();
    m_categories = std::move(categories);
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
    if (m_errorMessage == message) return;
    m_errorMessage = message;
    emit errorMessageChanged();
}

} // namespace smartmate::viewmodel
