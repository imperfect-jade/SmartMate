#include "DependencyGraphPage.h"

#include "DependencyGraphToolbar.h"
#include "DependencyGraphView.h"
#include "TaskGraphDetailsPanel.h"
#include "view/widgets/task/TaskDependencyDialog.h"
#include "view/widgets/task/TaskDetailsDialog.h"
#include "view/widgets/theme/WidgetTheme.h"
#include "common/presentation/UiNotification.h"
#include "viewmodel/contracts/AppearanceSettingsContract.h"
#include "viewmodel/contracts/TaskDependencyContract.h"
#include "viewmodel/contracts/TaskDetailsContract.h"
#include "viewmodel/contracts/TaskGraphContract.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStackedLayout>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace smartmate::view::widgets {
namespace {

void setFrameColor(QFrame &frame, const QColor &background, const QColor &border)
{
    Q_ASSERT(!frame.objectName().isEmpty());
    frame.setStyleSheet(QStringLiteral(
        "QFrame#%1 { background: %2; border: 1px solid %3; border-radius: 12px; }")
        .arg(frame.objectName(), background.name(), border.name()));
}

} // namespace

DependencyGraphPage::DependencyGraphPage(
    DependencyGraphPageDependencies dependencies, QWidget *parent)
    : QWidget(parent)
    , m_dependencies(dependencies)
    , m_toolbar(new DependencyGraphToolbar(dependencies.taskGraph, this))
    , m_notification(new QLabel(this))
    , m_canvasFrame(new QFrame(this))
    , m_view(new DependencyGraphView(dependencies.taskGraph, m_canvasFrame))
    , m_empty(new QLabel(m_canvasFrame))
    , m_canvasStack(new QStackedLayout)
    , m_openDetails(new QPushButton(tr("任务详情 ‹"), m_canvasFrame))
    , m_detailsPanel(new TaskGraphDetailsPanel(dependencies.taskGraph, this))
    , m_fullDetails(new TaskDetailsDialog(dependencies.taskDetails, this))
    , m_dependencyEditor(new TaskDependencyDialog(dependencies.taskDependencies, this))
{
    setObjectName(QStringLiteral("dependencyGraphPage"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(12);
    root->addWidget(m_toolbar);

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
    content->addWidget(m_detailsPanel);
    root->addLayout(content, 1);

    m_fullDetails->setObjectName(QStringLiteral("graphTaskDetailsDialog"));
    m_fullDetails->setActionsVisible(false);
    m_dependencyEditor->setObjectName(QStringLiteral("graphTaskDependencyDialog"));

    // 工具栏只上送视口和选择动作；筛选命令已由工具栏直接转发给 Contract。
    connect(m_toolbar, &DependencyGraphToolbar::locateFirstMatchRequested,
            this, [this] {
                if (m_dependencies.taskGraph.locateFirstMatch()) {
                    setDetailsExpanded(true);
                    QTimer::singleShot(210, m_view,
                                       &DependencyGraphView::centerSelectedNode);
                }
            });
    connect(m_toolbar, &DependencyGraphToolbar::locateCurrentRequested,
            this, [this] {
                if (m_dependencies.taskGraph.selectCurrentTask()) {
                    selectAndCenter(m_dependencies.taskGraph.selectedTaskId());
                }
            });
    connect(m_toolbar, &DependencyGraphToolbar::zoomOutRequested, this,
            [this] { m_view->setZoomFactor(m_view->zoomFactor() - 0.1); });
    connect(m_toolbar, &DependencyGraphToolbar::zoomInRequested, this,
            [this] { m_view->setZoomFactor(m_view->zoomFactor() + 0.1); });
    connect(m_toolbar, &DependencyGraphToolbar::resetZoomRequested, this,
            [this] { m_view->setZoomFactor(1.0); });
    connect(m_toolbar, &DependencyGraphToolbar::fitRequested,
            m_view, &DependencyGraphView::fitContent);

    connect(m_openDetails, &QPushButton::clicked, this,
            [this] { setDetailsExpanded(true); });
    connect(m_detailsPanel, &TaskGraphDetailsPanel::collapseRequested, this,
            [this] { setDetailsExpanded(false); });
    connect(m_detailsPanel, &TaskGraphDetailsPanel::pinnedChanged, this,
            [this](const bool pinned) { m_detailsPinned = pinned; });
    connect(m_detailsPanel, &TaskGraphDetailsPanel::centerRequested,
            m_view, &DependencyGraphView::centerSelectedNode);
    connect(m_detailsPanel, &TaskGraphDetailsPanel::fullDetailsRequested,
            m_fullDetails, &TaskDetailsDialog::openTask);
    connect(m_detailsPanel, &TaskGraphDetailsPanel::editDependenciesRequested,
            m_dependencyEditor, &TaskDependencyDialog::openTask);
    connect(m_detailsPanel, &TaskGraphDetailsPanel::taskActivated,
            this, &DependencyGraphPage::selectAndCenter);
    connect(m_view, &DependencyGraphView::zoomFactorChanged,
            m_toolbar, &DependencyGraphToolbar::setZoomFactor);

    // Contract→Widget：通知到达后，各组件重读当前 getter/Role。
    connect(&dependencies.taskGraph, &viewmodel::TaskGraphContract::searchTextChanged,
            this, &DependencyGraphPage::synchronizeControls);
    connect(&dependencies.taskGraph,
            &viewmodel::TaskGraphContract::statusFilterIndexChanged,
            this, &DependencyGraphPage::synchronizeControls);
    connect(&dependencies.taskGraph,
            &viewmodel::TaskGraphContract::categoryOptionsChanged,
            this, &DependencyGraphPage::synchronizeControls);
    connect(&dependencies.taskGraph,
            &viewmodel::TaskGraphContract::categoryFilterChanged,
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
                } else if (!m_detailsPinned) {
                    setDetailsExpanded(false);
                }
            });
    connect(&dependencies.taskGraph, &viewmodel::TaskGraphContract::graphChanged,
            this, [this] {
                m_notification->clear();
                m_notification->hide();
                synchronizeControls();
                synchronizeDetails();
            });
    connect(&dependencies.taskGraph,
            &viewmodel::TaskGraphContract::notificationRaised,
            this, [this](const common::UiNotification &notification) {
                m_notification->setText(notification.message);
                m_notification->show();
            });
    connect(&dependencies.appearanceSettings,
            &viewmodel::AppearanceSettingsContract::appearanceChanged,
            this, &DependencyGraphPage::applyTheme);

    // 连接完成后立即同步当前快照，页面首次显示不依赖未来通知。
    applyTheme();
    synchronizeControls();
    synchronizeDetails();
    setDetailsExpanded(false);
}

void DependencyGraphPage::activate()
{
    // 延迟到事件循环后适配，确保首次布局已获得真实 viewport 尺寸。
    if (!m_viewportInitialized) {
        m_viewportInitialized = true;
        QTimer::singleShot(0, m_view, &DependencyGraphView::fitContent);
    }
}

void DependencyGraphPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateResponsiveLayout();
    if (m_detailsExpanded) {
        QTimer::singleShot(210, m_view, &DependencyGraphView::centerSelectedNode);
    }
}

void DependencyGraphPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    activate();
}

void DependencyGraphPage::synchronizeControls()
{
    auto &graph = m_dependencies.taskGraph;
    m_toolbar->synchronize();
    m_empty->setText(graph.categoryFilterMode() == 0
        ? tr("还没有可显示的活动任务")
        : tr("当前类别没有可显示的任务"));
    m_canvasStack->setCurrentWidget(graph.empty()
        ? static_cast<QWidget *>(m_empty)
        : static_cast<QWidget *>(m_view));
    m_openDetails->setVisible(!m_detailsExpanded
                              && !graph.selectedTaskId().isEmpty());
}

void DependencyGraphPage::synchronizeDetails()
{
    m_detailsPanel->synchronize();
    m_openDetails->setVisible(
        !m_detailsExpanded
        && !m_dependencies.taskGraph.selectedTaskId().isEmpty());
    m_detailsPanel->applyTheme(WidgetTheme::fromAccentIndex(
        m_dependencies.appearanceSettings.accentThemeIndex()));
}

void DependencyGraphPage::applyTheme()
{
    const WidgetTheme theme = WidgetTheme::fromAccentIndex(
        m_dependencies.appearanceSettings.accentThemeIndex());
    m_view->setTheme(theme);
    setFrameColor(*m_canvasFrame, theme.surface, theme.border);
    m_detailsPanel->applyTheme(theme);
    m_notification->setStyleSheet(QStringLiteral(
        "QLabel#graphNotificationLabel { color: %1; border: none; "
        "background: transparent; }")
        .arg(theme.danger.name()));
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
    // 没有稳定选择时不允许展开空详情；展开状态仅保存在页面生命周期内。
    m_detailsExpanded = expanded
        && !m_dependencies.taskGraph.selectedTaskId().isEmpty();
    m_detailsPanel->setVisible(m_detailsExpanded);
    m_openDetails->setVisible(
        !m_detailsExpanded
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
