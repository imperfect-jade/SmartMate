#include "TaskDetailsDialog.h"

#include "viewmodel/contracts/TaskDetailsContract.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

namespace smartmate::view::widgets {

TaskDetailsDialog::TaskDetailsDialog(viewmodel::TaskDetailsContract &details,
                                     QWidget *parent)
    : QDialog(parent), m_details(details), m_title(new QLabel(this))
    , m_summary(new QLabel(this)), m_description(new QLabel(this))
    , m_schedule(new QLabel(this)), m_insight(new QLabel(this))
    , m_edit(new QPushButton(tr("编辑任务"), this))
    , m_editDependencies(new QPushButton(tr("编辑前置任务"), this))
{
    setObjectName(QStringLiteral("taskDetailsDialog"));
    setWindowTitle(tr("任务详情"));
    setModal(true);
    resize(560, 440);
    m_edit->setObjectName(QStringLiteral("editSelectedTaskButton"));
    m_editDependencies->setObjectName(QStringLiteral("editSelectedDependenciesButton"));
    auto *layout = new QVBoxLayout(this);
    m_title->setObjectName(QStringLiteral("sectionTitle"));
    m_description->setWordWrap(true);
    m_insight->setWordWrap(true);
    layout->addWidget(m_title);
    layout->addWidget(m_summary);
    layout->addWidget(m_description, 1);
    layout->addWidget(m_schedule);
    layout->addWidget(m_insight);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->addButton(m_edit, QDialogButtonBox::ActionRole);
    buttons->addButton(m_editDependencies, QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_edit, &QPushButton::clicked, this, [this] {
        const QString id = m_details.selectedTaskId();
        accept();
        emit editRequested(id);
    });
    connect(m_editDependencies, &QPushButton::clicked, this, [this] {
        const QString id = m_details.selectedTaskId();
        accept();
        emit editDependenciesRequested(id);
    });
    connect(&m_details, &viewmodel::TaskDetailsContract::selectionChanged,
            this, &TaskDetailsDialog::synchronize);
    synchronize();
}

bool TaskDetailsDialog::openTask(const QString &taskId)
{
    if (!m_details.selectTask(taskId)) return false;
    open();
    return true;
}

void TaskDetailsDialog::setActionsVisible(const bool visible)
{
    m_actionsVisible = visible;
    synchronize();
}

void TaskDetailsDialog::done(const int result)
{
    QDialog::done(result);
    m_details.clearSelection();
}

void TaskDetailsDialog::synchronize()
{
    m_title->setText(m_details.selectedTitle());
    QStringList summary{m_details.selectedStatusText(),
                        tr("%1优先级").arg(m_details.selectedPriorityText())};
    if (m_details.selectedHasCategory()) summary << m_details.selectedCategoryName();
    m_summary->setText(summary.join(QStringLiteral(" · ")));
    m_description->setText(m_details.selectedDescription().isEmpty()
        ? tr("暂无描述") : m_details.selectedDescription());
    m_schedule->setText(tr("截止：%1    预计：%2 分钟\n创建：%3    更新：%4")
        .arg(m_details.selectedDeadlineText().isEmpty()
                 ? tr("未设置") : m_details.selectedDeadlineText())
        .arg(m_details.selectedEstimatedMinutes())
        .arg(m_details.selectedCreatedAtText(), m_details.selectedUpdatedAtText()));
    m_insight->setText(!m_details.selectedBlockingReasonText().isEmpty()
        ? tr("阻塞：%1").arg(m_details.selectedBlockingReasonText())
        : tr("推荐：%1").arg(m_details.selectedReasonText()));
    m_edit->setVisible(m_actionsVisible && m_details.selectedCanEditTask());
    m_editDependencies->setVisible(
        m_actionsVisible && m_details.selectedCanEditDependencies());
}

} // namespace smartmate::view::widgets
