#include "TaskGraphDetailsPanel.h"

#include "view/widgets/theme/WidgetTheme.h"
#include "viewmodel/contracts/TaskGraphContract.h"

#include <QAbstractItemModel>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QStyledItemDelegate>
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

void setLabelColor(QLabel &label, const QColor &color)
{
    QPalette palette = label.palette();
    palette.setColor(QPalette::WindowText, color);
    label.setPalette(palette);
}

} // namespace

TaskGraphDetailsPanel::TaskGraphDetailsPanel(
    viewmodel::TaskGraphContract &graph, QWidget *parent)
    : QFrame(parent)
    , m_graph(graph)
    , m_pin(new QToolButton(this))
    , m_selectedTitle(new QLabel(this))
    , m_selectedCategory(new QLabel(this))
    , m_selectedContext(new QLabel(this))
    , m_selectedMeta(new QLabel(this))
    , m_selectedDescription(new QLabel(this))
    , m_divider(new QFrame(this))
    , m_selectedDeadline(new QLabel(this))
    , m_selectedDuration(new QLabel(this))
    , m_selectedRelations(new QLabel(this))
    , m_selectedBlocking(new QLabel(this))
    , m_predecessorHeading(new QLabel(tr("直接前置"), this))
    , m_successorHeading(new QLabel(tr("直接后继"), this))
    , m_predecessors(new QListView(this))
    , m_successors(new QListView(this))
    , m_editDependencies(new QPushButton(tr("编辑前置任务"), this))
{
    setObjectName(QStringLiteral("dependencyGraphDetails"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);
    auto *header = new QHBoxLayout;
    auto *title = new QLabel(tr("任务详情"), this);
    title->setObjectName(QStringLiteral("sectionTitle"));
    m_pin->setObjectName(QStringLiteral("pinGraphDetailsButton"));
    m_pin->setCheckable(true);
    m_pin->setText(QStringLiteral("○"));
    auto *collapse = new QToolButton(this);
    collapse->setObjectName(QStringLiteral("collapseGraphDetailsButton"));
    collapse->setText(QStringLiteral("›"));
    header->addWidget(title, 1);
    header->addWidget(m_pin);
    header->addWidget(collapse);
    layout->addLayout(header);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("graphDetailsScrollView"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("graphDetailsContent"));
    auto *body = new QVBoxLayout(content);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(10);
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
    m_divider->setObjectName(QStringLiteral("graphDetailsDivider"));
    m_divider->setFrameShape(QFrame::HLine);
    m_divider->setFixedHeight(1);
    m_selectedDeadline->setObjectName(QStringLiteral("selectedGraphTaskDeadline"));
    m_selectedDeadline->setWordWrap(true);
    m_selectedDuration->setObjectName(QStringLiteral("selectedGraphTaskDuration"));
    m_selectedDuration->setWordWrap(true);
    m_selectedRelations->setObjectName(QStringLiteral("selectedGraphTaskRelations"));
    m_selectedRelations->setWordWrap(true);
    m_selectedBlocking->setObjectName(QStringLiteral("selectedGraphTaskBlockingReason"));
    m_selectedBlocking->setWordWrap(true);
    body->addWidget(m_selectedTitle);
    body->addLayout(categoryRow);
    body->addWidget(m_selectedMeta);
    body->addWidget(m_selectedDescription);
    body->addWidget(m_divider);
    body->addWidget(m_selectedDeadline);
    body->addWidget(m_selectedDuration);
    body->addWidget(m_selectedRelations);
    body->addWidget(m_selectedBlocking);

    m_predecessors->setObjectName(QStringLiteral("graphPredecessorList"));
    m_successors->setObjectName(QStringLiteral("graphSuccessorList"));
    // 关系列表直接绑定 Contract 子模型，不遍历主图重建邻接关系。
    m_predecessors->setModel(m_graph.selectedPredecessors());
    m_successors->setModel(m_graph.selectedSuccessors());
    m_predecessors->setItemDelegate(new RelationDelegate(m_predecessors));
    m_successors->setItemDelegate(new RelationDelegate(m_successors));
    m_predecessors->setFrameShape(QFrame::NoFrame);
    m_successors->setFrameShape(QFrame::NoFrame);
    m_predecessors->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_successors->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_predecessors->setMouseTracking(true);
    m_successors->setMouseTracking(true);
    body->addWidget(m_predecessorHeading);
    body->addWidget(m_predecessors);
    body->addWidget(m_successorHeading);
    body->addWidget(m_successors);
    body->addStretch();
    scroll->setWidget(content);
    layout->addWidget(scroll, 1);

    auto *center = new QPushButton(tr("在画布中居中"), this);
    center->setObjectName(QStringLiteral("centerSelectedGraphTaskButton"));
    auto *fullDetails = new QPushButton(tr("查看完整详情"), this);
    fullDetails->setObjectName(QStringLiteral("openSelectedGraphTaskDetailsButton"));
    m_editDependencies->setObjectName(
        QStringLiteral("editSelectedGraphDependenciesButton"));
    layout->addWidget(center);
    layout->addWidget(fullDetails);
    layout->addWidget(m_editDependencies);

    connect(collapse, &QToolButton::clicked, this,
            &TaskGraphDetailsPanel::collapseRequested);
    connect(m_pin, &QToolButton::toggled, this, [this](const bool pinned) {
        m_pin->setText(pinned ? QStringLiteral("●") : QStringLiteral("○"));
        emit pinnedChanged(pinned);
    });
    connect(center, &QPushButton::clicked, this,
            &TaskGraphDetailsPanel::centerRequested);
    connect(fullDetails, &QPushButton::clicked, this, [this] {
        emit fullDetailsRequested(m_graph.selectedTaskId());
    });
    connect(m_editDependencies, &QPushButton::clicked, this, [this] {
        emit editDependenciesRequested(m_graph.selectedTaskId());
    });
    const auto relationActivated = [this](const QModelIndex &index) {
        emit taskActivated(index.data(Graph::RelationTaskIdRole).toString());
    };
    connect(m_predecessors, &QListView::activated, this, relationActivated);
    connect(m_successors, &QListView::activated, this, relationActivated);
    connect(m_predecessors, &QListView::clicked, this, relationActivated);
    connect(m_successors, &QListView::clicked, this, relationActivated);
    for (QAbstractItemModel *relations : {m_graph.selectedPredecessors(),
                                          m_graph.selectedSuccessors()}) {
        connect(relations, &QAbstractItemModel::modelReset,
                this, &TaskGraphDetailsPanel::synchronizeRelations);
        connect(relations, &QAbstractItemModel::rowsInserted,
                this, &TaskGraphDetailsPanel::synchronizeRelations);
        connect(relations, &QAbstractItemModel::rowsRemoved,
                this, &TaskGraphDetailsPanel::synchronizeRelations);
    }
    synchronize();
    synchronizeRelations();
}

void TaskGraphDetailsPanel::synchronize()
{
    // 阻塞原因、计数和编辑资格均由 Contract 所选节点投影提供。
    m_selectedTitle->setText(m_graph.selectedTaskTitle());
    const QString category = m_graph.selectedHasCategory()
        ? m_graph.selectedCategoryName() : tr("未分类");
    m_selectedCategory->setText(category);
    m_selectedCategory->setVisible(m_graph.selectedHasCategory());
    m_selectedContext->setVisible(!m_graph.selectedCoreNode());
    m_selectedMeta->setText(tr("%1 · %2优先级")
        .arg(m_graph.selectedStatusText(), m_graph.selectedPriorityText()));
    m_selectedDescription->setText(m_graph.selectedDescription().isEmpty()
        ? tr("暂无描述") : m_graph.selectedDescription());
    m_selectedDeadline->setText(tr("截止时间  %1").arg(
        m_graph.selectedDeadlineText()));
    m_selectedDuration->setText(tr("预计用时  %1").arg(
        m_graph.selectedEstimatedDurationText()));
    m_selectedRelations->setText(tr("直接前置 %1 项 · 直接后继 %2 项 · 可解锁 %3 项")
        .arg(m_graph.selectedPredecessorCount()).arg(m_graph.selectedSuccessorCount())
        .arg(m_graph.selectedUnlockCount()));
    m_selectedBlocking->setText(m_graph.selectedBlockingReason().isEmpty()
        ? QString{} : tr("阻塞：%1").arg(m_graph.selectedBlockingReason()));
    m_selectedBlocking->setVisible(!m_graph.selectedBlockingReason().isEmpty());
    m_editDependencies->setVisible(m_graph.canEditSelectedDependencies());
    synchronizeRelations();
}

void TaskGraphDetailsPanel::synchronizeRelations()
{
    const int predecessors = m_graph.selectedPredecessors()->rowCount();
    const int successors = m_graph.selectedSuccessors()->rowCount();
    m_predecessorHeading->setVisible(predecessors > 0);
    m_predecessors->setVisible(predecessors > 0);
    m_predecessors->setFixedHeight(std::min(4, predecessors) * 48 + 4);
    m_successorHeading->setVisible(successors > 0);
    m_successors->setVisible(successors > 0);
    m_successors->setFixedHeight(std::min(4, successors) * 48 + 4);
}

void TaskGraphDetailsPanel::applyTheme(const WidgetTheme &theme)
{
    setStyleSheet(QStringLiteral(
        "QFrame#dependencyGraphDetails { background: %1; border: 1px solid %2; "
        "border-radius: 12px; }")
        .arg(theme.surfaceElevated.name(), theme.border.name()));
    m_selectedTitle->setStyleSheet(QStringLiteral(
        "QLabel#selectedGraphTaskTitle { color: %1; font-size: 17px; "
        "font-weight: 700; border: none; background: transparent; }")
        .arg(theme.textPrimary.name()));
    const QColor categoryAccent{m_graph.selectedCategoryAccent()};
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
                  m_graph.selectedDescription().isEmpty()
                      ? theme.textMuted : theme.textBody);
    m_selectedDescription->setStyleSheet(QStringLiteral(
        "QLabel#selectedGraphTaskDescription { border: none; background: transparent; }"));
    m_divider->setStyleSheet(QStringLiteral(
        "QFrame#graphDetailsDivider { border: none; background: %1; }")
        .arg(theme.borderSoft.name()));
    m_predecessors->setStyleSheet(QStringLiteral(
        "QListView#graphPredecessorList { border: none; background: transparent; }"));
    m_successors->setStyleSheet(QStringLiteral(
        "QListView#graphSuccessorList { border: none; background: transparent; }"));
    m_selectedBlocking->setStyleSheet(QStringLiteral(
        "QLabel#selectedGraphTaskBlockingReason { background: %1; color: %2; "
        "padding: 8px; border: none; border-radius: 8px; }")
        .arg(theme.controlHover.name(), theme.warning.name()));
}

} // namespace smartmate::view::widgets
