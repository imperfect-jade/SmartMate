#pragma once

#include "StatisticsProjectionModels.h"
#include "model/statistics/StatisticsTypes.h"
#include "viewmodel/contracts/StatisticsContract.h"

#include <QDateTime>
#include <QTimeZone>
#include <QTimer>

#include <functional>
#include <optional>

namespace smartmate::model {
class StatisticsService;
class TaskService;
}

namespace smartmate::viewmodel {

/// 将 StatisticsService 快照格式化为页面状态；日期边界和业务统计仍完全由 Model 负责。
class StatisticsViewModel final : public StatisticsContract {
    Q_OBJECT

public:
    using NowProvider = std::function<QDateTime()>;
    using TimeZoneProvider = std::function<QTimeZone()>;

    StatisticsViewModel(model::StatisticsService &statisticsService,
                        model::TaskService &taskService,
                        QObject *parent = nullptr);
    /// 可注入时钟与时区的构造仅用于确定性测试和宿主环境适配。
    StatisticsViewModel(model::StatisticsService &statisticsService,
                        model::TaskService &taskService,
                        NowProvider nowProvider,
                        TimeZoneProvider timeZoneProvider,
                        QObject *parent = nullptr);

    [[nodiscard]] QAbstractItemModel *trend() noexcept override;
    [[nodiscard]] QAbstractItemModel *categories() noexcept override;
    [[nodiscard]] QAbstractItemModel *health() noexcept override;
    [[nodiscard]] TrendRange range() const noexcept override;
    [[nodiscard]] int todayCount() const noexcept override;
    [[nodiscard]] QString todayComparisonText() const override;
    [[nodiscard]] SemanticTone todaySemantic() const noexcept override;
    [[nodiscard]] int weekCount() const noexcept override;
    [[nodiscard]] QString weekComparisonText() const override;
    [[nodiscard]] SemanticTone weekSemantic() const noexcept override;
    [[nodiscard]] bool hasOnTimeRate() const noexcept override;
    [[nodiscard]] qreal onTimeRate() const noexcept override;
    [[nodiscard]] QString onTimeRateText() const override;
    [[nodiscard]] QString onTimeDetailText() const override;
    [[nodiscard]] int overdueCount() const noexcept override;
    [[nodiscard]] int urgentOverdueCount() const noexcept override;
    [[nodiscard]] QString overdueDetailText() const override;
    [[nodiscard]] int activeTaskCount() const noexcept override;
    [[nodiscard]] QString periodSummary() const override;
    [[nodiscard]] bool hasCompletionHistory() const noexcept override;
    [[nodiscard]] QString emptyStateText() const override;

public slots:
    bool setRange(TrendRange range) override;
    void reload() override;

private:
    [[nodiscard]] bool queryAndCommit(TrendRange requestedRange,
                                      bool commitRangeChange);
    [[nodiscard]] bool applySnapshot(model::StatisticsSnapshot snapshot);
    void scheduleInvalidationReload();
    void scheduleMidnightReload();
    void raiseLoadFailure();

    model::StatisticsService &m_statisticsService;
    NowProvider m_nowProvider;
    TimeZoneProvider m_timeZoneProvider;
    TrendRange m_range{Last7Days};
    std::optional<model::StatisticsSnapshot> m_snapshot;
    StatisticsTrendListModel *m_trendModel{nullptr};
    StatisticsCategoryListModel *m_categoryModel{nullptr};
    StatisticsHealthListModel *m_healthModel{nullptr};
    /// 同一任务命令的多个失效信号在事件循环尾部合并为一次查询。
    QTimer *m_invalidationTimer{nullptr};
    /// 单次计时器只负责跨越本地自然日；每次查询后都会按最新系统时区重排。
    QTimer *m_midnightTimer{nullptr};
};

} // namespace smartmate::viewmodel
