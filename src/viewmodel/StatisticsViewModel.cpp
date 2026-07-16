#include "StatisticsViewModel.h"

#include "model/services/StatisticsService.h"
#include "model/services/TaskService.h"

#include <QAbstractItemModel>
#include <QtMath>

#include <chrono>
#include <limits>
#include <utility>

namespace smartmate::viewmodel {
namespace {

model::StatisticsRange toModelRange(StatisticsContract::TrendRange range)
{
    switch (range) {
    case StatisticsContract::Last7Days:
        return model::StatisticsRange::Last7Days;
    case StatisticsContract::Last30Days:
        return model::StatisticsRange::Last30Days;
    case StatisticsContract::Last12Weeks:
        return model::StatisticsRange::Last12Weeks;
    }
    return model::StatisticsRange::Last7Days;
}

StatisticsContract::SemanticTone toTone(model::StatisticsChangeSemantic semantic)
{
    switch (semantic) {
    case model::StatisticsChangeSemantic::Positive:
        return StatisticsContract::Positive;
    case model::StatisticsChangeSemantic::Risk:
        return StatisticsContract::Risk;
    case model::StatisticsChangeSemantic::Neutral:
        return StatisticsContract::Neutral;
    }
    return StatisticsContract::Neutral;
}

QString comparisonText(const model::StatisticsComparison &comparison,
                       const QString &previousPeriod)
{
    if (comparison.delta > 0) {
        return QStringLiteral("较%1增加 %2 次").arg(previousPeriod).arg(comparison.delta);
    }
    if (comparison.delta < 0) {
        return QStringLiteral("较%1减少 %2 次").arg(previousPeriod).arg(-comparison.delta);
    }
    return QStringLiteral("与%1持平").arg(previousPeriod);
}

QString rangeName(StatisticsContract::TrendRange range)
{
    switch (range) {
    case StatisticsContract::Last7Days:
        return QStringLiteral("7 天");
    case StatisticsContract::Last30Days:
        return QStringLiteral("30 天");
    case StatisticsContract::Last12Weeks:
        return QStringLiteral("12 周");
    }
    return QStringLiteral("7 天");
}

QList<StatisticsTrendRow> makeTrendRows(const model::StatisticsSnapshot &snapshot)
{
    QList<StatisticsTrendRow> rows;
    rows.reserve(snapshot.trend.size());
    const bool weekly = snapshot.range == model::StatisticsRange::Last12Weeks;
    for (const auto &bucket : snapshot.trend) {
        const QString label = QStringLiteral("%1/%2")
                                  .arg(bucket.localStartDate.month())
                                  .arg(bucket.localStartDate.day());
        const QString dateText = weekly
            ? QStringLiteral("%1—%2")
                  .arg(bucket.localStartDate.toString(
                           QStringLiteral("yyyy年M月d日")),
                       bucket.localEndDate.toString(
                           QStringLiteral("yyyy年M月d日")))
            : QStringLiteral("%1月%2日")
                  .arg(bucket.localStartDate.month())
                  .arg(bucket.localStartDate.day());
        const QString summary = QStringLiteral("%1：完成 %2 次")
                                    .arg(dateText)
                                    .arg(bucket.completionCount);
        rows.push_back({label,
                        bucket.completionCount,
                        summary,
                        bucket.current,
                        summary});
    }
    return rows;
}

StatisticsCategoryContract::Color categoryColor(
    const model::StatisticsCategoryBucket &bucket)
{
    if (bucket.kind == model::StatisticsCategoryKind::Unclassified) {
        return StatisticsCategoryContract::Unclassified;
    }
    if (bucket.kind == model::StatisticsCategoryKind::Other) {
        return StatisticsCategoryContract::Other;
    }
    if (!bucket.categoryColor.has_value()) {
        return StatisticsCategoryContract::Unclassified;
    }
    return static_cast<StatisticsCategoryContract::Color>(
        static_cast<int>(*bucket.categoryColor));
}

QList<StatisticsCategoryRow> makeCategoryRows(const model::StatisticsSnapshot &snapshot)
{
    QList<StatisticsCategoryRow> rows;
    rows.reserve(snapshot.categories.size());
    for (const auto &bucket : snapshot.categories) {
        QString label = bucket.categoryName;
        if (bucket.kind == model::StatisticsCategoryKind::Unclassified) {
            label = QStringLiteral("未分类");
        } else if (bucket.kind == model::StatisticsCategoryKind::Other) {
            label = QStringLiteral("其他");
        }
        const QString summary = QStringLiteral("%1：完成 %2 次")
                                    .arg(label)
                                    .arg(bucket.completionCount);
        rows.push_back({label,
                        bucket.completionCount,
                        categoryColor(bucket),
                        summary,
                        summary});
    }
    return rows;
}

QList<StatisticsHealthRow> makeHealthRows(const model::StatisticsSnapshot &snapshot)
{
    const int maximum = qMax(snapshot.health.activeTaskCount, 1);
    return {
        {StatisticsHealthContract::Executable,
         QStringLiteral("可执行"),
         snapshot.health.executableCount,
         maximum,
         QStringLiteral("当前可以开始的待办任务"),
         StatisticsContract::Positive,
         QStringLiteral("可执行 %1 项。当前可以开始的待办任务。")
             .arg(snapshot.health.executableCount)},
        {StatisticsHealthContract::Blocked,
         QStringLiteral("被阻塞"),
         snapshot.health.blockedCount,
         maximum,
         QStringLiteral("前置任务尚未解析"),
         StatisticsContract::Risk,
         QStringLiteral("被阻塞 %1 项。前置任务尚未解析。")
             .arg(snapshot.health.blockedCount)},
        {StatisticsHealthContract::DueSoon,
         QStringLiteral("即将到期"),
         snapshot.health.dueSoonCount,
         maximum,
         QStringLiteral("三个本地自然日内到期"),
         StatisticsContract::Risk,
         QStringLiteral("即将到期 %1 项。三个本地自然日内到期。")
             .arg(snapshot.health.dueSoonCount)},
        {StatisticsHealthContract::Overdue,
         QStringLiteral("已经逾期"),
         snapshot.health.overdueCount,
         maximum,
         QStringLiteral("截止时间已经超过"),
         StatisticsContract::Risk,
         QStringLiteral("已经逾期 %1 项。截止时间已经超过。")
             .arg(snapshot.health.overdueCount)},
    };
}

} // namespace

StatisticsViewModel::StatisticsViewModel(model::StatisticsService &statisticsService,
                                         model::TaskService &taskService,
                                         QObject *parent)
    : StatisticsViewModel(statisticsService,
                          taskService,
                          [] { return QDateTime::currentDateTimeUtc(); },
                          [] { return QTimeZone::systemTimeZone(); },
                          parent)
{
}

StatisticsViewModel::StatisticsViewModel(model::StatisticsService &statisticsService,
                                         model::TaskService &taskService,
                                         NowProvider nowProvider,
                                         TimeZoneProvider timeZoneProvider,
                                         QObject *parent)
    : StatisticsContract(parent)
    , m_statisticsService(statisticsService)
    , m_nowProvider(std::move(nowProvider))
    , m_timeZoneProvider(std::move(timeZoneProvider))
    , m_trendModel(new StatisticsTrendListModel(this))
    , m_categoryModel(new StatisticsCategoryListModel(this))
    , m_healthModel(new StatisticsHealthListModel(this))
    , m_invalidationTimer(new QTimer(this))
    , m_midnightTimer(new QTimer(this))
{
    m_invalidationTimer->setObjectName(QStringLiteral("statisticsInvalidationTimer"));
    m_invalidationTimer->setSingleShot(true);
    m_invalidationTimer->setInterval(0);
    m_midnightTimer->setObjectName(QStringLiteral("statisticsMidnightTimer"));
    m_midnightTimer->setSingleShot(true);

    connect(m_invalidationTimer, &QTimer::timeout, this, &StatisticsViewModel::reload);
    connect(m_midnightTimer, &QTimer::timeout, this, &StatisticsViewModel::reload);
    connect(&taskService,
            &model::TaskService::tasksChanged,
            this,
            &StatisticsViewModel::scheduleInvalidationReload);
    connect(&taskService,
            &model::TaskService::dependenciesChanged,
            this,
            &StatisticsViewModel::scheduleInvalidationReload);

    reload();
}

QAbstractItemModel *StatisticsViewModel::trend() noexcept { return m_trendModel; }
QAbstractItemModel *StatisticsViewModel::categories() noexcept { return m_categoryModel; }
QAbstractItemModel *StatisticsViewModel::health() noexcept { return m_healthModel; }
StatisticsContract::TrendRange StatisticsViewModel::range() const noexcept { return m_range; }
int StatisticsViewModel::todayCount() const noexcept { return m_snapshot ? m_snapshot->today.currentCount : 0; }
QString StatisticsViewModel::todayComparisonText() const { return m_snapshot ? comparisonText(m_snapshot->today, QStringLiteral("昨日同期")) : QString(); }
StatisticsContract::SemanticTone StatisticsViewModel::todaySemantic() const noexcept { return m_snapshot ? toTone(m_snapshot->today.semantic) : Neutral; }
int StatisticsViewModel::weekCount() const noexcept { return m_snapshot ? m_snapshot->thisWeek.currentCount : 0; }
QString StatisticsViewModel::weekComparisonText() const { return m_snapshot ? comparisonText(m_snapshot->thisWeek, QStringLiteral("上周同期")) : QString(); }
StatisticsContract::SemanticTone StatisticsViewModel::weekSemantic() const noexcept { return m_snapshot ? toTone(m_snapshot->thisWeek.semantic) : Neutral; }
bool StatisticsViewModel::hasOnTimeRate() const noexcept { return m_snapshot && m_snapshot->deadlineCompletionCount > 0; }
qreal StatisticsViewModel::onTimeRate() const noexcept { return m_snapshot ? m_snapshot->onTimeRate().value_or(0.0) : 0.0; }
QString StatisticsViewModel::onTimeRateText() const { return hasOnTimeRate() ? QStringLiteral("%1%").arg(qRound(onTimeRate() * 100.0)) : QStringLiteral("—"); }
QString StatisticsViewModel::onTimeDetailText() const { return hasOnTimeRate() ? QStringLiteral("%1 / %2 次按时完成").arg(m_snapshot->onTimeCompletionCount).arg(m_snapshot->deadlineCompletionCount) : QStringLiteral("暂无截止任务"); }
int StatisticsViewModel::overdueCount() const noexcept { return m_snapshot ? m_snapshot->health.overdueCount : 0; }
int StatisticsViewModel::urgentOverdueCount() const noexcept { return m_snapshot ? m_snapshot->health.urgentOverdueCount : 0; }
QString StatisticsViewModel::overdueDetailText() const
{
    if (overdueCount() == 0) {
        return QStringLiteral("当前没有逾期任务");
    }
    if (urgentOverdueCount() > 0) {
        return QStringLiteral("其中 %1 项为紧急任务").arg(urgentOverdueCount());
    }
    return QStringLiteral("没有紧急逾期任务");
}
int StatisticsViewModel::activeTaskCount() const noexcept { return m_snapshot ? m_snapshot->health.activeTaskCount : 0; }
QString StatisticsViewModel::periodSummary() const
{
    if (!m_snapshot) {
        return {};
    }
    const QString period = rangeName(m_range);
    const auto &comparison = m_snapshot->selectedPeriod;
    if (comparison.delta > 0) {
        return QStringLiteral("最近 %1完成 %2 次，比此前 %1增加 %3 次")
            .arg(period).arg(comparison.currentCount).arg(comparison.delta);
    }
    if (comparison.delta < 0) {
        return QStringLiteral("最近 %1完成 %2 次，比此前 %1减少 %3 次")
            .arg(period).arg(comparison.currentCount).arg(-comparison.delta);
    }
    return QStringLiteral("最近 %1完成 %2 次，与此前 %1持平")
        .arg(period).arg(comparison.currentCount);
}
bool StatisticsViewModel::hasCompletionHistory() const noexcept { return m_snapshot && m_snapshot->hasCompletionHistory; }
QString StatisticsViewModel::emptyStateText() const
{
    if (!m_snapshot || !m_snapshot->hasCompletionHistory) {
        return QStringLiteral("完成新任务后开始生成趋势");
    }
    if (m_snapshot->selectedPeriod.currentCount == 0) {
        return QStringLiteral("该周期暂无完成记录");
    }
    return {};
}

bool StatisticsViewModel::setRange(TrendRange range)
{
    if (range == m_range) {
        return true;
    }
    return queryAndCommit(range, true);
}

void StatisticsViewModel::reload()
{
    (void) queryAndCommit(m_range, false);
}

bool StatisticsViewModel::queryAndCommit(TrendRange requestedRange,
                                         bool commitRangeChange)
{
    const QDateTime nowUtc = m_nowProvider();
    const QTimeZone timeZone = m_timeZoneProvider();
    const model::StatisticsResult result = m_statisticsService.snapshot(
        {toModelRange(requestedRange), nowUtc, timeZone});
    scheduleMidnightReload();
    if (!result.ok()) {
        raiseLoadFailure();
        return false;
    }

    const bool projectionChanged = applySnapshot(*result.value);
    if (commitRangeChange) {
        m_range = requestedRange;
        emit rangeChanged();
    }
    if (projectionChanged || commitRangeChange) {
        emit statisticsChanged();
    }
    return true;
}

bool StatisticsViewModel::applySnapshot(model::StatisticsSnapshot snapshot)
{
    if (m_snapshot && *m_snapshot == snapshot) {
        return false;
    }
    const bool trendChanged = m_trendModel->replaceRows(makeTrendRows(snapshot));
    const bool categoryChanged = m_categoryModel->replaceRows(makeCategoryRows(snapshot));
    const bool healthChanged = m_healthModel->replaceRows(makeHealthRows(snapshot));
    const bool pageChanged = !m_snapshot || *m_snapshot != snapshot;
    m_snapshot = std::move(snapshot);
    return pageChanged || trendChanged || categoryChanged || healthChanged;
}

void StatisticsViewModel::scheduleInvalidationReload()
{
    if (!m_invalidationTimer->isActive()) {
        m_invalidationTimer->start();
    }
}

void StatisticsViewModel::scheduleMidnightReload()
{
    const QDateTime nowUtc = m_nowProvider();
    const QTimeZone timeZone = m_timeZoneProvider();
    if (!nowUtc.isValid() || !timeZone.isValid()) {
        m_midnightTimer->stop();
        return;
    }
    const QDateTime localNow = nowUtc.toTimeZone(timeZone);
    const QDateTime nextMidnight(localNow.date().addDays(1),
                                 QTime(0, 0),
                                 timeZone);
    const qint64 milliseconds = qMax<qint64>(1, nowUtc.msecsTo(nextMidnight.toUTC()));
    m_midnightTimer->start(static_cast<int>(qMin<qint64>(
        milliseconds, std::numeric_limits<int>::max())));
}

void StatisticsViewModel::raiseLoadFailure()
{
    emit notificationRaised({common::UiSeverity::Error,
                             QStringLiteral("统计加载失败"),
                             QStringLiteral("统计数据暂时无法读取，请稍后重试。")});
}

} // namespace smartmate::viewmodel
