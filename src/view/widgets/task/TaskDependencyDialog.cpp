#include "TaskDependencyDialog.h"

#include "common/presentation/UiNotification.h"
#include "viewmodel/contracts/TaskDependencyContract.h"

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

class DependencyCandidateDelegate final : public QStyledItemDelegate {
public:
    DependencyCandidateDelegate(viewmodel::TaskDependencyContract &dependencies,
                                QObject *parent)
        : QStyledItemDelegate(parent), m_dependencies(dependencies) {}

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return {520, 76};
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        using Role = viewmodel::TaskDependencyContract::Role;
        painter->save();
        const QRect card = option.rect.adjusted(2, 3, -2, -3);
        const bool selected = index.data(Role::SelectedRole).toBool();
        const bool selectable = index.data(Role::SelectableRole).toBool();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(selected ? option.palette.highlight().color()
                                 : option.palette.mid().color());
        painter->setBrush(selected ? option.palette.alternateBase()
                                   : option.palette.base());
        painter->drawRoundedRect(card, 8, 8);
        QStyleOptionButton check;
        check.rect = {card.left() + 12, card.center().y() - 10, 20, 20};
        check.state = (selectable ? QStyle::State_Enabled : QStyle::State_None)
            | (selected ? QStyle::State_On : QStyle::State_Off);
        QApplication::style()->drawControl(QStyle::CE_CheckBox, &check, painter);
        painter->setPen(option.palette.color(selectable ? QPalette::Text
                                                        : QPalette::PlaceholderText));
        QFont font = option.font;
        font.setBold(true);
        painter->setFont(font);
        painter->drawText(card.adjusted(44, 8, -145, -38),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          index.data(Role::TitleRole).toString());
        painter->setFont(option.font);
        painter->setPen(option.palette.color(QPalette::PlaceholderText));
        const QString details = QObject::tr("ID %1 · %2 · %3优先级")
            .arg(index.data(Role::ShortIdRole).toString(),
                 index.data(Role::StatusTextRole).toString(),
                 index.data(Role::PriorityTextRole).toString());
        painter->drawText(card.adjusted(44, 38, -10, -8),
                          Qt::AlignLeft | Qt::AlignVCenter, details);
        if (index.data(Role::HasCategoryRole).toBool()) {
            painter->setPen(QColor(index.data(Role::CategoryAccentRole).toString()));
            painter->drawText(card.adjusted(card.width() - 137, 8, -10, -38),
                              Qt::AlignRight | Qt::AlignVCenter,
                              index.data(Role::CategoryNameRole).toString());
        }
        painter->restore();
    }

    bool editorEvent(QEvent *event, QAbstractItemModel *,
                     const QStyleOptionViewItem &, const QModelIndex &index) override
    {
        using Role = viewmodel::TaskDependencyContract::Role;
        if (event->type() != QEvent::MouseButtonRelease
            || !index.data(Role::SelectableRole).toBool()) return false;
        const auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton) return false;
        return m_dependencies.setPredecessorSelected(
            index.data(Role::TaskIdRole).toString(),
            !index.data(Role::SelectedRole).toBool());
    }

private:
    viewmodel::TaskDependencyContract &m_dependencies;
};

} // namespace

TaskDependencyDialog::TaskDependencyDialog(
    viewmodel::TaskDependencyContract &dependencies, QWidget *parent)
    : QDialog(parent)
    , m_dependencies(dependencies)
    , m_list(new QListView(this))
    , m_description(new QLabel(this))
    , m_count(new QLabel(this))
    , m_empty(new QLabel(tr("没有可选择的前置任务"), this))
    , m_notification(new QLabel(this))
    , m_save(new QPushButton(tr("保存"), this))
{
    setObjectName(QStringLiteral("taskDependencyDialog"));
    setWindowTitle(tr("管理前置任务"));
    setModal(true);
    resize(640, 540);
    setMinimumSize(480, 420);
    m_description->setWordWrap(true);
    m_count->setObjectName(QStringLiteral("dependencySelectedCountLabel"));
    m_list->setObjectName(QStringLiteral("dependencyCandidateList"));
    // Contract 提供候选、已选和资格 Role；View 不自行重建依赖规则。
    m_list->setModel(&m_dependencies);
    m_list->setItemDelegate(new DependencyCandidateDelegate(m_dependencies, m_list));
    m_empty->setAlignment(Qt::AlignCenter);
    m_notification->setObjectName(QStringLiteral("dependencyNotificationLabel"));
    m_notification->setWordWrap(true);
    m_save->setObjectName(QStringLiteral("saveDependenciesButton"));

    auto *root = new QVBoxLayout(this);
    root->addWidget(m_description);
    root->addWidget(m_count);
    root->addWidget(m_empty);
    root->addWidget(m_list, 1);
    root->addWidget(m_notification);
    auto *buttons = new QDialogButtonBox(this);
    auto *cancel = buttons->addButton(tr("取消"), QDialogButtonBox::RejectRole);
    cancel->setObjectName(QStringLiteral("cancelDependenciesButton"));
    buttons->addButton(m_save, QDialogButtonBox::AcceptRole);
    root->addWidget(buttons);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    // save() 一次提交完整前置集合；失败时保留窗口和草稿，禁止循环写单条依赖。
    connect(m_save, &QPushButton::clicked, this, [this] {
        if (!m_dependencies.save()) return;
        m_draftActive = false;
        QDialog::accept();
    });
    // 多种 Contract 通知共享 synchronize，确保控件始终重读同一份草稿快照。
    connect(&m_dependencies, &viewmodel::TaskDependencyContract::contextChanged,
            this, &TaskDependencyDialog::synchronize);
    connect(&m_dependencies, &viewmodel::TaskDependencyContract::countChanged,
            this, &TaskDependencyDialog::synchronize);
    connect(&m_dependencies, &viewmodel::TaskDependencyContract::selectionChanged,
            this, &TaskDependencyDialog::synchronize);
    connect(&m_dependencies, &viewmodel::TaskDependencyContract::formStateChanged,
            this, &TaskDependencyDialog::synchronize);
    connect(&m_dependencies, &viewmodel::TaskDependencyContract::notificationRaised,
            this, [this](const common::UiNotification &notification) {
                m_notification->setText(notification.message);
            });
    synchronize();
}

bool TaskDependencyDialog::openTask(const QString &taskId)
{
    // 切换目标前先结束旧草稿；只有 Contract 成功建立上下文后才展示窗口。
    if (m_draftActive) m_dependencies.cancel();
    m_notification->clear();
    if (!m_dependencies.beginEdit(taskId)) return false;
    m_draftActive = true;
    synchronize();
    open();
    return true;
}

void TaskDependencyDialog::reject()
{
    if (m_draftActive) {
        m_dependencies.cancel();
        m_draftActive = false;
    }
    QDialog::reject();
}

void TaskDependencyDialog::synchronize()
{
    m_description->setText(tr("为“%1”选择必须先完成的任务。所有前置任务解析后，当前任务才可执行。")
                               .arg(m_dependencies.taskTitle()));
    m_count->setText(tr("已选择 %1 项").arg(m_dependencies.selectedCount()));
    m_save->setEnabled(m_dependencies.canSave());
    m_empty->setVisible(m_dependencies.count() == 0);
    m_list->setVisible(m_dependencies.count() > 0);
    m_list->viewport()->update();
}

} // namespace smartmate::view::widgets
