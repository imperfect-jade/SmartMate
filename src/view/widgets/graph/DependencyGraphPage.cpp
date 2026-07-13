#include "DependencyGraphPage.h"

#include "DependencyGraphView.h"
#include "view/widgets/task/TaskDependencyDialog.h"
#include "view/widgets/task/TaskDetailsDialog.h"
#include "view/widgets/theme/WidgetTheme.h"
#include "common/presentation/UiNotification.h"
#include "viewmodel/contracts/AppearanceSettingsContract.h"
#include "viewmodel/contracts/TaskDependencyContract.h"
#include "viewmodel/contracts/TaskDetailsContract.h"
#include "viewmodel/contracts/TaskGraphContract.h"

#include <QAbstractItemModel>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStackedLayout>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace smartmate::view::widgets {
namespace {

using Graph = viewmodel::TaskGraphContract;

class RelationDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    { return {240, 48}; }
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        painter->save();
        const QRect card = option.rect.adjusted(1, 2, -1, -2);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(option.palette.mid().color());
        painter->setBrush(option.state.testFlag(QStyle::State_MouseOver)
                              ? option.palette.alternateBase()
                              : option.palette.base());
        painter->drawRoundedRect(card, 6, 6);
        QFont titleFont = option.font;
        titleFont.setBold(true);
        painter->setFont(titleFont);
        painter->setPen(option.palette.text().color());
        painter->drawText(card.adjusted(9, 3, -9, -22),
                          Qt::AlignVCenter | Qt::AlignLeft,
                          index.data(Graph::RelationTitleRole).toString());
        painter->setFont(option.font);
        painter->setPen(option.palette.placeholderText().color());
        painter->drawText(card.adjusted(9, 22, -9, -3),
                          Qt::AlignVCenter | Qt::AlignLeft,
                          QStringLiteral("%1 · %2").arg(
                              index.data(Graph::RelationStatusTextRole).toString(),
                              index.data(Graph::RelationTextRole).toString()));
        painter->restore();
    }
};

void setFrameColor(QFrame &frame, const QColor &background, const QColor &border)
{
    Q_ASSERT(!frame.objectName().isEmpty());
    frame.setStyleSheet(QStringLiteral(
        "QFrame#%1 { background: %2; border: 1px solid %3; border-radius: 12px; }")
        .arg(frame.objectName(), background.name(), border.name()));
}

void setLabelColor(QLabel &label, const QColor &color)
{
    QPalette palette = label.palette();
    palette.setColor(QPalette::WindowText, color);
    label.setPalette(palette);
}

} // namespace

DependencyGraphPage::DependencyGraphPage(
    DependencyGraphPageDependencies dependencies, QWidget *parent)
    : QWidget(parent), m_dependencies(dependencies)
    , m_search(new QLineEdit(this)), m_statusFilter(new QComboBox(this))
    , m_categoryFilter(new QComboBox(this)), m_taskCount(new QLabel(this))
    , m_blockedCount(new QLabel(this)), m_locateCurrent(new QPushButton(tr("定位当前"), this))
    , m_zoomOut(new QToolButton(this)), m_zoomIn(new QToolButton(this))
    , m_zoomLabel(new QLabel(this)), m_notification(new QLabel(this))
    , m_canvasFrame(new QFrame(this)), m_view(new DependencyGraphView(dependencies.taskGraph, m_canvasFrame))
    , m_empty(new QLabel(m_canvasFrame)), m_canvasStack(new QStackedLayout)
    , m_openDetails(new QPushButton(tr("任务详情 ‹"), m_canvasFrame))
    , m_detailsPanel(new QFrame(this)), m_pinDetails(new QToolButton(m_detailsPanel))
    , m_selectedTitle(new QLabel(m_detailsPanel)), m_selectedCategory(new QLabel(m_detailsPanel))
    , m_selectedContext(new QLabel(m_detailsPanel)), m_selectedMeta(new QLabel(m_detailsPanel))
    , m_selectedDescription(new QLabel(m_detailsPanel)), m_detailsDivider(new QFrame(m_detailsPanel))
    , m_selectedDeadline(new QLabel(m_detailsPanel)), m_selectedDuration(new QLabel(m_detailsPanel))
    , m_selectedRelations(new QLabel(m_detailsPanel)), m_selectedBlocking(new QLabel(m_detailsPanel))
    , m_predecessorHeading(new QLabel(tr("直接前置"), m_detailsPanel))
    , m_successorHeading(new QLabel(tr("直接后继"), m_detailsPanel))
    , m_predecessors(new QListView(m_detailsPanel)), m_successors(new QListView(m_detailsPanel))
    , m_editDependencies(new QPushButton(tr("编辑前置任务"), m_detailsPanel))
    , m_fullDetails(new TaskDetailsDialog(dependencies.taskDetails, this))
    , m_dependencyEditor(new TaskDependencyDialog(dependencies.taskDependencies, this))
{
    setObjectName(QStringLiteral("dependencyGraphPage"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(12);

    auto *toolbar = new QFrame(this);
    toolbar->setObjectName(QStringLiteral("dependencyGraphToolbar"));
    auto *toolbarLayout = new QGridLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 12, 12, 12);
    toolbarLayout->setHorizontalSpacing(8);
    auto *title = new QLabel(tr("依赖图"), toolbar);
    title->setObjectName(QStringLiteral("pageTitle"));
    m_taskCount->setObjectName(QStringLiteral("graphTaskCountLabel"));
    m_blockedCount->setObjectName(QStringLiteral("graphBlockedCountLabel"));
    m_search->setObjectName(QStringLiteral("graphSearchField"));
    m_search->setPlaceholderText(tr("搜索并定位任务"));
    m_search->setMinimumWidth(150);
    m_statusFilter->setObjectName(QStringLiteral("graphStatusFilter"));
    m_statusFilter->addItems({tr("全部状态"), tr("待办"), tr("进行中"),
                              tr("阻塞"), tr("已完成")});
    m_categoryFilter->setObjectName(QStringLiteral("graphCategoryFilter"));
    m_locateCurrent->setObjectName(QStringLiteral("locateCurrentGraphTaskButton"));
    m_zoomOut->setObjectName(QStringLiteral("zoomOutGraphButton"));
    m_zoomOut->setText(QStringLiteral("−"));
    m_zoomIn->setObjectName(QStringLiteral("zoomInGraphButton"));
    m_zoomIn->setText(QStringLiteral("+"));
    m_zoomLabel->setObjectName(QStringLiteral("graphZoomLabel"));
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    auto *resetZoom = new QPushButton(tr("100%"), toolbar);
    resetZoom->setObjectName(QStringLiteral("resetGraphZoomButton"));
    auto *fit = new QPushButton(tr("适应画布"), toolbar);
    fit->setObjectName(QStringLiteral("fitGraphButton"));
    auto *reload = new QToolButton(toolbar);
    reload->setObjectName(QStringLiteral("reloadGraphButton"));
    reload->setText(QStringLiteral("↻"));
    toolbarLayout->addWidget(title, 0, 0);
    toolbarLayout->addWidget(m_taskCount, 0, 1);
    toolbarLayout->addWidget(m_blockedCount, 0, 2);
    toolbarLayout->addWidget(m_search, 0, 3, 1, 2);
    toolbarLayout->addWidget(m_statusFilter, 0, 5);
    toolbarLayout->addWidget(m_categoryFilter, 0, 6);
    toolbarLayout->setColumnStretch(3, 1);
    toolbarLayout->addWidget(m_locateCurrent, 1, 2);
    toolbarLayout->addWidget(m_zoomOut, 1, 3);
    toolbarLayout->addWidget(m_zoomLabel, 1, 4);
    toolbarLayout->addWidget(m_zoomIn, 1, 5);
    toolbarLayout->addWidget(resetZoom, 1, 6);
    toolbarLayout->addWidget(fit, 1, 7);
    toolbarLayout->addWidget(reload, 1, 8);
    root->addWidget(toolbar);

    m_notification->setObjectName(QStringLiteral("graphNotificationLabel"));
    m_notification->setWordWrap(true);
    m_notification->hide();
    root->addWidget(m_notification);

    auto *content = new QHBoxLayout;
    content->setSpacing(12);
    m_canvasFrame->setObjectName(QStringLiteral("dependencyGraphCanvasFrame"));
    m_canvasFrame->setMinimumWidth(500);
    auto *canvasLayout = new QVBoxLayout(m_canvasFrame);
    canvasLayout->setContentsMargins(1, 1, 1, 1);
    auto *canvasHeader = new QHBoxLayout;
    canvasHeader->addStretch();
    m_openDetails->setObjectName(QStringLiteral("openGraphDetailsButton"));
    canvasHeader->addWidget(m_openDetails);
    canvasLayout->addLayout(canvasHeader);
    m_view->setObjectName(QStringLiteral("dependencyGraphViewport"));
    m_empty->setObjectName(QStringLiteral("dependencyGraphEmptyState"));
    m_empty->setAlignment(Qt::AlignCenter);
    m_canvasStack->addWidget(m_view);
    m_canvasStack->addWidget(m_empty);
    canvasLayout->addLayout(m_canvasStack, 1);
    content->addWidget(m_canvasFrame, 1);

    m_detailsPanel->setObjectName(QStringLiteral("dependencyGraphDetails"));
    auto *detailsLayout = new QVBoxLayout(m_detailsPanel);
    detailsLayout->setContentsMargins(16, 16, 16, 16);
    detailsLayout->setSpacing(8);
    auto *detailsHeader = new QHBoxLayout;
    auto *detailsTitle = new QLabel(tr("任务详情"), m_detailsPanel);
    detailsTitle->setObjectName(QStringLiteral("sectionTitle"));
    m_pinDetails->setObjectName(QStringLiteral("pinGraphDetailsButton"));
    m_pinDetails->setCheckable(true);
    m_pinDetails->setText(QStringLiteral("○"));
    auto *collapse = new QToolButton(m_detailsPanel);
    collapse->setObjectName(QStringLiteral("collapseGraphDetailsButton"));
    collapse->setText(QStringLiteral("›"));
    detailsHeader->addWidget(detailsTitle, 1);
    detailsHeader->addWidget(m_pinDetails);
    detailsHeader->addWidget(collapse);
    detailsLayout->addLayout(detailsHeader);
    auto *scroll = new QScrollArea(m_detailsPanel);
    scroll->setObjectName(QStringLiteral("graphDetailsScrollView"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *detailsContent = new QWidget(scroll);
    detailsContent->setObjectName(QStringLiteral("graphDetailsContent"));
    auto *detailsBody = new QVBoxLayout(detailsContent);
    detailsBody->setContentsMargins(0, 0, 0, 0);
    detailsBody->setSpacing(10);
    m_selectedTitle->setObjectName(QStringLiteral("selectedGraphTaskTitle"));
    m_selectedTitle->setWordWrap(true);
    auto *categoryRow = new QHBoxLayout;
    categoryRow->setContentsMargins(0, 0, 0, 0);
    categoryRow->setSpacing(6);
    m_selectedCategory->setObjectName(QStringLiteral("selectedGraphTaskCategory"));
    m_selectedCategory->setWordWrap(true);
    m_selectedContext->setObjectName(QStringLiteral("selectedGraphTaskContext"));
    m_selectedContext->setText(tr("跨类别上下文"));
    categoryRow->addWidget(m_selectedCategory, 0, Qt::AlignLeft);
    categoryRow->addWidget(m_selectedContext, 0, Qt::AlignLeft);
    categoryRow->addStretch();
    m_selectedMeta->setObjectName(QStringLiteral("selectedGraphTaskMeta"));
    m_selectedMeta->setWordWrap(true);
    m_selectedDescription->setObjectName(QStringLiteral("selectedGraphTaskDescription"));
    m_selectedDescription->setWordWrap(true);
    m_detailsDivider->setObjectName(QStringLiteral("graphDetailsDivider"));
    m_detailsDivider->setFrameShape(QFrame::HLine);
    m_detailsDivider->setFixedHeight(1);
    m_selectedDeadline->setObjectName(QStringLiteral("selectedGraphTaskDeadline"));
    m_selectedDeadline->setWordWrap(true);
    m_selectedDuration->setObjectName(QStringLiteral("selectedGraphTaskDuration"));
    m_selectedDuration->setWordWrap(true);
    m_selectedRelations->setObjectName(QStringLiteral("selectedGraphTaskRelations"));
    m_selectedRelations->setWordWrap(true);
    m_selectedBlocking->setObjectName(QStringLiteral("selectedGraphTaskBlockingReason"));
    m_selectedBlocking->setWordWrap(true);
    detailsBody->addWidget(m_selectedTitle);
    detailsBody->addLayout(categoryRow);
    detailsBody->addWidget(m_selectedMeta);
    detailsBody->addWidget(m_selectedDescription);
    detailsBody->addWidget(m_detailsDivider);
    detailsBody->addWidget(m_selectedDeadline);
    detailsBody->addWidget(m_selectedDuration);
    detailsBody->addWidget(m_selectedRelations);
    detailsBody->addWidget(m_selectedBlocking);
    m_predecessors->setObjectName(QStringLiteral("graphPredecessorList"));
    m_successors->setObjectName(QStringLiteral("graphSuccessorList"));
    m_predecessors->setModel(dependencies.taskGraph.selectedPredecessors());
    m_successors->setModel(dependencies.taskGraph.selectedSuccessors());
    m_predecessors->setItemDelegate(new RelationDelegate(m_predecessors));
    m_successors->setItemDelegate(new RelationDelegate(m_successors));
    m_predecessors->setFrameShape(QFrame::NoFrame);
    m_successors->setFrameShape(QFrame::NoFrame);
    m_predecessors->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_successors->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_predecessors->setMouseTracking(true);
    m_successors->setMouseTracking(true);
    detailsBody->addWidget(m_predecessorHeading);
    detailsBody->addWidget(m_predecessors);
    detailsBody->addWidget(m_successorHeading);
    detailsBody->addWidget(m_successors);
    detailsBody->addStretch();
    scroll->setWidget(detailsContent);
    detailsLayout->addWidget(scroll, 1);
    auto *center = new QPushButton(tr("在画布中居中"), m_detailsPanel);
    center->setObjectName(QStringLiteral("centerSelectedGraphTaskButton"));
    auto *fullDetails = new QPushButton(tr("查看完整详情"), m_detailsPanel);
    fullDetails->setObjectName(QStringLiteral("openSelectedGraphTaskDetailsButton"));
    m_editDependencies->setObjectName(QStringLiteral("editSelectedGraphDependenciesButton"));
    detailsLayout->addWidget(center);
    detailsLayout->addWidget(fullDetails);
    detailsLayout->addWidget(m_editDependencies);
    content->addWidget(m_detailsPanel);
    root->addLayout(content, 1);

    m_fullDetails->setObjectName(QStringLiteral("graphTaskDetailsDialog"));
    m_fullDetails->setActionsVisible(false);
    m_dependencyEditor->setObjectName(QStringLiteral("graphTaskDependencyDialog"));

    connect(m_search, &QLineEdit::textEdited, &dependencies.taskGraph,
            &viewmodel::TaskGraphContract::setSearchText);
    connect(m_search, &QLineEdit::returnPressed, this, [this] {
        if (m_dependencies.taskGraph.locateFirstMatch()) {
            setDetailsExpanded(true);
            QTimer::singleShot(210, m_view, &DependencyGraphView::centerSelectedNode);
        }
    });
    connect(m_statusFilter, &QComboBox::activated, &dependencies.taskGraph,
            &viewmodel::TaskGraphContract::setStatusFilterIndex);
    connect(m_categoryFilter, &QComboBox::activated, this, [this](const int index) {
        m_dependencies.taskGraph.setCategoryFilter(
            m_categoryFilter->itemData(index, Qt::UserRole).toInt(),
            m_categoryFilter->itemData(index, Qt::UserRole + 1).toString());
    });
    connect(m_locateCurrent, &QPushButton::clicked, this, [this] {
        if (m_dependencies.taskGraph.selectCurrentTask()) selectAndCenter(
            m_dependencies.taskGraph.selectedTaskId());
    });
    connect(m_zoomOut, &QToolButton::clicked, this,
            [this] { m_view->setZoomFactor(m_view->zoomFactor() - 0.1); });
    connect(m_zoomIn, &QToolButton::clicked, this,
            [this] { m_view->setZoomFactor(m_view->zoomFactor() + 0.1); });
    connect(resetZoom, &QPushButton::clicked, this,
            [this] { m_view->setZoomFactor(1.0); });
    connect(fit, &QPushButton::clicked, m_view, &DependencyGraphView::fitContent);
    connect(reload, &QToolButton::clicked, &dependencies.taskGraph,
            &viewmodel::TaskGraphContract::reload);
    connect(m_openDetails, &QPushButton::clicked, this,
            [this] { setDetailsExpanded(true); });
    connect(collapse, &QToolButton::clicked, this,
            [this] { setDetailsExpanded(false); });
    connect(m_pinDetails, &QToolButton::toggled, this, [this](const bool pinned) {
        m_detailsPinned = pinned;
        m_pinDetails->setText(pinned ? QStringLiteral("●") : QStringLiteral("○"));
    });
    connect(center, &QPushButton::clicked, m_view,
            &DependencyGraphView::centerSelectedNode);
    connect(fullDetails, &QPushButton::clicked, this, [this] {
        m_fullDetails->openTask(m_dependencies.taskGraph.selectedTaskId());
    });
    connect(m_editDependencies, &QPushButton::clicked, this, [this] {
        m_dependencyEditor->openTask(m_dependencies.taskGraph.selectedTaskId());
    });
    const auto relationActivated = [this](const QModelIndex &index) {
        selectAndCenter(index.data(Graph::RelationTaskIdRole).toString());
    };
    connect(m_predecessors, &QListView::activated, this, relationActivated);
    connect(m_successors, &QListView::activated, this, relationActivated);
    connect(m_predecessors, &QListView::clicked, this, relationActivated);
    connect(m_successors, &QListView::clicked, this, relationActivated);
    connect(m_view, &DependencyGraphView::zoomFactorChanged, this, [this](qreal factor) {
        m_zoomLabel->setText(tr("%1%").arg(qRound(factor * 100.0)));
        m_zoomOut->setEnabled(factor > 0.5);
        m_zoomIn->setEnabled(factor < 2.0);
    });
    connect(&dependencies.taskGraph, &viewmodel::TaskGraphContract::searchTextChanged,
            this, &DependencyGraphPage::synchronizeControls);
    connect(&dependencies.taskGraph, &viewmodel::TaskGraphContract::statusFilterIndexChanged,
            this, &DependencyGraphPage::synchronizeControls);
    connect(&dependencies.taskGraph, &viewmodel::TaskGraphContract::categoryOptionsChanged,
            this, &DependencyGraphPage::synchronizeControls);
    connect(&dependencies.taskGraph, &viewmodel::TaskGraphContract::categoryFilterChanged,
            this, [this] {
                synchronizeControls();
                QTimer::singleShot(0, m_view, &DependencyGraphView::fitContent);
            });
    connect(&dependencies.taskGraph, &viewmodel::TaskGraphContract::selectionChanged,
            this, [this] {
                synchronizeDetails();
                if (!m_dependencies.taskGraph.selectedTaskId().isEmpty()) {
                    setDetailsExpanded(true);
                    QTimer::singleShot(210, m_view,
                                       &DependencyGraphView::centerSelectedNode);
                } else if (!m_detailsPinned) setDetailsExpanded(false);
            });
    connect(&dependencies.taskGraph, &viewmodel::TaskGraphContract::graphChanged,
            this, [this] {
                m_notification->clear();
                m_notification->hide();
                synchronizeControls();
                synchronizeDetails();
            });
    connect(&dependencies.taskGraph, &viewmodel::TaskGraphContract::notificationRaised,
            this, [this](const common::UiNotification &notification) {
                m_notification->setText(notification.message);
                m_notification->show();
            });
    connect(&dependencies.appearanceSettings,
            &viewmodel::AppearanceSettingsContract::appearanceChanged,
            this, &DependencyGraphPage::applyTheme);
    for (QAbstractItemModel *relations : {dependencies.taskGraph.selectedPredecessors(),
                                          dependencies.taskGraph.selectedSuccessors()}) {
        connect(relations, &QAbstractItemModel::modelReset,
                this, &DependencyGraphPage::synchronizeRelations);
        connect(relations, &QAbstractItemModel::rowsInserted,
                this, &DependencyGraphPage::synchronizeRelations);
        connect(relations, &QAbstractItemModel::rowsRemoved,
                this, &DependencyGraphPage::synchronizeRelations);
    }
    applyTheme();
    synchronizeControls();
    synchronizeDetails();
    synchronizeRelations();
    setDetailsExpanded(false);
}

void DependencyGraphPage::activate()
{
    if (!m_viewportInitialized) {
        m_viewportInitialized = true;
        QTimer::singleShot(0, m_view, &DependencyGraphView::fitContent);
    }
}

void DependencyGraphPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateResponsiveLayout();
    if (m_detailsExpanded)
        QTimer::singleShot(210, m_view, &DependencyGraphView::centerSelectedNode);
}

void DependencyGraphPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    activate();
}

void DependencyGraphPage::synchronizeControls()
{
    auto &graph = m_dependencies.taskGraph;
    const QSignalBlocker searchBlocker(m_search), statusBlocker(m_statusFilter),
                         categoryBlocker(m_categoryFilter);
    m_search->setText(graph.searchText());
    m_statusFilter->setCurrentIndex(graph.statusFilterIndex());
    m_categoryFilter->clear();
    int selectedIndex = 0;
    const QVariantList options = graph.categoryFilterOptions();
    for (int row = 0; row < options.size(); ++row) {
        const QVariantMap option = options.at(row).toMap();
        const int mode = option.value(QStringLiteral("mode")).toInt();
        const QString id = option.value(QStringLiteral("categoryId")).toString();
        m_categoryFilter->addItem(option.value(QStringLiteral("name")).toString());
        m_categoryFilter->setItemData(row, mode, Qt::UserRole);
        m_categoryFilter->setItemData(row, id, Qt::UserRole + 1);
        if (mode == graph.categoryFilterMode()
            && (mode != 2 || id == graph.categoryFilterCategoryId())) selectedIndex = row;
    }
    m_categoryFilter->setCurrentIndex(selectedIndex);
    m_taskCount->setText(tr("%1 项任务").arg(graph.taskCount()));
    m_blockedCount->setText(tr("%1 项阻塞").arg(graph.blockedCount()));
    m_locateCurrent->setEnabled(!graph.currentTaskId().isEmpty());
    m_empty->setText(graph.categoryFilterMode() == 0
        ? tr("还没有可显示的活动任务") : tr("当前类别没有可显示的任务"));
    m_canvasStack->setCurrentWidget(graph.empty() ? static_cast<QWidget *>(m_empty)
                                                   : static_cast<QWidget *>(m_view));
    m_openDetails->setVisible(!m_detailsExpanded && !graph.selectedTaskId().isEmpty());
}

void DependencyGraphPage::synchronizeDetails()
{
    auto &graph = m_dependencies.taskGraph;
    m_selectedTitle->setText(graph.selectedTaskTitle());
    const QString category = graph.selectedHasCategory()
        ? graph.selectedCategoryName() : tr("未分类");
    m_selectedCategory->setText(category);
    m_selectedCategory->setVisible(graph.selectedHasCategory());
    m_selectedContext->setVisible(!graph.selectedCoreNode());
    m_selectedMeta->setText(tr("%1 · %2优先级")
        .arg(graph.selectedStatusText(), graph.selectedPriorityText()));
    m_selectedDescription->setText(graph.selectedDescription().isEmpty()
        ? tr("暂无描述") : graph.selectedDescription());
    m_selectedDeadline->setText(tr("截止时间  %1").arg(graph.selectedDeadlineText()));
    m_selectedDuration->setText(tr("预计用时  %1").arg(
        graph.selectedEstimatedDurationText()));
    m_selectedRelations->setText(tr("直接前置 %1 项 · 直接后继 %2 项 · 可解锁 %3 项")
        .arg(graph.selectedPredecessorCount()).arg(graph.selectedSuccessorCount())
        .arg(graph.selectedUnlockCount()));
    m_selectedBlocking->setText(graph.selectedBlockingReason().isEmpty()
        ? QString{} : tr("阻塞：%1").arg(graph.selectedBlockingReason()));
    m_selectedBlocking->setVisible(!graph.selectedBlockingReason().isEmpty());
    m_editDependencies->setVisible(graph.canEditSelectedDependencies());
    m_openDetails->setVisible(!m_detailsExpanded && !graph.selectedTaskId().isEmpty());
    applyDetailsTheme();
    synchronizeRelations();
}

void DependencyGraphPage::synchronizeRelations()
{
    const int predecessors = m_dependencies.taskGraph.selectedPredecessors()->rowCount();
    const int successors = m_dependencies.taskGraph.selectedSuccessors()->rowCount();
    m_predecessorHeading->setVisible(predecessors > 0);
    m_predecessors->setVisible(predecessors > 0);
    m_predecessors->setFixedHeight(std::min(4, predecessors) * 48 + 4);
    m_successorHeading->setVisible(successors > 0);
    m_successors->setVisible(successors > 0);
    m_successors->setFixedHeight(std::min(4, successors) * 48 + 4);
}

void DependencyGraphPage::applyTheme()
{
    const WidgetTheme theme = WidgetTheme::fromAccentIndex(
        m_dependencies.appearanceSettings.accentThemeIndex());
    m_view->setTheme(theme);
    setFrameColor(*m_canvasFrame, theme.surface, theme.border);
    setFrameColor(*m_detailsPanel, theme.surfaceElevated, theme.border);
    m_notification->setStyleSheet(QStringLiteral(
        "QLabel#graphNotificationLabel { color: %1; border: none; background: transparent; }")
        .arg(theme.danger.name()));
    applyDetailsTheme();
}

void DependencyGraphPage::applyDetailsTheme()
{
    const WidgetTheme theme = WidgetTheme::fromAccentIndex(
        m_dependencies.appearanceSettings.accentThemeIndex());
    m_selectedTitle->setStyleSheet(QStringLiteral(
        "QLabel#selectedGraphTaskTitle { color: %1; font-size: 17px; "
        "font-weight: 700; border: none; background: transparent; }")
        .arg(theme.textPrimary.name()));
    const QColor categoryAccent{m_dependencies.taskGraph.selectedCategoryAccent()};
    m_selectedCategory->setStyleSheet(QStringLiteral(
        "QLabel#selectedGraphTaskCategory { color: %1; font-weight: 600; "
        "padding: 3px 8px; border: 1px solid %1; border-radius: 9px; "
        "background: rgba(%2, %3, %4, 31); }")
        .arg(categoryAccent.name())
        .arg(categoryAccent.red()).arg(categoryAccent.green()).arg(categoryAccent.blue()));
    for (QLabel *label : {m_selectedContext, m_selectedMeta, m_selectedDeadline,
                          m_selectedDuration, m_selectedRelations,
                          m_predecessorHeading, m_successorHeading}) {
        setLabelColor(*label, theme.textSecondary);
        label->setStyleSheet(QStringLiteral("border: none; background: transparent;"));
    }
    setLabelColor(*m_selectedDescription,
                  m_dependencies.taskGraph.selectedDescription().isEmpty()
                      ? theme.textMuted : theme.textBody);
    m_selectedDescription->setStyleSheet(QStringLiteral(
        "QLabel#selectedGraphTaskDescription { border: none; background: transparent; }")
    );
    m_detailsDivider->setStyleSheet(QStringLiteral(
        "QFrame#graphDetailsDivider { border: none; background: %1; }")
        .arg(theme.borderSoft.name()));
    m_predecessors->setStyleSheet(QStringLiteral(
        "QListView#graphPredecessorList { border: none; background: transparent; }")
    );
    m_successors->setStyleSheet(QStringLiteral(
        "QListView#graphSuccessorList { border: none; background: transparent; }")
    );
    m_selectedBlocking->setStyleSheet(QStringLiteral(
        "QLabel#selectedGraphTaskBlockingReason { background: %1; color: %2; "
        "padding: 8px; border: none; border-radius: 8px; }")
        .arg(theme.controlHover.name(), theme.warning.name()));
}

void DependencyGraphPage::updateResponsiveLayout()
{
    if (!m_detailsExpanded) return;
    const int available = std::max(0, width() - 36);
    const int maximumForCanvas = std::max(260, available - 500 - 12);
    const int preferred = qRound(available * 0.36);
    m_detailsPanel->setFixedWidth(std::clamp(
        std::min(preferred, maximumForCanvas), 260, 340));
}

void DependencyGraphPage::setDetailsExpanded(const bool expanded)
{
    m_detailsExpanded = expanded && !m_dependencies.taskGraph.selectedTaskId().isEmpty();
    m_detailsPanel->setVisible(m_detailsExpanded);
    m_openDetails->setVisible(!m_detailsExpanded
        && !m_dependencies.taskGraph.selectedTaskId().isEmpty());
    updateResponsiveLayout();
}

void DependencyGraphPage::selectAndCenter(const QString &taskId)
{
    if (!m_dependencies.taskGraph.selectTask(taskId)) return;
    setDetailsExpanded(true);
    QTimer::singleShot(210, m_view, &DependencyGraphView::centerSelectedNode);
}

} // namespace smartmate::view::widgets
