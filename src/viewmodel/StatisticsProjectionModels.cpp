#include "StatisticsProjectionModels.h"

#include <utility>

namespace smartmate::viewmodel {
namespace {

template<typename Row>
bool validRow(const QList<Row> &rows, const QModelIndex &index)
{
    return index.isValid() && index.row() >= 0 && index.row() < rows.size();
}

} // namespace

StatisticsTrendListModel::StatisticsTrendListModel(QObject *parent)
    : StatisticsTrendContract(parent)
{
}

int StatisticsTrendListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant StatisticsTrendListModel::data(const QModelIndex &index, const int role) const
{
    if (!validRow(m_rows, index)) return {};
    const auto &row = m_rows.at(index.row());
    switch (role) {
    case LabelRole: return row.label;
    case ValueRole: return row.value;
    case TooltipRole: return row.tooltip;
    case CurrentRole: return row.current;
    case AccessibleTextRole: return row.accessibleText;
    default: return {};
    }
}

QHash<int, QByteArray> StatisticsTrendListModel::roleNames() const
{
    return {{LabelRole, "label"}, {ValueRole, "value"},
            {TooltipRole, "tooltip"}, {CurrentRole, "current"},
            {AccessibleTextRole, "accessibleText"}};
}

bool StatisticsTrendListModel::replaceRows(QList<StatisticsTrendRow> rows)
{
    if (m_rows == rows) return false;
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
    return true;
}

StatisticsCategoryListModel::StatisticsCategoryListModel(QObject *parent)
    : StatisticsCategoryContract(parent)
{
}

int StatisticsCategoryListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant StatisticsCategoryListModel::data(const QModelIndex &index,
                                            const int role) const
{
    if (!validRow(m_rows, index)) return {};
    const auto &row = m_rows.at(index.row());
    switch (role) {
    case LabelRole: return row.label;
    case ValueRole: return row.value;
    case ColorRole: return static_cast<int>(row.color);
    case TooltipRole: return row.tooltip;
    case AccessibleTextRole: return row.accessibleText;
    default: return {};
    }
}

QHash<int, QByteArray> StatisticsCategoryListModel::roleNames() const
{
    return {{LabelRole, "label"}, {ValueRole, "value"},
            {ColorRole, "color"}, {TooltipRole, "tooltip"},
            {AccessibleTextRole, "accessibleText"}};
}

bool StatisticsCategoryListModel::replaceRows(QList<StatisticsCategoryRow> rows)
{
    if (m_rows == rows) return false;
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
    return true;
}

StatisticsHealthListModel::StatisticsHealthListModel(QObject *parent)
    : StatisticsHealthContract(parent)
{
}

int StatisticsHealthListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant StatisticsHealthListModel::data(const QModelIndex &index,
                                          const int role) const
{
    if (!validRow(m_rows, index)) return {};
    const auto &row = m_rows.at(index.row());
    switch (role) {
    case TypeRole: return static_cast<int>(row.type);
    case LabelRole: return row.label;
    case ValueRole: return row.value;
    case MaximumRole: return row.maximum;
    case DescriptionRole: return row.description;
    case SemanticRole: return static_cast<int>(row.semantic);
    case AccessibleTextRole: return row.accessibleText;
    default: return {};
    }
}

QHash<int, QByteArray> StatisticsHealthListModel::roleNames() const
{
    return {{TypeRole, "type"}, {LabelRole, "label"},
            {ValueRole, "value"}, {MaximumRole, "maximum"},
            {DescriptionRole, "description"}, {SemanticRole, "semantic"},
            {AccessibleTextRole, "accessibleText"}};
}

bool StatisticsHealthListModel::replaceRows(QList<StatisticsHealthRow> rows)
{
    if (m_rows == rows) return false;
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
    return true;
}

} // namespace smartmate::viewmodel
