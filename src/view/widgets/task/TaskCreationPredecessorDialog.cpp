#include "TaskCreationPredecessorDialog.h"

#include "viewmodel/contracts/TaskEditorContract.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListView>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace smartmate::view::widgets {
namespace {

class CreationCandidateDelegate final : public QStyledItemDelegate {
public:
    CreationCandidateDelegate(viewmodel::TaskEditorContract &editor,
                              QObject *parent)
        : QStyledItemDelegate(parent), m_editor(editor) {}

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return {520, 72};
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        using Role = viewmodel::TaskEditorContract::Role;
        painter->save();
        const QRect card = option.rect.adjusted(2, 3, -2, -3);
        const bool selected = index.data(Role::CandidateSelectedRole).toBool();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(selected ? option.palette.highlight().color()
                                 : option.palette.mid().color());
        painter->setBrush(selected ? option.palette.alternateBase()
                                   : option.palette.base());
        painter->drawRoundedRect(card, 8, 8);
        QStyleOptionButton check;
        check.rect = {card.left() + 12, card.center().y() - 10, 20, 20};
        check.state = QStyle::State_Enabled
            | (selected ? QStyle::State_On : QStyle::State_Off);
        QApplication::style()->drawControl(QStyle::CE_CheckBox, &check, painter);
        painter->setPen(option.palette.text().color());
        QFont titleFont = option.font;
        titleFont.setBold(true);
        painter->setFont(titleFont);
        painter->drawText(card.adjusted(44, 8, -140, -34),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          index.data(Role::CandidateTitleRole).toString());
        painter->setFont(option.font);
        painter->setPen(option.palette.color(QPalette::PlaceholderText));
        const QString detail = QObject::tr("ID %1 · %2 · %3优先级")
            .arg(index.data(Role::CandidateShortIdRole).toString(),
                 index.data(Role::CandidateStatusTextRole).toString(),
                 index.data(Role::CandidatePriorityTextRole).toString());
        painter->drawText(card.adjusted(44, 34, -10, -8),
                          Qt::AlignLeft | Qt::AlignVCenter, detail);
        if (index.data(Role::CandidateHasCategoryRole).toBool()) {
            painter->setPen(QColor(index.data(
                Role::CandidateCategoryAccentRole).toString()));
            painter->drawText(card.adjusted(card.width() - 132, 8, -10, -34),
                              Qt::AlignRight | Qt::AlignVCenter,
                              index.data(Role::CandidateCategoryNameRole).toString());
        }
        painter->restore();
    }

    bool editorEvent(QEvent *event, QAbstractItemModel *,
                     const QStyleOptionViewItem &, const QModelIndex &index) override
    {
        if (event->type() != QEvent::MouseButtonRelease) return false;
        const auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton) return false;
        const bool selected = index.data(
            viewmodel::TaskEditorContract::CandidateSelectedRole).toBool();
        return m_editor.setCreationPredecessorSelected(
            index.data(viewmodel::TaskEditorContract::CandidateTaskIdRole).toString(),
            !selected);
    }

private:
    viewmodel::TaskEditorContract &m_editor;
};

} // namespace

TaskCreationPredecessorDialog::TaskCreationPredecessorDialog(
    viewmodel::TaskEditorContract &editor, QWidget *parent)
    : QDialog(parent)
    , m_editor(editor)
    , m_list(new QListView(this))
    , m_count(new QLabel(this))
    , m_empty(new QLabel(tr("还没有可作为前置的活动任务"), this))
    , m_clear(new QPushButton(tr("清空"), this))
{
    setObjectName(QStringLiteral("taskCreationPredecessorDialog"));
    setWindowTitle(tr("选择新任务的前置任务"));
    setModal(true);
    resize(640, 520);
    setMinimumSize(480, 400);
    m_list->setObjectName(QStringLiteral("creationPredecessorCandidateList"));
    // TaskEditorContract 在此前置候选视图中临时充当列表模型，选择身份仍为稳定 TaskId。
    m_list->setModel(&m_editor);
    m_list->setItemDelegate(new CreationCandidateDelegate(m_editor, m_list));
    m_count->setObjectName(QStringLiteral("creationPredecessorSelectedCountLabel"));
    m_empty->setAlignment(Qt::AlignCenter);
    m_clear->setObjectName(QStringLiteral("clearCreationPredecessorsButton"));

    auto *root = new QVBoxLayout(this);
    auto *description = new QLabel(
        tr("所选任务必须全部完成，新任务才会自动解锁。任务与依赖将在保存时一次写入。"), this);
    description->setWordWrap(true);
    root->addWidget(description);
    root->addWidget(m_count);
    root->addWidget(m_empty);
    root->addWidget(m_list, 1);
    auto *buttons = new QDialogButtonBox(this);
    auto *cancel = buttons->addButton(tr("取消"), QDialogButtonBox::RejectRole);
    auto *accept = buttons->addButton(tr("确定"), QDialogButtonBox::AcceptRole);
    cancel->setObjectName(QStringLiteral("cancelCreationPredecessorsButton"));
    accept->setObjectName(QStringLiteral("acceptCreationPredecessorsButton"));
    buttons->addButton(m_clear, QDialogButtonBox::ActionRole);
    root->addWidget(buttons);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    // 确认/取消只提交或回滚前置选择子会话，真正创建任务仍由主编辑器一次保存。
    connect(accept, &QPushButton::clicked, this, [this] {
        m_editor.acceptPredecessorSelection();
        m_selectionActive = false;
        QDialog::accept();
    });
    connect(m_clear, &QPushButton::clicked, &m_editor,
            &viewmodel::TaskEditorContract::clearCreationPredecessors);
    connect(&m_editor, &viewmodel::TaskEditorContract::predecessorCandidatesChanged,
            this, &TaskCreationPredecessorDialog::synchronize);
    connect(&m_editor, &viewmodel::TaskEditorContract::predecessorSelectionChanged,
            this, &TaskCreationPredecessorDialog::synchronize);
    synchronize();
}

void TaskCreationPredecessorDialog::openSelection()
{
    // 先建立检查点再显示窗口，保证关闭窗口能够恢复进入前的选择集合。
    m_editor.beginPredecessorSelection();
    m_selectionActive = true;
    synchronize();
    open();
}

void TaskCreationPredecessorDialog::reject()
{
    if (m_selectionActive) {
        m_editor.cancelPredecessorSelection();
        m_selectionActive = false;
    }
    QDialog::reject();
}

void TaskCreationPredecessorDialog::synchronize()
{
    // 列表行的选中 Role 由 Contract 通知刷新；这里仅同步聚合计数和空状态。
    m_count->setText(tr("已选择 %1 项").arg(m_editor.selectedPredecessorCount()));
    m_clear->setEnabled(m_editor.selectedPredecessorCount() > 0);
    m_empty->setVisible(m_editor.predecessorCandidateCount() == 0);
    m_list->setVisible(m_editor.predecessorCandidateCount() > 0);
    m_list->viewport()->update();
}

} // namespace smartmate::view::widgets
