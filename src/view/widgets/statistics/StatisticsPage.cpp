#include "StatisticsPage.h"

#include "view/widgets/theme/WidgetTheme.h"

#include <QAbstractAxis>
#include <QAbstractBarSeries>
#include <QAbstractItemModel>
#include <QBarCategoryAxis>
#include <QBarSeries>
#include <QBarSet>
#include <QButtonGroup>
#include <QChart>
#include <QChartView>
#include <QCursor>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLegend>
#include <QHorizontalBarSeries>
#include <QPainter>
#include <QPieSeries>
#include <QPieSlice>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTimer>
#include <QToolTip>
#include <QValueAxis>
#include <QVBoxLayout>

#include <algorithm>
#include <tuple>

namespace smartmate::view::widgets {
namespace {

using StatisticsContract = viewmodel::StatisticsContract;
using TrendContract = viewmodel::StatisticsTrendContract;
using CategoryContract = viewmodel::StatisticsCategoryContract;
using HealthContract = viewmodel::StatisticsHealthContract;

QFrame *card(const QString &objectName, const QString &title,
             QWidget *parent, QVBoxLayout **layoutOut = nullptr)
{
    auto *result = new QFrame(parent);
    result->setObjectName(objectName);
    result->setFrameShape(QFrame::StyledPanel);
    auto *layout = new QVBoxLayout(result);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(8);
    auto *heading = new QLabel(title, result);
    heading->setObjectName(QStringLiteral("statisticsCardTitle"));
    layout->addWidget(heading);
    if (layoutOut != nullptr) {
        *layoutOut = layout;
    }
    return result;
}

QLabel *valueLabel(const QString &objectName, QWidget *parent)
{
    auto *result = new QLabel(parent);
    result->setObjectName(objectName);
    result->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return result;
}

QLabel *detailLabel(const QString &objectName, QWidget *parent)
{
    auto *result = new QLabel(parent);
    result->setObjectName(objectName);
    result->setWordWrap(true);
    return result;
}

void clearLayout(QLayout &layout)
{
    while (QLayoutItem *item = layout.takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            delete widget;
        }
        delete item;
    }
}

void detachLayoutItems(QLayout &layout)
{
    while (QLayoutItem *item = layout.takeAt(0)) {
        delete item;
    }
}

StatisticsContract::SemanticTone tone(const QVariant &value)
{
    return static_cast<StatisticsContract::SemanticTone>(value.toInt());
}

QColor toneColor(const WidgetTheme &theme,
                 const StatisticsContract::SemanticTone value)
{
    switch (value) {
    case StatisticsContract::Positive: return theme.done;
    case StatisticsContract::Risk: return theme.danger;
    case StatisticsContract::Neutral: return theme.textSecondary;
    }
    return theme.textSecondary;
}

QString toneStyle(const QColor &color, const bool prominent = false)
{
    return QStringLiteral("color: %1; font-weight: %2;")
        .arg(color.name(), prominent ? QStringLiteral("700")
                                     : QStringLiteral("600"));
}

void clearChart(QChart &chart)
{
    const QList<QAbstractSeries *> series = chart.series();
    for (QAbstractSeries *item : series) {
        chart.removeSeries(item);
        delete item;
    }
    const QList<QAbstractAxis *> axes = chart.axes();
    for (QAbstractAxis *axis : axes) {
        chart.removeAxis(axis);
        delete axis;
    }
}

void configureChart(QChart &chart)
{
    chart.setAnimationOptions(QChart::NoAnimation);
    chart.setBackgroundVisible(false);
    chart.setPlotAreaBackgroundVisible(false);
    chart.setDropShadowEnabled(false);
    chart.setMargins({6, 6, 6, 6});
    chart.legend()->hide();
}

void styleAxes(QChart &chart, const WidgetTheme &theme, const QFont &font)
{
    for (QAbstractAxis *axis : chart.axes()) {
        axis->setLabelsBrush(theme.textSecondary);
        axis->setLabelsFont(font);
        axis->setLinePen(QPen{theme.border});
        axis->setGridLinePen(QPen{theme.borderSoft});
    }
}

QString joinedAccessibleRows(QAbstractItemModel &model, const int role)
{
    QStringList rows;
    rows.reserve(model.rowCount());
    for (int row = 0; row < model.rowCount(); ++row) {
        rows.append(model.index(row, 0).data(role).toString());
    }
    return rows.join(QStringLiteral("；"));
}

} // namespace

StatisticsPage::StatisticsPage(viewmodel::StatisticsContract &statistics,
                               QWidget *parent)
    : QScrollArea(parent)
    , m_statistics(statistics)
    , m_content(new QWidget)
    , m_overviewLayout(new QGridLayout)
    , m_sectionsLayout(new QGridLayout)
    , m_todayCard(nullptr)
    , m_weekCard(nullptr)
    , m_onTimeCard(nullptr)
    , m_overdueCard(nullptr)
    , m_todayValue(nullptr)
    , m_todayDetail(nullptr)
    , m_weekValue(nullptr)
    , m_weekDetail(nullptr)
    , m_onTimeValue(nullptr)
    , m_onTimeDetail(nullptr)
    , m_overdueValue(nullptr)
    , m_overdueDetail(nullptr)
    , m_last7Days(new QPushButton(tr("近 7 天"), m_content))
    , m_last30Days(new QPushButton(tr("近 30 天"), m_content))
    , m_last12Weeks(new QPushButton(tr("近 12 周"), m_content))
    , m_refresh(new QPushButton(tr("刷新"), m_content))
    , m_trendCard(nullptr)
    , m_categoryCard(nullptr)
    , m_healthCard(nullptr)
    , m_trendStack(new QStackedWidget(m_content))
    , m_categoryStack(new QStackedWidget(m_content))
    , m_trendEmpty(new QLabel(m_trendStack))
    , m_categoryEmpty(new QLabel(m_categoryStack))
    , m_trendSummary(new QLabel(m_content))
    , m_categorySummary(new QLabel(m_content))
    , m_healthSummary(new QLabel(m_content))
    , m_healthRows(nullptr)
    , m_trendChart(new QChart)
    , m_trendView(new QChartView(m_trendChart, m_trendStack))
    , m_trendSeries(nullptr)
    , m_categoryChart(new QChart)
    , m_categoryView(new QChartView(m_categoryChart, m_categoryStack))
    , m_onTimeChart(new QChart)
    , m_onTimeView(nullptr)
    , m_onTimeSeries(new QPieSeries)
    , m_refreshTimer(new QTimer(this))
{
    setObjectName(QStringLiteral("statisticsPage"));
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // QScrollArea 的 viewport 默认使用 Base role，主题切换时会露出普通表面色。
    // 页面空白区与内容区统一使用 Window role，卡片仍由 WidgetTheme 使用 surface。
    viewport()->setObjectName(QStringLiteral("statisticsViewport"));
    viewport()->setBackgroundRole(QPalette::Window);
    viewport()->setAutoFillBackground(true);
    m_content->setObjectName(QStringLiteral("statisticsContent"));
    m_content->setBackgroundRole(QPalette::Window);
    m_content->setAutoFillBackground(true);
    setWidget(m_content);

    auto *root = new QVBoxLayout(m_content);
    root->setContentsMargins(24, 22, 24, 28);
    root->setSpacing(18);
    root->setSizeConstraint(QLayout::SetMinimumSize);

    auto *headingLayout = new QHBoxLayout;
    auto *heading = new QLabel(tr("数据统计"), m_content);
    heading->setObjectName(QStringLiteral("pageTitle"));
    auto *subtitle = new QLabel(tr("快速了解完成情况、变化趋势与当前任务健康"), m_content);
    subtitle->setObjectName(QStringLiteral("secondaryText"));
    auto *headingText = new QVBoxLayout;
    headingText->setSpacing(2);
    headingText->addWidget(heading);
    headingText->addWidget(subtitle);
    headingLayout->addLayout(headingText);
    headingLayout->addStretch();

    auto *rangeGroup = new QButtonGroup(this);
    rangeGroup->setExclusive(true);
    const QList<QPushButton *> rangeButtons{m_last7Days, m_last30Days,
                                            m_last12Weeks};
    for (int index = 0; index < rangeButtons.size(); ++index) {
        QPushButton *button = rangeButtons.at(index);
        button->setCheckable(true);
        button->setAccessibleName(button->text());
        button->setAccessibleDescription(tr("切换统计趋势周期"));
        rangeGroup->addButton(button, index);
        headingLayout->addWidget(button);
    }
    m_last7Days->setObjectName(QStringLiteral("last7DaysButton"));
    m_last30Days->setObjectName(QStringLiteral("last30DaysButton"));
    m_last12Weeks->setObjectName(QStringLiteral("last12WeeksButton"));
    m_refresh->setObjectName(QStringLiteral("statisticsRefreshButton"));
    m_refresh->setAccessibleName(tr("刷新统计"));
    headingLayout->addWidget(m_refresh);
    root->addLayout(headingLayout);

    QVBoxLayout *todayLayout = nullptr;
    m_todayCard = card(QStringLiteral("todayStatisticsCard"), tr("今日完成"),
                       m_content, &todayLayout);
    m_todayValue = valueLabel(QStringLiteral("todayStatisticsValue"), m_todayCard);
    m_todayDetail = detailLabel(QStringLiteral("todayStatisticsDetail"), m_todayCard);
    todayLayout->addWidget(m_todayValue);
    todayLayout->addWidget(m_todayDetail);
    todayLayout->addStretch();

    QVBoxLayout *weekLayout = nullptr;
    m_weekCard = card(QStringLiteral("weekStatisticsCard"), tr("本周完成"),
                      m_content, &weekLayout);
    m_weekValue = valueLabel(QStringLiteral("weekStatisticsValue"), m_weekCard);
    m_weekDetail = detailLabel(QStringLiteral("weekStatisticsDetail"), m_weekCard);
    weekLayout->addWidget(m_weekValue);
    weekLayout->addWidget(m_weekDetail);
    weekLayout->addStretch();

    QVBoxLayout *onTimeLayout = nullptr;
    m_onTimeCard = card(QStringLiteral("onTimeStatisticsCard"), tr("按时完成率"),
                        m_content, &onTimeLayout);
    auto *onTimeBody = new QHBoxLayout;
    m_onTimeView = new QChartView(m_onTimeChart, m_onTimeCard);
    m_onTimeView->setObjectName(QStringLiteral("onTimeChartView"));
    m_onTimeView->setFixedSize(104, 88);
    m_onTimeView->setRenderHint(QPainter::Antialiasing);
    m_onTimeView->setRubberBand(QChartView::NoRubberBand);
    m_onTimeView->setFrameShape(QFrame::NoFrame);
    m_onTimeValue = valueLabel(QStringLiteral("onTimeStatisticsValue"), m_onTimeCard);
    m_onTimeDetail = detailLabel(QStringLiteral("onTimeStatisticsDetail"), m_onTimeCard);
    auto *onTimeText = new QVBoxLayout;
    onTimeText->addWidget(m_onTimeValue);
    onTimeText->addWidget(m_onTimeDetail);
    onTimeText->addStretch();
    onTimeBody->addWidget(m_onTimeView);
    onTimeBody->addLayout(onTimeText, 1);
    onTimeLayout->addLayout(onTimeBody);

    QVBoxLayout *overdueLayout = nullptr;
    m_overdueCard = card(QStringLiteral("overdueStatisticsCard"), tr("当前逾期"),
                         m_content, &overdueLayout);
    m_overdueValue = valueLabel(QStringLiteral("overdueStatisticsValue"), m_overdueCard);
    m_overdueDetail = detailLabel(QStringLiteral("overdueStatisticsDetail"), m_overdueCard);
    overdueLayout->addWidget(m_overdueValue);
    overdueLayout->addWidget(m_overdueDetail);
    overdueLayout->addStretch();
    root->addLayout(m_overviewLayout);

    QVBoxLayout *trendLayout = nullptr;
    m_trendCard = card(QStringLiteral("trendStatisticsCard"), tr("完成趋势"),
                       m_content, &trendLayout);
    m_trendStack->setObjectName(QStringLiteral("trendStatisticsStack"));
    m_trendView->setObjectName(QStringLiteral("trendChartView"));
    m_trendView->setMinimumHeight(280);
    m_trendView->setRenderHint(QPainter::Antialiasing);
    m_trendView->setRubberBand(QChartView::NoRubberBand);
    m_trendView->setFrameShape(QFrame::NoFrame);
    m_trendEmpty->setObjectName(QStringLiteral("trendEmptyState"));
    m_trendEmpty->setAlignment(Qt::AlignCenter);
    m_trendEmpty->setWordWrap(true);
    m_trendStack->addWidget(m_trendView);
    m_trendStack->addWidget(m_trendEmpty);
    m_trendSummary->setObjectName(QStringLiteral("trendAccessibleSummary"));
    m_trendSummary->setWordWrap(true);
    trendLayout->addWidget(m_trendStack);
    trendLayout->addWidget(m_trendSummary);

    QVBoxLayout *categoryLayout = nullptr;
    m_categoryCard = card(QStringLiteral("categoryStatisticsCard"), tr("类别构成"),
                          m_content, &categoryLayout);
    m_categoryStack->setObjectName(QStringLiteral("categoryStatisticsStack"));
    m_categoryView->setObjectName(QStringLiteral("categoryChartView"));
    m_categoryView->setMinimumHeight(250);
    m_categoryView->setRenderHint(QPainter::Antialiasing);
    m_categoryView->setRubberBand(QChartView::NoRubberBand);
    m_categoryView->setFrameShape(QFrame::NoFrame);
    m_categoryEmpty->setObjectName(QStringLiteral("categoryEmptyState"));
    m_categoryEmpty->setAlignment(Qt::AlignCenter);
    m_categoryEmpty->setWordWrap(true);
    m_categoryStack->addWidget(m_categoryView);
    m_categoryStack->addWidget(m_categoryEmpty);
    m_categorySummary->setObjectName(QStringLiteral("categoryAccessibleSummary"));
    m_categorySummary->setWordWrap(true);
    categoryLayout->addWidget(m_categoryStack);
    categoryLayout->addWidget(m_categorySummary);

    QVBoxLayout *healthLayout = nullptr;
    m_healthCard = card(QStringLiteral("healthStatisticsCard"), tr("任务健康"),
                        m_content, &healthLayout);
    m_healthSummary->setObjectName(QStringLiteral("healthAccessibleSummary"));
    m_healthSummary->setWordWrap(true);
    healthLayout->addWidget(m_healthSummary);
    m_healthRows = new QVBoxLayout;
    m_healthRows->setSpacing(10);
    healthLayout->addLayout(m_healthRows);
    healthLayout->addStretch();
    root->addLayout(m_sectionsLayout);

    configureChart(*m_trendChart);
    configureChart(*m_categoryChart);
    configureChart(*m_onTimeChart);
    m_onTimeChart->addSeries(m_onTimeSeries);
    m_onTimeSeries->setHoleSize(0.62);
    m_onTimeSeries->setPieSize(0.92);

    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(0);
    connect(m_refreshTimer, &QTimer::timeout, this, &StatisticsPage::refreshAll);
    connect(&statistics, &StatisticsContract::statisticsChanged,
            this, &StatisticsPage::scheduleRefresh);
    connect(&statistics, &StatisticsContract::rangeChanged, this, [this] {
        syncRangeButtons();
        scheduleRefresh();
    });
    connectModel(statistics.trend());
    connectModel(statistics.categories());
    connectModel(statistics.health());
    connect(rangeGroup, &QButtonGroup::idClicked, this, [this](const int id) {
        const auto range = static_cast<StatisticsContract::TrendRange>(id);
        (void) m_statistics.setRange(range);
        // 失败时 Contract 保持旧范围，此处立即撤销按钮的临时选中状态。
        syncRangeButtons();
    });
    connect(m_refresh, &QPushButton::clicked,
            &statistics, &StatisticsContract::reload);

    refreshAll();
    applyResponsiveLayout();
}

void StatisticsPage::resizeEvent(QResizeEvent *event)
{
    QScrollArea::resizeEvent(event);
    applyResponsiveLayout();
}

void StatisticsPage::showEvent(QShowEvent *event)
{
    QScrollArea::showEvent(event);
    if (m_hasBeenShown) {
        m_statistics.reload();
    }
    m_hasBeenShown = true;
}

void StatisticsPage::changeEvent(QEvent *event)
{
    QScrollArea::changeEvent(event);
    if (event->type() == QEvent::PaletteChange
        || event->type() == QEvent::FontChange) {
        scheduleRefresh();
    }
}

void StatisticsPage::connectModel(QAbstractItemModel *model)
{
    connect(model, &QAbstractItemModel::modelReset,
            this, &StatisticsPage::scheduleRefresh);
    connect(model, &QAbstractItemModel::dataChanged,
            this, &StatisticsPage::scheduleRefresh);
    connect(model, &QAbstractItemModel::rowsInserted,
            this, &StatisticsPage::scheduleRefresh);
    connect(model, &QAbstractItemModel::rowsRemoved,
            this, &StatisticsPage::scheduleRefresh);
    connect(model, &QAbstractItemModel::rowsMoved,
            this, &StatisticsPage::scheduleRefresh);
}

void StatisticsPage::scheduleRefresh()
{
    if (!m_refreshTimer->isActive()) {
        m_refreshTimer->start();
    }
}

void StatisticsPage::refreshAll()
{
    syncRangeButtons();
    refreshOverview();
    refreshTrend();
    refreshCategories();
    refreshHealth();
    applyTheme();
}

void StatisticsPage::refreshOverview()
{
    m_todayValue->setText(QStringLiteral("%1 次").arg(m_statistics.todayCount()));
    m_todayDetail->setText(m_statistics.todayComparisonText());
    m_weekValue->setText(QStringLiteral("%1 次").arg(m_statistics.weekCount()));
    m_weekDetail->setText(m_statistics.weekComparisonText());
    m_onTimeValue->setText(m_statistics.onTimeRateText());
    m_onTimeDetail->setText(m_statistics.onTimeDetailText());
    m_overdueValue->setText(QStringLiteral("%1 项").arg(m_statistics.overdueCount()));
    m_overdueDetail->setText(m_statistics.overdueDetailText());

    const WidgetTheme theme = WidgetTheme::fromPalette(palette());
    m_todayDetail->setStyleSheet(toneStyle(
        toneColor(theme, m_statistics.todaySemantic())));
    m_weekDetail->setStyleSheet(toneStyle(
        toneColor(theme, m_statistics.weekSemantic())));
    m_overdueValue->setStyleSheet(toneStyle(
        m_statistics.overdueCount() > 0 ? theme.danger : theme.textPrimary,
        true));

    const QString todayAccessible = tr("今日完成 %1 次。%2")
        .arg(m_statistics.todayCount()).arg(m_statistics.todayComparisonText());
    const QString weekAccessible = tr("本周完成 %1 次。%2")
        .arg(m_statistics.weekCount()).arg(m_statistics.weekComparisonText());
    const QString onTimeAccessible = tr("按时完成率 %1。%2")
        .arg(m_statistics.onTimeRateText(), m_statistics.onTimeDetailText());
    const QString overdueAccessible = tr("当前逾期 %1 项，紧急逾期 %2 项。%3")
        .arg(m_statistics.overdueCount())
        .arg(m_statistics.urgentOverdueCount())
        .arg(m_statistics.overdueDetailText());
    const QList<std::tuple<QFrame *, QString, QString>> cards{
        {m_todayCard, tr("今日完成"), todayAccessible},
        {m_weekCard, tr("本周完成"), weekAccessible},
        {m_onTimeCard, tr("按时完成率"), onTimeAccessible},
        {m_overdueCard, tr("当前逾期"), overdueAccessible},
    };
    for (const auto &[cardWidget, name, description] : cards) {
        cardWidget->setAccessibleName(name);
        cardWidget->setAccessibleDescription(description);
    }
    m_onTimeView->setAccessibleName(tr("按时完成率环形图"));
    m_onTimeView->setAccessibleDescription(onTimeAccessible);

    m_onTimeSeries->clear();
    if (m_statistics.hasOnTimeRate()) {
        auto *onTime = m_onTimeSeries->append(tr("按时"), m_statistics.onTimeRate());
        auto *late = m_onTimeSeries->append(tr("未按时"),
                                            1.0 - m_statistics.onTimeRate());
        onTime->setLabelVisible(false);
        late->setLabelVisible(false);
    } else {
        auto *unavailable = m_onTimeSeries->append(tr("暂无截止任务"), 1.0);
        unavailable->setLabelVisible(false);
    }
}

void StatisticsPage::refreshTrend()
{
    clearChart(*m_trendChart);
    m_trendSeries = new QBarSeries;
    auto *set = new QBarSet(tr("完成次数"));
    m_trendTooltips.clear();
    QStringList axisLabels;
    QAbstractItemModel *model = m_statistics.trend();
    int maximum = 0;
    int currentRow = -1;
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        const int value = index.data(TrendContract::ValueRole).toInt();
        *set << value;
        maximum = std::max(maximum, value);
        const bool current = index.data(TrendContract::CurrentRole).toBool();
        if (current) currentRow = row;
        QString label = index.data(TrendContract::LabelRole).toString();
        if (m_statistics.range() == StatisticsContract::Last30Days
            && row != 0 && row != model->rowCount() - 1 && !current
            && row % 5 != 0) {
            // QBarCategoryAxis 会忽略重复类别；用不同长度的零宽字符保留
            // 30 个轴槽位，同时让非关键日期在视觉上保持为空。
            label = QString(row + 1, QChar{0x200B});
        }
        axisLabels.append(label);
        m_trendTooltips.append(index.data(TrendContract::TooltipRole).toString());
    }
    m_trendSeries->append(set);
    m_trendChart->addSeries(m_trendSeries);
    auto *categories = new QBarCategoryAxis;
    categories->append(axisLabels);
    if (m_statistics.range() == StatisticsContract::Last30Days) {
        categories->setLabelsAngle(-55);
    }
    auto *values = new QValueAxis;
    values->setRange(0, std::max(1, maximum));
    values->setLabelFormat(QStringLiteral("%d"));
    values->setTickInterval(1.0);
    values->setMinorTickCount(0);
    m_trendChart->addAxis(categories, Qt::AlignBottom);
    m_trendChart->addAxis(values, Qt::AlignLeft);
    m_trendSeries->attachAxis(categories);
    m_trendSeries->attachAxis(values);
    if (currentRow >= 0) {
        set->setBarSelected(currentRow, true);
    }
    connect(m_trendSeries, &QAbstractBarSeries::hovered, this,
            [this](const bool visible, const int index, QBarSet *) {
        if (visible && index >= 0 && index < m_trendTooltips.size()) {
            QToolTip::showText(QCursor::pos(), m_trendTooltips.at(index), m_trendView);
        } else {
            QToolTip::hideText();
        }
    });

    const QString emptyText = m_statistics.emptyStateText();
    m_trendEmpty->setText(emptyText);
    m_trendStack->setCurrentWidget(emptyText.isEmpty()
                                       ? static_cast<QWidget *>(m_trendView)
                                       : static_cast<QWidget *>(m_trendEmpty));
    m_trendSummary->setText(m_statistics.periodSummary());
    const QString accessible = QStringLiteral("%1。%2")
        .arg(m_statistics.periodSummary(),
             joinedAccessibleRows(*model, TrendContract::AccessibleTextRole));
    m_trendView->setAccessibleName(tr("完成趋势柱状图"));
    m_trendView->setAccessibleDescription(accessible);
    m_trendSummary->setAccessibleName(tr("完成趋势文字摘要"));
    m_trendSummary->setAccessibleDescription(accessible);
}

void StatisticsPage::refreshCategories()
{
    clearChart(*m_categoryChart);
    QAbstractItemModel *model = m_statistics.categories();
    QStringList labels;
    QList<int> values;
    QList<CategoryContract::Color> colors;
    QStringList tooltips;
    int maximum = 0;
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        labels.append(index.data(CategoryContract::LabelRole).toString());
        const int value = index.data(CategoryContract::ValueRole).toInt();
        values.append(value);
        maximum = std::max(maximum, value);
        colors.append(static_cast<CategoryContract::Color>(
            index.data(CategoryContract::ColorRole).toInt()));
        tooltips.append(index.data(CategoryContract::TooltipRole).toString());
    }

    auto *valueAxis = new QValueAxis;
    valueAxis->setRange(0, std::max(1, maximum));
    valueAxis->setLabelFormat(QStringLiteral("%d"));
    valueAxis->setTickInterval(1.0);
    auto *categoryAxis = new QBarCategoryAxis;
    categoryAxis->append(labels);
    m_categoryChart->addAxis(valueAxis, Qt::AlignBottom);
    m_categoryChart->addAxis(categoryAxis, Qt::AlignLeft);
    const WidgetTheme theme = WidgetTheme::fromPalette(palette());
    for (int target = 0; target < values.size(); ++target) {
        auto *series = new QHorizontalBarSeries;
        auto *set = new QBarSet(labels.at(target));
        QList<qreal> rowValues(values.size(), 0.0);
        rowValues[target] = values.at(target);
        set->append(rowValues);
        set->setColor(theme.statisticsCategoryColor(colors.at(target)));
        set->setBorderColor(Qt::transparent);
        series->append(set);
        series->setBarWidth(0.72);
        m_categoryChart->addSeries(series);
        series->attachAxis(valueAxis);
        series->attachAxis(categoryAxis);
        const QString tooltip = tooltips.at(target);
        connect(series, &QAbstractBarSeries::hovered, this,
                [this, tooltip](const bool visible, int, QBarSet *) {
            if (visible) {
                QToolTip::showText(QCursor::pos(), tooltip, m_categoryView);
            } else {
                QToolTip::hideText();
            }
        });
    }

    const QString emptyText = m_statistics.emptyStateText();
    m_categoryEmpty->setText(emptyText);
    m_categoryStack->setCurrentWidget(emptyText.isEmpty() && model->rowCount() > 0
                                          ? static_cast<QWidget *>(m_categoryView)
                                          : static_cast<QWidget *>(m_categoryEmpty));
    if (model->rowCount() == 0 && emptyText.isEmpty()) {
        m_categoryEmpty->setText(tr("该周期暂无类别完成记录"));
    }
    const QString summary = joinedAccessibleRows(
        *model, CategoryContract::AccessibleTextRole);
    m_categorySummary->setText(summary);
    m_categoryView->setAccessibleName(tr("类别构成横向柱状图"));
    m_categoryView->setAccessibleDescription(summary);
    m_categorySummary->setAccessibleName(tr("类别构成文字摘要"));
    m_categorySummary->setAccessibleDescription(summary);
}

void StatisticsPage::refreshHealth()
{
    clearLayout(*m_healthRows);
    QAbstractItemModel *model = m_statistics.health();
    const WidgetTheme theme = WidgetTheme::fromPalette(palette());
    int executableCount = 0;
    int blockedCount = 0;
    int dueSoonCount = 0;
    int overdueCount = 0;
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        const int type = index.data(HealthContract::TypeRole).toInt();
        const QString label = index.data(HealthContract::LabelRole).toString();
        const int value = index.data(HealthContract::ValueRole).toInt();
        const int maximum = std::max(1, index.data(HealthContract::MaximumRole).toInt());
        const QString description = index.data(HealthContract::DescriptionRole).toString();
        const QString accessible = index.data(HealthContract::AccessibleTextRole).toString();
        const auto semantic = tone(index.data(HealthContract::SemanticRole));

        switch (static_cast<HealthContract::Type>(type)) {
        case HealthContract::Executable:
            executableCount = value;
            break;
        case HealthContract::Blocked:
            blockedCount = value;
            break;
        case HealthContract::DueSoon:
            dueSoonCount = value;
            break;
        case HealthContract::Overdue:
            overdueCount = value;
            break;
        }

        auto *container = new QWidget(m_healthCard);
        container->setObjectName(QStringLiteral("healthRow_%1").arg(type));
        container->setAccessibleName(label);
        container->setAccessibleDescription(accessible);
        auto *layout = new QGridLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setHorizontalSpacing(10);
        layout->setVerticalSpacing(3);
        auto *name = new QLabel(label, container);
        name->setObjectName(QStringLiteral("healthLabel_%1").arg(type));
        auto *count = new QLabel(QStringLiteral("%1 项").arg(value), container);
        count->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        count->setStyleSheet(toneStyle(toneColor(theme, semantic), true));
        auto *progress = new QProgressBar(container);
        progress->setObjectName(QStringLiteral("healthProgress_%1").arg(type));
        progress->setRange(0, maximum);
        progress->setValue(value);
        progress->setTextVisible(false);
        progress->setAccessibleName(label);
        progress->setAccessibleDescription(accessible);
        progress->setStyleSheet(QStringLiteral(
            "QProgressBar { background: %1; border: none; border-radius: 4px; min-height: 8px; max-height: 8px; }"
            "QProgressBar::chunk { background: %2; border-radius: 4px; }")
            .arg(theme.surfaceStrong.name(), toneColor(theme, semantic).name()));
        auto *detail = new QLabel(description, container);
        detail->setObjectName(QStringLiteral("healthDescription_%1").arg(type));
        detail->setWordWrap(true);
        detail->setStyleSheet(toneStyle(theme.textSecondary));
        layout->addWidget(name, 0, 0);
        layout->addWidget(count, 0, 1);
        layout->addWidget(progress, 1, 0, 1, 2);
        layout->addWidget(detail, 2, 0, 1, 2);
        m_healthRows->addWidget(container);
    }
    const QString summary = tr(
        "当前有 %1 项活动任务。执行状态：%2 项可执行，%3 项被阻塞。"
        "风险提醒：%4 项即将到期，%5 项已经逾期；风险项可能与执行状态重叠。")
        .arg(m_statistics.activeTaskCount())
        .arg(executableCount)
        .arg(blockedCount)
        .arg(dueSoonCount)
        .arg(overdueCount);
    m_healthSummary->setText(summary);
    m_healthSummary->setAccessibleName(tr("任务健康文字摘要"));
    m_healthSummary->setAccessibleDescription(summary);
    m_healthCard->setAccessibleName(tr("任务健康"));
    m_healthCard->setAccessibleDescription(summary);
}

void StatisticsPage::syncRangeButtons()
{
    const QSignalBlocker first(m_last7Days);
    const QSignalBlocker second(m_last30Days);
    const QSignalBlocker third(m_last12Weeks);
    m_last7Days->setChecked(m_statistics.range() == StatisticsContract::Last7Days);
    m_last30Days->setChecked(m_statistics.range() == StatisticsContract::Last30Days);
    m_last12Weeks->setChecked(m_statistics.range() == StatisticsContract::Last12Weeks);
}

void StatisticsPage::applyTheme()
{
    const WidgetTheme theme = WidgetTheme::fromPalette(palette());
    const QFont chartFont = font();
    const QList<QChart *> charts{m_trendChart, m_categoryChart, m_onTimeChart};
    for (QChart *chart : charts) {
        chart->setTitleBrush(theme.textPrimary);
        chart->setTitleFont(chartFont);
        styleAxes(*chart, theme, chartFont);
    }
    m_trendView->setBackgroundBrush(Qt::transparent);
    m_categoryView->setBackgroundBrush(Qt::transparent);
    m_onTimeView->setBackgroundBrush(Qt::transparent);
    if (m_trendSeries != nullptr && !m_trendSeries->barSets().isEmpty()) {
        QBarSet *set = m_trendSeries->barSets().constFirst();
        set->setColor(theme.primarySoft);
        set->setBorderColor(theme.primary);
        set->setSelectedColor(theme.primary);
    }
    const QList<QPieSlice *> slices = m_onTimeSeries->slices();
    if (!slices.isEmpty()) {
        slices.at(0)->setColor(m_statistics.hasOnTimeRate()
                                   ? theme.done : theme.surfaceStrong);
        slices.at(0)->setBorderColor(Qt::transparent);
    }
    if (slices.size() > 1) {
        slices.at(1)->setColor(theme.surfaceStrong);
        slices.at(1)->setBorderColor(Qt::transparent);
    }
}

void StatisticsPage::applyResponsiveLayout()
{
    const QWidget *topLevel = window();
    const bool wide = topLevel != nullptr && topLevel->width() >= 1050;
    if (wide == m_wideLayout && m_overviewLayout->count() > 0) {
        return;
    }
    m_wideLayout = wide;
    detachLayoutItems(*m_overviewLayout);
    detachLayoutItems(*m_sectionsLayout);
    if (wide) {
        m_overviewLayout->addWidget(m_todayCard, 0, 0);
        m_overviewLayout->addWidget(m_weekCard, 0, 1);
        m_overviewLayout->addWidget(m_onTimeCard, 0, 2);
        m_overviewLayout->addWidget(m_overdueCard, 0, 3);
        for (int column = 0; column < 4; ++column) {
            m_overviewLayout->setColumnStretch(column, 1);
        }
        m_sectionsLayout->addWidget(m_trendCard, 0, 0, 1, 2);
        m_sectionsLayout->addWidget(m_healthCard, 1, 0);
        m_sectionsLayout->addWidget(m_categoryCard, 1, 1);
        m_sectionsLayout->setColumnStretch(0, 1);
        m_sectionsLayout->setColumnStretch(1, 1);
    } else {
        m_overviewLayout->addWidget(m_todayCard, 0, 0);
        m_overviewLayout->addWidget(m_weekCard, 0, 1);
        m_overviewLayout->addWidget(m_onTimeCard, 1, 0);
        m_overviewLayout->addWidget(m_overdueCard, 1, 1);
        m_overviewLayout->setColumnStretch(0, 1);
        m_overviewLayout->setColumnStretch(1, 1);
        m_sectionsLayout->addWidget(m_trendCard, 0, 0);
        m_sectionsLayout->addWidget(m_healthCard, 1, 0);
        m_sectionsLayout->addWidget(m_categoryCard, 2, 0);
        m_sectionsLayout->setColumnStretch(0, 1);
    }
}

} // namespace smartmate::view::widgets
