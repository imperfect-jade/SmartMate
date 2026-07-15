#include "DependencyGraphToolbar.h"

#include "viewmodel/contracts/TaskGraphContract.h"

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QToolButton>

namespace smartmate::view::widgets {

DependencyGraphToolbar::DependencyGraphToolbar(
    viewmodel::TaskGraphContract &graph, QWidget *parent)
    : QFrame(parent)
    , m_graph(graph)
    , m_search(new QLineEdit(this))
    , m_statusFilter(new QComboBox(this))
    , m_categoryFilter(new QComboBox(this))
    , m_taskCount(new QLabel(this))
    , m_blockedCount(new QLabel(this))
    , m_locateCurrent(new QPushButton(tr("定位当前"), this))
    , m_zoomOut(new QToolButton(this))
    , m_zoomIn(new QToolButton(this))
    , m_zoomLabel(new QLabel(this))
{
    setObjectName(QStringLiteral("dependencyGraphToolbar"));
    auto *layout = new QGridLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setHorizontalSpacing(8);
    auto *title = new QLabel(tr("依赖图"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    m_taskCount->setObjectName(QStringLiteral("graphTaskCountLabel"));
    m_blockedCount->setObjectName(QStringLiteral("graphBlockedCountLabel"));
    m_search->setObjectName(QStringLiteral("graphSearchField"));
    m_search->setPlaceholderText(tr("搜索并定位任务"));
    m_search->setMinimumWidth(150);
    m_statusFilter->setObjectName(QStringLiteral("graphStatusFilter"));
    m_statusFilter->addItems(m_graph.statusFilterOptions());
    m_categoryFilter->setObjectName(QStringLiteral("graphCategoryFilter"));
    m_locateCurrent->setObjectName(QStringLiteral("locateCurrentGraphTaskButton"));
    m_zoomOut->setObjectName(QStringLiteral("zoomOutGraphButton"));
    m_zoomOut->setText(QStringLiteral("−"));
    m_zoomIn->setObjectName(QStringLiteral("zoomInGraphButton"));
    m_zoomIn->setText(QStringLiteral("+"));
    m_zoomLabel->setObjectName(QStringLiteral("graphZoomLabel"));
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    auto *resetZoom = new QPushButton(tr("100%"), this);
    resetZoom->setObjectName(QStringLiteral("resetGraphZoomButton"));
    auto *fit = new QPushButton(tr("适应画布"), this);
    fit->setObjectName(QStringLiteral("fitGraphButton"));
    auto *reload = new QToolButton(this);
    reload->setObjectName(QStringLiteral("reloadGraphButton"));
    reload->setText(QStringLiteral("↻"));
    layout->addWidget(title, 0, 0);
    layout->addWidget(m_taskCount, 0, 1);
    layout->addWidget(m_blockedCount, 0, 2);
    layout->addWidget(m_search, 0, 3, 1, 2);
    layout->addWidget(m_statusFilter, 0, 5);
    layout->addWidget(m_categoryFilter, 0, 6);
    layout->setColumnStretch(3, 1);
    layout->addWidget(m_locateCurrent, 1, 2);
    layout->addWidget(m_zoomOut, 1, 3);
    layout->addWidget(m_zoomLabel, 1, 4);
    layout->addWidget(m_zoomIn, 1, 5);
    layout->addWidget(resetZoom, 1, 6);
    layout->addWidget(fit, 1, 7);
    layout->addWidget(reload, 1, 8);

    // 筛选控件只转发用户输入；类别身份始终使用 Contract 提供的稳定 ID。
    connect(m_search, &QLineEdit::textEdited, &m_graph,
            &viewmodel::TaskGraphContract::setSearchText);
    connect(m_search, &QLineEdit::returnPressed, this,
            &DependencyGraphToolbar::locateFirstMatchRequested);
    connect(m_statusFilter, &QComboBox::activated, &m_graph,
            &viewmodel::TaskGraphContract::setStatusFilterIndex);
    connect(m_categoryFilter, &QComboBox::activated, this, [this](const int index) {
        m_graph.setCategoryFilter(
            m_categoryFilter->itemData(index, Qt::UserRole).toInt(),
            m_categoryFilter->itemData(index, Qt::UserRole + 1).toString());
    });
    connect(m_locateCurrent, &QPushButton::clicked, this,
            &DependencyGraphToolbar::locateCurrentRequested);
    connect(m_zoomOut, &QToolButton::clicked, this,
            &DependencyGraphToolbar::zoomOutRequested);
    connect(m_zoomIn, &QToolButton::clicked, this,
            &DependencyGraphToolbar::zoomInRequested);
    connect(resetZoom, &QPushButton::clicked, this,
            &DependencyGraphToolbar::resetZoomRequested);
    connect(fit, &QPushButton::clicked, this,
            &DependencyGraphToolbar::fitRequested);
    connect(reload, &QToolButton::clicked, &m_graph,
            &viewmodel::TaskGraphContract::reload);
    synchronize();
    setZoomFactor(1.0);
}

void DependencyGraphToolbar::synchronize()
{
    // Contract→Widget 回填阻断控件信号，避免形成反向命令循环。
    const QSignalBlocker searchBlocker(m_search), statusBlocker(m_statusFilter),
                         categoryBlocker(m_categoryFilter);
    m_search->setText(m_graph.searchText());
    m_statusFilter->setCurrentIndex(m_graph.statusFilterIndex());
    m_categoryFilter->clear();
    int selectedIndex = 0;
    const QVariantList options = m_graph.categoryFilterOptions();
    for (int row = 0; row < options.size(); ++row) {
        const QVariantMap option = options.at(row).toMap();
        const int mode = option.value(QStringLiteral("mode")).toInt();
        const QString id = option.value(QStringLiteral("categoryId")).toString();
        m_categoryFilter->addItem(option.value(QStringLiteral("name")).toString());
        m_categoryFilter->setItemData(row, mode, Qt::UserRole);
        m_categoryFilter->setItemData(row, id, Qt::UserRole + 1);
        if (mode == m_graph.categoryFilterMode()
            && (mode != 2 || id == m_graph.categoryFilterCategoryId())) {
            selectedIndex = row;
        }
    }
    m_categoryFilter->setCurrentIndex(selectedIndex);
    m_taskCount->setText(tr("%1 项任务").arg(m_graph.taskCount()));
    m_blockedCount->setText(tr("%1 项阻塞").arg(m_graph.blockedCount()));
    m_locateCurrent->setEnabled(!m_graph.currentTaskId().isEmpty());
}

void DependencyGraphToolbar::setZoomFactor(const qreal factor)
{
    m_zoomLabel->setText(tr("%1%").arg(qRound(factor * 100.0)));
    m_zoomOut->setEnabled(factor > 0.5);
    m_zoomIn->setEnabled(factor < 2.0);
}

} // namespace smartmate::view::widgets
