#pragma once

#include "common/presentation/UiNotification.h"

#include <QAbstractListModel>
#include <QObject>
#include <QString>

namespace smartmate::viewmodel {

/// 完成趋势的只读列表契约；日期分桶已由 Model 计算，Role 只承载展示投影。
class StatisticsTrendContract : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role {
        LabelRole = Qt::UserRole + 1,
        ValueRole,
        TooltipRole,
        CurrentRole,
        AccessibleTextRole,
    };
    Q_ENUM(Role)

    ~StatisticsTrendContract() override = default;

protected:
    explicit StatisticsTrendContract(QObject *parent = nullptr)
        : QAbstractListModel(parent)
    {
    }
};

/// 完成类别构成的只读列表契约；颜色值是稳定 View 边界枚举而非领域枚举。
class StatisticsCategoryContract : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role {
        LabelRole = Qt::UserRole + 1,
        ValueRole,
        ColorRole,
        TooltipRole,
        AccessibleTextRole,
    };
    Q_ENUM(Role)

    enum Color {
        Blue = 0,
        Teal,
        Green,
        Amber,
        Orange,
        Rose,
        Violet,
        Slate,
        Unclassified,
        Other,
    };
    Q_ENUM(Color)

    ~StatisticsCategoryContract() override = default;

protected:
    explicit StatisticsCategoryContract(QObject *parent = nullptr)
        : QAbstractListModel(parent)
    {
    }
};

/// 当前任务健康的只读列表契约；maximum 用于普通 Widgets 进度行显示相对规模。
class StatisticsHealthContract : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role {
        TypeRole = Qt::UserRole + 1,
        LabelRole,
        ValueRole,
        MaximumRole,
        DescriptionRole,
        SemanticRole,
        AccessibleTextRole,
    };
    Q_ENUM(Role)

    enum Type {
        Executable = 0,
        Blocked,
        DueSoon,
        Overdue,
    };
    Q_ENUM(Type)

    ~StatisticsHealthContract() override = default;

protected:
    explicit StatisticsHealthContract(QObject *parent = nullptr)
        : QAbstractListModel(parent)
    {
    }
};

/// 统计仪表盘的页面级抽象契约；只暴露展示状态、强类型命令和稳定子模型。
class StatisticsContract : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *trend READ trend CONSTANT)
    Q_PROPERTY(QAbstractItemModel *categories READ categories CONSTANT)
    Q_PROPERTY(QAbstractItemModel *health READ health CONSTANT)
    Q_PROPERTY(TrendRange range READ range NOTIFY rangeChanged)
    Q_PROPERTY(int todayCount READ todayCount NOTIFY statisticsChanged)
    Q_PROPERTY(QString todayComparisonText READ todayComparisonText NOTIFY statisticsChanged)
    Q_PROPERTY(SemanticTone todaySemantic READ todaySemantic NOTIFY statisticsChanged)
    Q_PROPERTY(int weekCount READ weekCount NOTIFY statisticsChanged)
    Q_PROPERTY(QString weekComparisonText READ weekComparisonText NOTIFY statisticsChanged)
    Q_PROPERTY(SemanticTone weekSemantic READ weekSemantic NOTIFY statisticsChanged)
    Q_PROPERTY(bool hasOnTimeRate READ hasOnTimeRate NOTIFY statisticsChanged)
    Q_PROPERTY(qreal onTimeRate READ onTimeRate NOTIFY statisticsChanged)
    Q_PROPERTY(QString onTimeRateText READ onTimeRateText NOTIFY statisticsChanged)
    Q_PROPERTY(QString onTimeDetailText READ onTimeDetailText NOTIFY statisticsChanged)
    Q_PROPERTY(int overdueCount READ overdueCount NOTIFY statisticsChanged)
    Q_PROPERTY(int urgentOverdueCount READ urgentOverdueCount NOTIFY statisticsChanged)
    Q_PROPERTY(QString overdueDetailText READ overdueDetailText NOTIFY statisticsChanged)
    Q_PROPERTY(int activeTaskCount READ activeTaskCount NOTIFY statisticsChanged)
    Q_PROPERTY(QString periodSummary READ periodSummary NOTIFY statisticsChanged)
    Q_PROPERTY(bool hasCompletionHistory READ hasCompletionHistory NOTIFY statisticsChanged)
    Q_PROPERTY(QString emptyStateText READ emptyStateText NOTIFY statisticsChanged)

public:
    enum TrendRange {
        Last7Days = 0,
        Last30Days,
        Last12Weeks,
    };
    Q_ENUM(TrendRange)

    enum SemanticTone {
        Neutral = 0,
        Positive,
        Risk,
    };
    Q_ENUM(SemanticTone)

    ~StatisticsContract() override = default;

    [[nodiscard]] virtual QAbstractItemModel *trend() noexcept = 0;
    [[nodiscard]] virtual QAbstractItemModel *categories() noexcept = 0;
    [[nodiscard]] virtual QAbstractItemModel *health() noexcept = 0;
    [[nodiscard]] virtual TrendRange range() const noexcept = 0;
    [[nodiscard]] virtual int todayCount() const noexcept = 0;
    [[nodiscard]] virtual QString todayComparisonText() const = 0;
    [[nodiscard]] virtual SemanticTone todaySemantic() const noexcept = 0;
    [[nodiscard]] virtual int weekCount() const noexcept = 0;
    [[nodiscard]] virtual QString weekComparisonText() const = 0;
    [[nodiscard]] virtual SemanticTone weekSemantic() const noexcept = 0;
    [[nodiscard]] virtual bool hasOnTimeRate() const noexcept = 0;
    [[nodiscard]] virtual qreal onTimeRate() const noexcept = 0;
    [[nodiscard]] virtual QString onTimeRateText() const = 0;
    [[nodiscard]] virtual QString onTimeDetailText() const = 0;
    [[nodiscard]] virtual int overdueCount() const noexcept = 0;
    [[nodiscard]] virtual int urgentOverdueCount() const noexcept = 0;
    [[nodiscard]] virtual QString overdueDetailText() const = 0;
    [[nodiscard]] virtual int activeTaskCount() const noexcept = 0;
    [[nodiscard]] virtual QString periodSummary() const = 0;
    [[nodiscard]] virtual bool hasCompletionHistory() const noexcept = 0;
    [[nodiscard]] virtual QString emptyStateText() const = 0;

public slots:
    /// 切换趋势范围；查询失败时保持原范围和最后成功投影。
    virtual bool setRange(TrendRange range) = 0;
    /// 使用当前范围重新查询，不改变会话筛选。
    virtual void reload() = 0;

signals:
    void rangeChanged();
    void statisticsChanged();
    void notificationRaised(const smartmate::common::UiNotification &notification);

protected:
    explicit StatisticsContract(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
};

} // namespace smartmate::viewmodel
