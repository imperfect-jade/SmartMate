#include "FocusPage.h"

#include "view/widgets/theme/WidgetTheme.h"

#include <QAbstractItemModel>
#include <QBoxLayout>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QVBoxLayout>

namespace smartmate::view::widgets {
namespace {

using FocusContract = viewmodel::FocusContract;
using HistoryContract = viewmodel::FocusHistoryContract;

void clearLayout(QLayout &layout)
{
    while (QLayoutItem *item = layout.takeAt(0)) {
        if (QWidget *widget = item->widget()) delete widget;
        delete item;
    }
}

QLabel *metadataLabel(const QString &objectName, QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setObjectName(objectName);
    label->setWordWrap(true);
    return label;
}

} // namespace

FocusPage::FocusPage(viewmodel::FocusContract &focus, QWidget *parent)
    : QScrollArea(parent)
    , m_focus(focus)
    , m_content(new QWidget)
    , m_sessionCard(new QFrame(m_content))
    , m_state(new QLabel(m_sessionCard))
    , m_taskTitle(new QLabel(m_sessionCard))
    , m_elapsed(new QLabel(m_sessionCard))
    , m_category(metadataLabel(QStringLiteral("focusCategoryText"), m_sessionCard))
    , m_estimate(metadataLabel(QStringLiteral("focusEstimateText"), m_sessionCard))
    , m_startedAt(metadataLabel(QStringLiteral("focusStartedAtText"), m_sessionCard))
    , m_emptyState(new QLabel(m_sessionCard))
    , m_storageWarning(new QLabel(m_sessionCard))
    , m_historyCount(new QLabel(m_content))
    , m_historyEmpty(new QLabel(m_content))
    , m_showTasks(new QPushButton(tr("前往任务"), m_sessionCard))
    , m_start(new QPushButton(tr("开始专注"), m_sessionCard))
    , m_pause(new QPushButton(tr("暂停"), m_sessionCard))
    , m_resume(new QPushButton(tr("继续"), m_sessionCard))
    , m_complete(new QPushButton(tr("完成专注"), m_sessionCard))
    , m_abandon(new QPushButton(tr("放弃专注"), m_sessionCard))
    , m_metadataLayout(new QGridLayout)
    , m_actionLayout(new QBoxLayout(QBoxLayout::LeftToRight))
    , m_historyRows(new QVBoxLayout)
{
    setObjectName(QStringLiteral("focusPage"));
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    viewport()->setObjectName(QStringLiteral("focusViewport"));
    viewport()->setBackgroundRole(QPalette::Window);
    viewport()->setAutoFillBackground(true);
    m_content->setObjectName(QStringLiteral("focusContent"));
    m_content->setBackgroundRole(QPalette::Window);
    m_content->setAutoFillBackground(true);
    setWidget(m_content);

    auto *root = new QVBoxLayout(m_content);
    root->setContentsMargins(28, 24, 28, 32);
    root->setSpacing(18);
    root->setSizeConstraint(QLayout::SetMinimumSize);

    auto *header = new QHBoxLayout;
    auto *headerText = new QVBoxLayout;
    headerText->setSpacing(2);
    auto *title = new QLabel(tr("专注"), m_content);
    title->setObjectName(QStringLiteral("pageTitle"));
    auto *subtitle = new QLabel(tr("围绕当前进行中的任务自由计时，按自己的节奏暂停和继续"),
                                m_content);
    subtitle->setObjectName(QStringLiteral("secondaryText"));
    subtitle->setWordWrap(true);
    auto *refresh = new QPushButton(tr("刷新"), m_content);
    refresh->setObjectName(QStringLiteral("refreshFocusButton"));
    refresh->setAccessibleName(tr("刷新专注数据"));
    headerText->addWidget(title);
    headerText->addWidget(subtitle);
    header->addLayout(headerText, 1);
    header->addWidget(refresh, 0, Qt::AlignTop);
    root->addLayout(header);

    m_sessionCard->setObjectName(QStringLiteral("focusSessionCard"));
    m_sessionCard->setFrameShape(QFrame::StyledPanel);
    m_sessionCard->setAccessibleName(tr("当前专注"));
    auto *sessionLayout = new QVBoxLayout(m_sessionCard);
    sessionLayout->setContentsMargins(24, 22, 24, 22);
    sessionLayout->setSpacing(12);

    m_state->setObjectName(QStringLiteral("focusSessionState"));
    m_state->setAccessibleName(tr("专注状态"));
    m_taskTitle->setObjectName(QStringLiteral("focusSessionTaskTitle"));
    m_taskTitle->setWordWrap(true);
    m_taskTitle->setAccessibleName(tr("当前任务"));
    m_elapsed->setObjectName(QStringLiteral("focusElapsedText"));
    m_elapsed->setAlignment(Qt::AlignCenter);
    m_elapsed->setAccessibleName(tr("累计专注时长"));
    m_emptyState->setObjectName(QStringLiteral("focusEmptyState"));
    m_emptyState->setWordWrap(true);
    m_emptyState->setAlignment(Qt::AlignCenter);
    m_emptyState->setAccessibleName(tr("专注空状态"));
    m_storageWarning->setObjectName(QStringLiteral("focusStorageWarning"));
    m_storageWarning->setWordWrap(true);
    m_storageWarning->setAccessibleName(tr("专注保存状态"));

    m_metadataLayout->setContentsMargins(0, 0, 0, 0);
    m_metadataLayout->setHorizontalSpacing(18);
    m_metadataLayout->setVerticalSpacing(8);
    m_category->setAccessibleName(tr("任务类别"));
    m_estimate->setAccessibleName(tr("预计用时"));
    m_startedAt->setAccessibleName(tr("开始时间"));

    for (QPushButton *button : {m_showTasks, m_start, m_pause, m_resume,
                                m_complete, m_abandon}) {
        button->setMinimumHeight(38);
        m_actionLayout->addWidget(button);
    }
    m_actionLayout->addStretch();
    m_showTasks->setObjectName(QStringLiteral("showTasksFromFocusButton"));
    m_start->setObjectName(QStringLiteral("startFocusButton"));
    m_pause->setObjectName(QStringLiteral("pauseFocusButton"));
    m_resume->setObjectName(QStringLiteral("resumeFocusButton"));
    m_complete->setObjectName(QStringLiteral("completeFocusButton"));
    m_abandon->setObjectName(QStringLiteral("abandonFocusButton"));
    m_showTasks->setAccessibleName(tr("前往任务页面"));
    m_start->setAccessibleName(tr("开始当前任务专注"));
    m_pause->setAccessibleName(tr("暂停当前专注"));
    m_resume->setAccessibleName(tr("继续当前专注"));
    m_complete->setAccessibleName(tr("完成当前专注"));
    m_abandon->setAccessibleName(tr("放弃当前专注"));

    sessionLayout->addWidget(m_state);
    sessionLayout->addWidget(m_taskTitle);
    sessionLayout->addWidget(m_emptyState);
    sessionLayout->addWidget(m_elapsed);
    sessionLayout->addLayout(m_metadataLayout);
    sessionLayout->addWidget(m_storageWarning);
    sessionLayout->addLayout(m_actionLayout);
    root->addWidget(m_sessionCard);

    auto *historyHeader = new QHBoxLayout;
    auto *historyTitle = new QLabel(tr("最近专注"), m_content);
    historyTitle->setObjectName(QStringLiteral("sectionTitle"));
    m_historyCount->setObjectName(QStringLiteral("focusHistoryCount"));
    m_historyCount->setAccessibleName(tr("专注历史数量"));
    historyHeader->addWidget(historyTitle);
    historyHeader->addStretch();
    historyHeader->addWidget(m_historyCount);
    root->addLayout(historyHeader);

    m_historyEmpty->setObjectName(QStringLiteral("focusHistoryEmptyState"));
    m_historyEmpty->setAlignment(Qt::AlignCenter);
    m_historyEmpty->setWordWrap(true);
    m_historyEmpty->setAccessibleName(tr("专注历史空状态"));
    root->addWidget(m_historyEmpty);
    m_historyRows->setContentsMargins(0, 0, 0, 0);
    m_historyRows->setSpacing(10);
    root->addLayout(m_historyRows);
    root->addStretch();

    connect(refresh, &QPushButton::clicked, &m_focus, &FocusContract::reload);
    connect(m_showTasks, &QPushButton::clicked,
            this, &FocusPage::showTasksRequested);
    connect(m_start, &QPushButton::clicked, this, [this] {
        if (!m_focus.startFocus(m_focus.taskId())) syncCurrent();
    });
    connect(m_pause, &QPushButton::clicked, this, [this] {
        if (!m_focus.pauseFocus(m_focus.sessionId())) syncCurrent();
    });
    connect(m_resume, &QPushButton::clicked, this, [this] {
        if (!m_focus.resumeFocus(m_focus.sessionId())) syncCurrent();
    });
    connect(m_complete, &QPushButton::clicked, this, [this] {
        if (!m_focus.completeFocus(m_focus.sessionId())) syncCurrent();
    });
    connect(m_abandon, &QPushButton::clicked, this, [this] {
        const auto answer = QMessageBox::question(
            this, tr("放弃专注"),
            tr("放弃后，本次专注记录不会保留。确定要放弃吗？"),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer == QMessageBox::Yes
            && !m_focus.abandonFocus(m_focus.sessionId())) {
            syncCurrent();
        }
    });

    connect(&m_focus, &FocusContract::focusChanged,
            this, &FocusPage::syncCurrent);
    connect(&m_focus, &FocusContract::historyChanged,
            this, &FocusPage::rebuildHistory);
    connect(&m_focus, &FocusContract::storageWarningChanged,
            this, &FocusPage::syncCurrent);
    connectHistoryModel(m_focus.history());

    syncAll();
    applyResponsiveLayout();
}

void FocusPage::resizeEvent(QResizeEvent *event)
{
    QScrollArea::resizeEvent(event);
    applyResponsiveLayout();
}

void FocusPage::showEvent(QShowEvent *event)
{
    QScrollArea::showEvent(event);
    m_focus.reload();
}

void FocusPage::changeEvent(QEvent *event)
{
    QScrollArea::changeEvent(event);
    if (event->type() == QEvent::PaletteChange
        || event->type() == QEvent::FontChange) {
        syncCurrent();
        rebuildHistory();
    }
}

void FocusPage::syncAll()
{
    syncCurrent();
    rebuildHistory();
}

void FocusPage::syncCurrent()
{
    const auto state = m_focus.pageState();
    const bool hasTask = state != FocusContract::NoInProgressTask;
    const bool hasSession = state == FocusContract::Running
        || state == FocusContract::Paused;

    m_state->setText(m_focus.stateText());
    m_taskTitle->setText(m_focus.taskTitle());
    m_elapsed->setText(m_focus.elapsedText());
    m_category->setText(tr("类别：%1").arg(m_focus.categoryName()));
    m_category->setStyleSheet(
        QStringLiteral("color: %1; font-weight: 600;")
            .arg(WidgetTheme::fromPalette(palette())
                     .focusCategoryColor(m_focus.categoryColor()).name()));
    m_estimate->setText(tr("预计：%1").arg(m_focus.estimatedText()));
    m_startedAt->setText(tr("开始：%1").arg(m_focus.startedAtText()));
    m_emptyState->setText(m_focus.emptyStateText());
    m_storageWarning->setText(m_focus.storageWarningText());

    m_taskTitle->setVisible(hasTask);
    m_emptyState->setVisible(!hasTask);
    m_elapsed->setVisible(hasTask);
    m_category->setVisible(hasTask);
    m_estimate->setVisible(hasTask);
    m_startedAt->setVisible(hasSession);
    m_storageWarning->setVisible(m_focus.hasStorageWarning());
    m_showTasks->setVisible(!hasTask);
    m_start->setVisible(state == FocusContract::ReadyToStart);
    m_pause->setVisible(state == FocusContract::Running);
    m_resume->setVisible(state == FocusContract::Paused);
    m_complete->setVisible(hasSession);
    m_abandon->setVisible(hasSession);

    m_start->setEnabled(m_focus.canStart());
    m_pause->setEnabled(m_focus.canPause());
    m_resume->setEnabled(m_focus.canResume());
    m_complete->setEnabled(m_focus.canComplete());
    m_abandon->setEnabled(m_focus.canAbandon());

    m_elapsed->setAccessibleDescription(
        tr("当前累计专注时长为 %1").arg(m_focus.elapsedText()));
    m_sessionCard->setAccessibleDescription(
        hasTask
            ? tr("%1，任务 %2，累计专注 %3")
                  .arg(m_focus.stateText(), m_focus.taskTitle(), m_focus.elapsedText())
            : m_focus.emptyStateText());
}

void FocusPage::rebuildHistory()
{
    clearLayout(*m_historyRows);
    QAbstractItemModel *model = m_focus.history();
    const int count = model != nullptr ? model->rowCount() : 0;
    m_historyCount->setText(tr("%1 条").arg(m_focus.historyCount()));
    m_historyEmpty->setText(m_focus.historyEmptyStateText());
    m_historyEmpty->setVisible(!m_focus.hasHistory());

    if (model == nullptr) return;
    const WidgetTheme theme = WidgetTheme::fromPalette(palette());
    for (int row = 0; row < count; ++row) {
        const QModelIndex index = model->index(row, 0);
        auto *record = new QFrame(m_content);
        record->setObjectName(QStringLiteral("focusHistoryRow"));
        record->setFrameShape(QFrame::StyledPanel);
        record->setToolTip(index.data(HistoryContract::TooltipRole).toString());
        record->setAccessibleName(tr("专注记录 %1").arg(row + 1));
        record->setAccessibleDescription(
            index.data(HistoryContract::AccessibleTextRole).toString());

        auto *layout = new QHBoxLayout(record);
        layout->setContentsMargins(16, 13, 16, 13);
        layout->setSpacing(12);
        auto *marker = new QFrame(record);
        marker->setObjectName(QStringLiteral("focusHistoryCategoryMarker"));
        marker->setFixedSize(8, 34);
        const auto categoryColor = static_cast<FocusContract::CategoryColor>(
            index.data(HistoryContract::CategoryColorRole).toInt());
        marker->setStyleSheet(QStringLiteral("background: %1; border: none; border-radius: 4px;")
                                  .arg(theme.focusCategoryColor(categoryColor).name()));
        auto *texts = new QVBoxLayout;
        texts->setSpacing(3);
        auto *task = new QLabel(index.data(HistoryContract::TaskTitleRole).toString(), record);
        task->setObjectName(QStringLiteral("focusHistoryTaskTitle"));
        task->setWordWrap(true);
        auto *meta = new QLabel(
            tr("%1 · %2")
                .arg(index.data(HistoryContract::CompletedAtTextRole).toString(),
                     index.data(HistoryContract::CategoryNameRole).toString()),
            record);
        meta->setObjectName(QStringLiteral("focusHistoryMetadata"));
        texts->addWidget(task);
        texts->addWidget(meta);
        auto *duration = new QLabel(
            index.data(HistoryContract::DurationTextRole).toString(), record);
        duration->setObjectName(QStringLiteral("focusHistoryDuration"));
        duration->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        duration->setAccessibleName(tr("专注时长"));
        duration->setAccessibleDescription(duration->text());
        layout->addWidget(marker);
        layout->addLayout(texts, 1);
        layout->addWidget(duration);
        m_historyRows->addWidget(record);
    }
}

void FocusPage::applyResponsiveLayout()
{
    // 产品阈值以顶层窗口为准；侧栏不能让 1180px 正常窗口被误判为窄布局。
    const QWidget *topLevel = window();
    const int responsiveWidth = topLevel != nullptr
        ? topLevel->width() : width();
    const bool wide = responsiveWidth >= 1050;
    if (m_wideLayout == wide && m_metadataLayout->count() != 0) return;
    m_wideLayout = wide;
    m_metadataLayout->removeWidget(m_category);
    m_metadataLayout->removeWidget(m_estimate);
    m_metadataLayout->removeWidget(m_startedAt);
    if (wide) {
        m_metadataLayout->addWidget(m_category, 0, 0);
        m_metadataLayout->addWidget(m_estimate, 0, 1);
        m_metadataLayout->addWidget(m_startedAt, 0, 2);
        m_actionLayout->setDirection(QBoxLayout::LeftToRight);
    } else {
        m_metadataLayout->addWidget(m_category, 0, 0);
        m_metadataLayout->addWidget(m_estimate, 1, 0);
        m_metadataLayout->addWidget(m_startedAt, 2, 0);
        m_actionLayout->setDirection(QBoxLayout::TopToBottom);
    }
}

void FocusPage::connectHistoryModel(QAbstractItemModel *model)
{
    if (model == nullptr) return;
    connect(model, &QAbstractItemModel::modelReset,
            this, &FocusPage::rebuildHistory);
    connect(model, &QAbstractItemModel::dataChanged,
            this, [this] { rebuildHistory(); });
    connect(model, &QAbstractItemModel::rowsInserted,
            this, [this] { rebuildHistory(); });
    connect(model, &QAbstractItemModel::rowsRemoved,
            this, [this] { rebuildHistory(); });
    connect(model, &QAbstractItemModel::rowsMoved,
            this, [this] { rebuildHistory(); });
}

} // namespace smartmate::view::widgets
