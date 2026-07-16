#pragma once

#include "viewmodel/contracts/StatisticsContract.h"

#include <QList>
#include <QString>

namespace smartmate::viewmodel {

struct StatisticsTrendRow final {
    QString label;
    int value{0};
    QString tooltip;
    bool current{false};
    QString accessibleText;
    friend bool operator==(const StatisticsTrendRow &,
                           const StatisticsTrendRow &) = default;
};

struct StatisticsCategoryRow final {
    QString label;
    int value{0};
    StatisticsCategoryContract::Color color{StatisticsCategoryContract::Other};
    QString tooltip;
    QString accessibleText;
    friend bool operator==(const StatisticsCategoryRow &,
                           const StatisticsCategoryRow &) = default;
};

struct StatisticsHealthRow final {
    StatisticsHealthContract::Type type{StatisticsHealthContract::Executable};
    QString label;
    int value{0};
    int maximum{0};
    QString description;
    StatisticsContract::SemanticTone semantic{StatisticsContract::Neutral};
    QString accessibleText;
    friend bool operator==(const StatisticsHealthRow &,
                           const StatisticsHealthRow &) = default;
};

class StatisticsTrendListModel final : public StatisticsTrendContract {
    Q_OBJECT
public:
    explicit StatisticsTrendListModel(QObject *parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    /// 数据实际变化时才按完整快照重置模型。
    bool replaceRows(QList<StatisticsTrendRow> rows);

private:
    QList<StatisticsTrendRow> m_rows;
};

class StatisticsCategoryListModel final : public StatisticsCategoryContract {
    Q_OBJECT
public:
    explicit StatisticsCategoryListModel(QObject *parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    bool replaceRows(QList<StatisticsCategoryRow> rows);

private:
    QList<StatisticsCategoryRow> m_rows;
};

class StatisticsHealthListModel final : public StatisticsHealthContract {
    Q_OBJECT
public:
    explicit StatisticsHealthListModel(QObject *parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    bool replaceRows(QList<StatisticsHealthRow> rows);

private:
    QList<StatisticsHealthRow> m_rows;
};

} // namespace smartmate::viewmodel
