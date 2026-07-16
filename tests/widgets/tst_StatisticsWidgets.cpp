#include "view/widgets/statistics/StatisticsPage.h"
#include "view/widgets/theme/WidgetTheme.h"
#include "viewmodel/contracts/StatisticsContract.h"

#include <QAbstractBarSeries>
#include <QBarCategoryAxis>
#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QChartView>
#include <QDate>
#include <QFrame>
#include <QHorizontalBarSeries>
#include <QLabel>
#include <QPieSeries>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTest>
#include <QToolTip>

using namespace smartmate;

namespace {

QList<QHash<int, QVariant>> makeTrendRows(const int count, const bool weekly = false)
{
    QList<QHash<int, QVariant>> rows;
    const QDate start{2026, 7, 10};
    rows.reserve(count);
    for (int row = 0; row < count; ++row) {
        const QDate bucketDate = start.addDays(weekly ? row * 7 : row);
        const int value = count == 30 && row >= 27 ? row - 26 : row % 3;
        const QString label = QStringLiteral("%1/%2")
                                  .arg(bucketDate.month())
                                  .arg(bucketDate.day());
        rows.append({
            {viewmodel::StatisticsTrendContract::LabelRole, label},
            {viewmodel::StatisticsTrendContract::ValueRole, value},
            {viewmodel::StatisticsTrendContract::TooltipRole,
             QStringLiteral("%1月%2日：完成 %3 次")
                 .arg(bucketDate.month()).arg(bucketDate.day()).arg(value)},
            {viewmodel::StatisticsTrendContract::CurrentRole, row == count - 1},
            {viewmodel::StatisticsTrendContract::AccessibleTextRole,
             QStringLiteral("%1月%2日完成 %3 次")
                 .arg(bucketDate.month()).arg(bucketDate.day()).arg(value)},
        });
    }
    return rows;
}

class MutableRows final : public QAbstractListModel {
public:
    explicit MutableRows(QObject *parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : rows.size();
    }
    QVariant data(const QModelIndex &index, const int role) const override
    {
        return index.isValid() && index.row() >= 0 && index.row() < rows.size()
            ? rows.at(index.row()).value(role) : QVariant{};
    }
    void replace(QList<QHash<int, QVariant>> replacement)
    {
        beginResetModel();
        rows = std::move(replacement);
        endResetModel();
    }
    void setValue(const int row, const int role, const QVariant &value)
    {
        rows[row][role] = value;
        emit dataChanged(index(row, 0), index(row, 0), {role});
    }
    QList<QHash<int, QVariant>> rows;
};

class FakeStatistics final : public viewmodel::StatisticsContract {
public:
    FakeStatistics()
        : StatisticsContract(nullptr)
        , trendRows(this)
        , categoryRows(this)
        , healthRows(this)
    {
        trendRows.rows = makeTrendRows(7);
        categoryRows.rows = {
            {{viewmodel::StatisticsCategoryContract::LabelRole, QStringLiteral("学习")},
             {viewmodel::StatisticsCategoryContract::ValueRole, 4},
             {viewmodel::StatisticsCategoryContract::ColorRole,
              viewmodel::StatisticsCategoryContract::Blue},
             {viewmodel::StatisticsCategoryContract::TooltipRole,
              QStringLiteral("学习：完成 4 次")},
             {viewmodel::StatisticsCategoryContract::AccessibleTextRole,
              QStringLiteral("学习完成 4 次")}},
            {{viewmodel::StatisticsCategoryContract::LabelRole, QStringLiteral("未分类")},
             {viewmodel::StatisticsCategoryContract::ValueRole, 2},
             {viewmodel::StatisticsCategoryContract::ColorRole,
              viewmodel::StatisticsCategoryContract::Unclassified},
             {viewmodel::StatisticsCategoryContract::TooltipRole,
              QStringLiteral("未分类：完成 2 次")},
             {viewmodel::StatisticsCategoryContract::AccessibleTextRole,
              QStringLiteral("未分类完成 2 次")}},
        };
        const QList<QString> labels{QStringLiteral("可执行"), QStringLiteral("被阻塞"),
                                    QStringLiteral("即将到期"), QStringLiteral("已经逾期")};
        for (int row = 0; row < labels.size(); ++row) {
            healthRows.rows.append({
                {viewmodel::StatisticsHealthContract::TypeRole, row},
                {viewmodel::StatisticsHealthContract::LabelRole, labels.at(row)},
                {viewmodel::StatisticsHealthContract::ValueRole, row + 1},
                {viewmodel::StatisticsHealthContract::MaximumRole, 8},
                {viewmodel::StatisticsHealthContract::DescriptionRole,
                 QStringLiteral("健康说明 %1").arg(row)},
                {viewmodel::StatisticsHealthContract::SemanticRole,
                 row == 0 ? Positive : Risk},
                {viewmodel::StatisticsHealthContract::AccessibleTextRole,
                 QStringLiteral("%1 %2 项").arg(labels.at(row)).arg(row + 1)},
            });
        }
    }

    QAbstractItemModel *trend() noexcept override { return &trendRows; }
    QAbstractItemModel *categories() noexcept override { return &categoryRows; }
    QAbstractItemModel *health() noexcept override { return &healthRows; }
    TrendRange range() const noexcept override { return selectedRange; }
    int todayCount() const noexcept override { return today; }
    QString todayComparisonText() const override { return QStringLiteral("较昨日同期增加 1 次"); }
    SemanticTone todaySemantic() const noexcept override { return Positive; }
    int weekCount() const noexcept override { return week; }
    QString weekComparisonText() const override { return QStringLiteral("与上周同期持平"); }
    SemanticTone weekSemantic() const noexcept override { return Neutral; }
    bool hasOnTimeRate() const noexcept override { return rateAvailable; }
    qreal onTimeRate() const noexcept override { return rate; }
    QString onTimeRateText() const override { return rateAvailable ? QStringLiteral("75%") : QStringLiteral("—"); }
    QString onTimeDetailText() const override { return rateAvailable ? QStringLiteral("3 / 4 次按时完成") : QStringLiteral("暂无截止任务"); }
    int overdueCount() const noexcept override { return overdue; }
    int urgentOverdueCount() const noexcept override { return urgent; }
    QString overdueDetailText() const override { return QStringLiteral("其中 1 项为紧急任务"); }
    int activeTaskCount() const noexcept override { return 8; }
    QString periodSummary() const override { return QStringLiteral("最近 7 天完成 6 次，与此前 7 天持平"); }
    bool hasCompletionHistory() const noexcept override { return history; }
    QString emptyStateText() const override { return emptyText; }
    bool setRange(const TrendRange range) override
    {
        ++rangeCalls;
        lastRequestedRange = range;
        if (rejectRange) return false;
        if (selectedRange != range) {
            selectedRange = range;
            emit rangeChanged();
        }
        return true;
    }
    void reload() override { ++reloadCalls; }
    void announceStatistics() { emit statisticsChanged(); }

    MutableRows trendRows;
    MutableRows categoryRows;
    MutableRows healthRows;
    TrendRange selectedRange{Last7Days};
    TrendRange lastRequestedRange{Last7Days};
    int today{2};
    int week{6};
    int overdue{2};
    int urgent{1};
    qreal rate{0.75};
    bool rateAvailable{true};
    bool history{true};
    bool rejectRange{false};
    QString emptyText;
    int rangeCalls{0};
    int reloadCalls{0};
};

template<typename T>
T *required(QObject &root, const char *objectName)
{
    T *result = root.findChild<T *>(QString::fromLatin1(objectName));
    Q_ASSERT(result != nullptr);
    return result;
}

QBarSeries *trendSeries(view::widgets::StatisticsPage &page)
{
    QChartView *view = required<QChartView>(page, "trendChartView");
    for (QAbstractSeries *series : view->chart()->series()) {
        if (auto *bars = qobject_cast<QBarSeries *>(series)) return bars;
    }
    return nullptr;
}

QBarCategoryAxis *trendAxis(view::widgets::StatisticsPage &page)
{
    QChartView *view = required<QChartView>(page, "trendChartView");
    for (QAbstractAxis *axis : view->chart()->axes(Qt::Horizontal)) {
        if (auto *categories = qobject_cast<QBarCategoryAxis *>(axis)) return categories;
    }
    return nullptr;
}

} // namespace

class StatisticsWidgetsTest final : public QObject {
    Q_OBJECT

private slots:
    void initialProjectionBuildsCardsChartsAndAccessibility();
    void commandsAndModelNotificationsRefreshWithoutDuplication();
    void emptyStatesThemeAndResponsiveLayoutRemainUsable();
};

void StatisticsWidgetsTest::initialProjectionBuildsCardsChartsAndAccessibility()
{
    FakeStatistics statistics;
    view::widgets::StatisticsPage page{statistics};

    QCOMPARE(required<QLabel>(page, "todayStatisticsValue")->text(), QStringLiteral("2 次"));
    QCOMPARE(required<QLabel>(page, "weekStatisticsValue")->text(), QStringLiteral("6 次"));
    QCOMPARE(required<QLabel>(page, "onTimeStatisticsValue")->text(), QStringLiteral("75%"));
    QCOMPARE(required<QLabel>(page, "overdueStatisticsValue")->text(), QStringLiteral("2 项"));

    QBarSeries *trend = trendSeries(page);
    QVERIFY(trend != nullptr);
    QCOMPARE(trend->barSets().size(), 1);
    QBarSet *set = trend->barSets().constFirst();
    QCOMPARE(set->count(), 7);
    QCOMPARE(set->at(2), 2.0);
    QVERIFY(set->isBarSelected(6));
    QVERIFY(trendAxis(page) != nullptr);
    QCOMPARE(trendAxis(page)->count(), 7);

    QChartView *categoryView = required<QChartView>(page, "categoryChartView");
    QCOMPARE(categoryView->chart()->series().size(), 2);
    auto *firstCategory = qobject_cast<QHorizontalBarSeries *>(
        categoryView->chart()->series().constFirst());
    QVERIFY(firstCategory != nullptr);
    QCOMPARE(firstCategory->barSets().constFirst()->color(), QColor{"#2563eb"});
    auto *secondCategory = qobject_cast<QHorizontalBarSeries *>(
        categoryView->chart()->series().at(1));
    QCOMPARE(secondCategory->barSets().constFirst()->color(), QColor{"#94a3b8"});
    QChartView *onTimeView = required<QChartView>(page, "onTimeChartView");
    auto *pie = qobject_cast<QPieSeries *>(onTimeView->chart()->series().constFirst());
    QVERIFY(pie != nullptr);
    QCOMPARE(pie->count(), 2);

    QCOMPARE(required<QProgressBar>(page, "healthProgress_0")->value(), 1);
    QCOMPARE(required<QProgressBar>(page, "healthProgress_3")->value(), 4);
    QCOMPARE(required<QLabel>(page, "healthAccessibleSummary")->text(),
             QStringLiteral("当前有 8 项活动任务。执行状态：1 项可执行，2 项被阻塞。"
                            "风险提醒：3 项即将到期，4 项已经逾期；"
                            "风险项可能与执行状态重叠。"));
    QVERIFY(!required<QFrame>(page, "todayStatisticsCard")
                 ->accessibleDescription().isEmpty());
    QVERIFY(!required<QChartView>(page, "trendChartView")
                 ->accessibleDescription().isEmpty());
    QVERIFY(required<QLabel>(page, "categoryAccessibleSummary")
                ->text().contains(QStringLiteral("学习完成 4 次")));

    trend->hovered(true, 2, set);
    QCOMPARE(QToolTip::text(), QStringLiteral("7月12日：完成 2 次"));
    trend->hovered(false, 2, set);
    firstCategory->hovered(true, 0, firstCategory->barSets().constFirst());
    QCOMPARE(QToolTip::text(), QStringLiteral("学习：完成 4 次"));
    firstCategory->hovered(false, 0, firstCategory->barSets().constFirst());
}

void StatisticsWidgetsTest::commandsAndModelNotificationsRefreshWithoutDuplication()
{
    FakeStatistics statistics;
    view::widgets::StatisticsPage page{statistics};

    statistics.trendRows.replace(makeTrendRows(30));
    QTest::mouseClick(required<QPushButton>(page, "last30DaysButton"), Qt::LeftButton);
    QCOMPARE(statistics.rangeCalls, 1);
    QCOMPARE(statistics.lastRequestedRange, viewmodel::StatisticsContract::Last30Days);
    QVERIFY(required<QPushButton>(page, "last30DaysButton")->isChecked());
    QTRY_COMPARE(trendSeries(page)->barSets().constFirst()->count(), 30);
    QBarSeries *thirtyDaySeries = trendSeries(page);
    QVERIFY(thirtyDaySeries != nullptr);
    QBarSet *thirtyDaySet = thirtyDaySeries->barSets().constFirst();
    QCOMPARE(thirtyDaySet->count(), 30);
    QCOMPARE(thirtyDaySet->at(27), 1.0);
    QCOMPARE(thirtyDaySet->at(28), 2.0);
    QCOMPARE(thirtyDaySet->at(29), 3.0);
    QVERIFY(thirtyDaySet->isBarSelected(29));
    QBarCategoryAxis *thirtyDayAxis = trendAxis(page);
    QVERIFY(thirtyDayAxis != nullptr);
    QCOMPARE(thirtyDayAxis->count(), 30);
    int visibleLabelCount = 0;
    for (QString label : thirtyDayAxis->categories()) {
        label.remove(QChar{0x200B});
        if (!label.isEmpty()) ++visibleLabelCount;
    }
    QCOMPARE(visibleLabelCount, 7);

    statistics.rejectRange = true;
    QTest::mouseClick(required<QPushButton>(page, "last12WeeksButton"), Qt::LeftButton);
    QCOMPARE(statistics.rangeCalls, 2);
    QCOMPARE(statistics.selectedRange, viewmodel::StatisticsContract::Last30Days);
    QVERIFY(required<QPushButton>(page, "last30DaysButton")->isChecked());
    QVERIFY(!required<QPushButton>(page, "last12WeeksButton")->isChecked());

    QTest::mouseClick(required<QPushButton>(page, "statisticsRefreshButton"), Qt::LeftButton);
    QCOMPARE(statistics.reloadCalls, 1);

    statistics.today = 5;
    statistics.trendRows.setValue(0, viewmodel::StatisticsTrendContract::ValueRole, 9);
    statistics.announceStatistics();
    QTRY_COMPARE(required<QLabel>(page, "todayStatisticsValue")->text(),
                 QStringLiteral("5 次"));
    QBarSeries *trend = trendSeries(page);
    QVERIFY(trend != nullptr);
    QCOMPARE(trend->barSets().constFirst()->at(0), 9.0);
    QCOMPARE(trend->barSets().constFirst()->count(), 30);
    QCOMPARE(trendAxis(page)->count(), 30);

    statistics.announceStatistics();
    QTRY_COMPARE(required<QChartView>(page, "trendChartView")
                     ->chart()->series().size(), 1);
    QCOMPARE(trendSeries(page)->barSets().constFirst()->count(), 30);

    page.show();
    QCoreApplication::processEvents();
    page.hide();
    page.show();
    QCoreApplication::processEvents();
    QCOMPARE(statistics.reloadCalls, 2);

    statistics.rejectRange = false;
    statistics.trendRows.replace(makeTrendRows(12, true));
    QTest::mouseClick(required<QPushButton>(page, "last12WeeksButton"), Qt::LeftButton);
    QTRY_COMPARE(trendSeries(page)->barSets().constFirst()->count(), 12);
    QCOMPARE(trendAxis(page)->count(), 12);
    QVERIFY(trendSeries(page)->barSets().constFirst()->isBarSelected(11));
    for (const QString &label : trendAxis(page)->categories()) {
        QVERIFY(!label.contains(QChar{0x200B}));
    }
}

void StatisticsWidgetsTest::emptyStatesThemeAndResponsiveLayoutRemainUsable()
{
    FakeStatistics statistics;
    statistics.history = false;
    statistics.rateAvailable = false;
    statistics.emptyText = QStringLiteral("完成新任务后开始生成趋势");
    statistics.categoryRows.rows.clear();
    view::widgets::StatisticsPage page{statistics};

    QCOMPARE(required<QLabel>(page, "trendEmptyState")->text(),
             QStringLiteral("完成新任务后开始生成趋势"));
    QCOMPARE(required<QLabel>(page, "categoryEmptyState")->text(),
             QStringLiteral("完成新任务后开始生成趋势"));
    QCOMPARE(required<QLabel>(page, "onTimeStatisticsValue")->text(), QStringLiteral("—"));
    auto *pie = qobject_cast<QPieSeries *>(
        required<QChartView>(page, "onTimeChartView")->chart()->series().constFirst());
    QCOMPARE(pie->count(), 1);

    statistics.history = true;
    statistics.emptyText = QStringLiteral("该周期暂无完成记录");
    statistics.announceStatistics();
    QTRY_COMPARE(required<QLabel>(page, "trendEmptyState")->text(),
                 QStringLiteral("该周期暂无完成记录"));
    QCOMPARE(required<QProgressBar>(page, "healthProgress_0")->value(), 1);

    const auto blueTheme = view::widgets::WidgetTheme::fromAccentIndex(1);
    page.setPalette(blueTheme.palette());
    QCoreApplication::processEvents();
    QWidget *content = required<QWidget>(page, "statisticsContent");
    QCOMPARE(page.viewport()->objectName(), QStringLiteral("statisticsViewport"));
    QCOMPARE(page.viewport()->backgroundRole(), QPalette::Window);
    QCOMPARE(content->backgroundRole(), QPalette::Window);
    QVERIFY(page.viewport()->autoFillBackground());
    QVERIFY(content->autoFillBackground());
    QCOMPARE(page.viewport()->palette().color(page.viewport()->backgroundRole()),
             blueTheme.background);
    QCOMPARE(content->palette().color(content->backgroundRole()),
             blueTheme.background);
    QTRY_COMPARE(trendSeries(page)->barSets().constFirst()->color(),
                 blueTheme.primarySoft);

    page.resize(1180, 760);
    page.show();
    QCoreApplication::processEvents();
    QFrame *todayCard = required<QFrame>(page, "todayStatisticsCard");
    QFrame *onTimeCard = required<QFrame>(page, "onTimeStatisticsCard");
    QCOMPARE(todayCard->geometry().top(), onTimeCard->geometry().top());
    page.resize(900, 620);
    QCoreApplication::processEvents();
    QVERIFY(onTimeCard->geometry().top() > todayCard->geometry().top());
    QVERIFY(page.verticalScrollBar()->maximum() > 0);
    page.ensureWidgetVisible(required<QFrame>(page, "categoryStatisticsCard"));
    QVERIFY(page.verticalScrollBar()->value() > 0);
}

QTEST_MAIN(StatisticsWidgetsTest)
#include "tst_StatisticsWidgets.moc"
