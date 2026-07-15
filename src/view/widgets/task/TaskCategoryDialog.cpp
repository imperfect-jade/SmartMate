#include "TaskCategoryDialog.h"

#include "common/presentation/UiNotification.h"
#include "viewmodel/contracts/TaskCategoryContract.h"

#include <QAbstractItemModel>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace smartmate::view::widgets {
namespace {

class TaskCategoryDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return {320, 58};
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        painter->save();
        const QRect card = option.rect.adjusted(2, 3, -2, -3);
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(selected ? option.palette.highlight().color()
                                 : option.palette.mid().color());
        painter->setBrush(selected ? option.palette.alternateBase()
                                   : option.palette.base());
        painter->drawRoundedRect(card, 8, 8);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(index.data(
            viewmodel::TaskCategoryContract::AccentRole).toString()));
        painter->drawEllipse(QRect{card.left() + 12, card.center().y() - 8, 16, 16});
        painter->setPen(option.palette.text().color());
        QFont font = option.font;
        font.setBold(true);
        painter->setFont(font);
        painter->drawText(card.adjusted(40, 0, -100, 0),
                          Qt::AlignVCenter | Qt::AlignLeft,
                          index.data(viewmodel::TaskCategoryContract::NameRole).toString());
        painter->setFont(option.font);
        painter->setPen(option.palette.color(QPalette::PlaceholderText));
        painter->drawText(card.adjusted(card.width() - 92, 0, -12, 0),
                          Qt::AlignVCenter | Qt::AlignRight,
                          QObject::tr("%1 项任务").arg(index.data(
                              viewmodel::TaskCategoryContract::TaskCountRole).toInt()));
        painter->restore();
    }
};

} // namespace

TaskCategoryDialog::TaskCategoryDialog(
    viewmodel::TaskCategoryContract &categories, QWidget *parent)
    : QDialog(parent)
    , m_categories(categories)
    , m_list(new QListView(this))
    , m_empty(new QLabel(tr("还没有类别，可以创建学习、工作或旅行等类别。"), this))
    , m_name(new QLineEdit(this))
    , m_color(new QComboBox(this))
    , m_notification(new QLabel(this))
    , m_edit(new QPushButton(tr("编辑所选"), this))
    , m_delete(new QPushButton(tr("删除所选"), this))
    , m_reset(new QPushButton(tr("重置"), this))
    , m_save(new QPushButton(this))
{
    setObjectName(QStringLiteral("taskCategoryDialog"));
    setWindowTitle(tr("管理类别"));
    setModal(true);
    resize(760, 500);
    setMinimumSize(620, 420);

    m_list->setObjectName(QStringLiteral("categoryListView"));
    // Contract 本身是抽象列表模型；Delegate 只消费稳定 Role，不接触类别领域实体。
    m_list->setModel(&m_categories);
    m_list->setItemDelegate(new TaskCategoryDelegate(m_list));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_empty->setObjectName(QStringLiteral("categoryEmptyStateLabel"));
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setWordWrap(true);
    m_name->setObjectName(QStringLiteral("categoryNameField"));
    m_color->setObjectName(QStringLiteral("categoryColorComboBox"));
    m_notification->setObjectName(QStringLiteral("categoryNotificationLabel"));
    m_notification->setWordWrap(true);
    m_edit->setObjectName(QStringLiteral("editSelectedCategoryButton"));
    m_delete->setObjectName(QStringLiteral("deleteSelectedCategoryButton"));
    m_reset->setObjectName(QStringLiteral("resetCategoryDraftButton"));
    m_save->setObjectName(QStringLiteral("saveCategoryButton"));

    auto *root = new QHBoxLayout(this);
    auto *catalog = new QVBoxLayout;
    auto *catalogActions = new QHBoxLayout;
    auto *create = new QPushButton(tr("新建类别"), this);
    create->setObjectName(QStringLiteral("newCategoryButton"));
    catalogActions->addWidget(create);
    catalogActions->addStretch();
    catalogActions->addWidget(m_edit);
    catalogActions->addWidget(m_delete);
    catalog->addLayout(catalogActions);
    catalog->addWidget(m_empty);
    catalog->addWidget(m_list, 1);
    root->addLayout(catalog, 3);

    auto *draft = new QVBoxLayout;
    auto *form = new QFormLayout;
    form->addRow(tr("名称"), m_name);
    form->addRow(tr("颜色"), m_color);
    draft->addLayout(form);
    draft->addWidget(m_notification);
    draft->addStretch();
    auto *draftActions = new QHBoxLayout;
    draftActions->addWidget(m_reset);
    draftActions->addStretch();
    draftActions->addWidget(m_save);
    draft->addLayout(draftActions);
    root->addLayout(draft, 2);

    auto *close = new QDialogButtonBox(QDialogButtonBox::Close, this);
    draft->addWidget(close);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);
    // 控件事件直接转发为 Contract 语义命令；删除确认属于 View 的交互职责。
    connect(create, &QPushButton::clicked, &m_categories,
            &viewmodel::TaskCategoryContract::beginCreate);
    connect(m_edit, &QPushButton::clicked, this, [this] {
        m_categories.beginEdit(selectedCategoryId());
    });
    connect(m_delete, &QPushButton::clicked, this, [this] {
        const QModelIndex index = m_list->currentIndex();
        if (!index.isValid()) return;
        const QString id = index.data(
            viewmodel::TaskCategoryContract::CategoryIdRole).toString();
        const QString name = index.data(
            viewmodel::TaskCategoryContract::NameRole).toString();
        const int taskCount = index.data(
            viewmodel::TaskCategoryContract::TaskCountRole).toInt();
        const QString message = taskCount > 0
            ? tr("删除“%1”后，关联的 %2 项任务将变为未分类。任务状态和全部依赖关系保持不变。")
                  .arg(name).arg(taskCount)
            : tr("确定删除空类别“%1”吗？").arg(name);
        if (QMessageBox::question(this, tr("确认删除类别"), message,
                QMessageBox::Ok | QMessageBox::Cancel,
                QMessageBox::Cancel) == QMessageBox::Ok) {
            m_categories.deleteCategory(id);
        }
    });
    connect(m_reset, &QPushButton::clicked, &m_categories,
            &viewmodel::TaskCategoryContract::cancel);
    connect(m_save, &QPushButton::clicked, &m_categories,
            &viewmodel::TaskCategoryContract::save);
    // textEdited/activated 只代表用户输入，程序性回填不会重复写回草稿。
    connect(m_name, &QLineEdit::textEdited, &m_categories,
            &viewmodel::TaskCategoryContract::setDraftName);
    connect(m_color, &QComboBox::activated, &m_categories,
            &viewmodel::TaskCategoryContract::setDraftColorIndex);
    connect(m_list->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &TaskCategoryDialog::synchronizeActions);
    // Contract 通知只表示 getter 已变化；窗口收到后统一重读当前草稿和资格。
    connect(&m_categories, &viewmodel::TaskCategoryContract::draftChanged,
            this, &TaskCategoryDialog::synchronizeDraft);
    connect(&m_categories, &viewmodel::TaskCategoryContract::countChanged,
            this, &TaskCategoryDialog::synchronizeActions);
    connect(&m_categories, &viewmodel::TaskCategoryContract::saved,
            this, [this] { selectEditingCategory(); });
    connect(&m_categories, &viewmodel::TaskCategoryContract::notificationRaised,
            this, [this](const common::UiNotification &notification) {
                m_notification->setText(notification.message);
            });
    synchronizeDraft();
    synchronizeActions();
}

void TaskCategoryDialog::openManager()
{
    m_notification->clear();
    m_categories.reload();
    synchronizeDraft();
    synchronizeActions();
    selectEditingCategory();
    open();
}

void TaskCategoryDialog::synchronizeDraft()
{
    // 阻断回填信号，严格区分 ViewModel→Widget 同步与用户→Contract 命令。
    const QSignalBlocker nameBlocker(m_name);
    const QSignalBlocker colorBlocker(m_color);
    m_name->setText(m_categories.draftName());
    if (m_color->count() != m_categories.colorOptions().size()) {
        m_color->clear();
        const QStringList names = m_categories.colorOptions();
        const QStringList accents = m_categories.colorAccents();
        for (int i = 0; i < names.size(); ++i) {
            m_color->addItem(names.at(i), i);
            if (i < accents.size()) {
                m_color->setItemData(i, QColor(accents.at(i)), Qt::DecorationRole);
            }
        }
    }
    m_color->setCurrentIndex(m_categories.draftColorIndex());
    m_reset->setEnabled(m_categories.dirty() || m_categories.editMode());
    m_save->setEnabled(m_categories.canSave());
    m_save->setText(m_categories.editMode() ? tr("保存修改") : tr("创建"));
    if (!m_categories.dirty()) m_notification->clear();
}

void TaskCategoryDialog::synchronizeActions()
{
    const bool hasSelection = m_list->currentIndex().isValid();
    m_edit->setEnabled(hasSelection);
    m_delete->setEnabled(hasSelection);
    m_empty->setVisible(m_categories.empty());
    m_list->setVisible(!m_categories.empty());
}

void TaskCategoryDialog::selectEditingCategory()
{
    // 保存或重载后用稳定类别 ID 恢复选择，不能依赖可能变化的行号。
    const QString editingId = m_categories.editingCategoryId();
    if (editingId.isEmpty()) return;
    for (int row = 0; row < m_categories.rowCount(); ++row) {
        const QModelIndex index = m_categories.index(row);
        if (index.data(viewmodel::TaskCategoryContract::CategoryIdRole).toString()
            == editingId) {
            m_list->setCurrentIndex(index);
            return;
        }
    }
}

QString TaskCategoryDialog::selectedCategoryId() const
{
    return m_list->currentIndex().data(
        viewmodel::TaskCategoryContract::CategoryIdRole).toString();
}

} // namespace smartmate::view::widgets
