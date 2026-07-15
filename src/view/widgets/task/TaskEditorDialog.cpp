#include "TaskEditorDialog.h"

#include "DeadlinePickerDialog.h"
#include "DurationPickerDialog.h"
#include "TaskCreationPredecessorDialog.h"

#include "viewmodel/contracts/TaskEditorContract.h"

#include <QComboBox>
#include <QDate>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTime>
#include <QVBoxLayout>

#include <algorithm>

namespace smartmate::view::widgets {
namespace {

QLabel *fieldLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("taskEditorFieldLabel"));
    return label;
}

QLabel *sectionTitle(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("taskEditorSectionTitle"));
    return label;
}

QFrame *sectionCard(QWidget *parent, QVBoxLayout *&layout)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("taskEditorSectionCard"));
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 15, 16, 16);
    layout->setSpacing(9);
    return card;
}

QFrame *readOnlyField(QLabel &value, QWidget *parent)
{
    auto *frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("taskEditorReadOnlyField"));
    frame->setMinimumHeight(42);
    frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(12, 7, 12, 7);
    layout->addWidget(&value, 1);
    return frame;
}

QWidget *fieldBlock(const QString &labelText, QWidget *control, QWidget *parent)
{
    auto *block = new QWidget(parent);
    auto *layout = new QVBoxLayout(block);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(fieldLabel(labelText, block));
    layout->addWidget(control);
    return block;
}

} // namespace

TaskEditorDialog::TaskEditorDialog(viewmodel::TaskEditorContract &editor,
                                   QWidget *parent)
    : QDialog(parent)
    , m_editor(editor)
    , m_headerTitle(new QLabel(this))
    , m_headerSubtitle(new QLabel(this))
    , m_scroll(new QScrollArea(this))
    , m_planningGrid(new QGridLayout)
    , m_statusField(nullptr)
    , m_priorityField(nullptr)
    , m_categoryField(nullptr)
    , m_title(new QLineEdit(this))
    , m_description(new QPlainTextEdit(this))
    , m_status(new QLabel(this))
    , m_priority(new QComboBox(this))
    , m_category(new QComboBox(this))
    , m_predecessorField(nullptr)
    , m_predecessors(new QLabel(this))
    , m_choosePredecessors(new QPushButton(this))
    , m_clearPredecessors(new QPushButton(QStringLiteral("×"), this))
    , m_deadline(new QLabel(this))
    , m_deadlineClear(new QPushButton(QStringLiteral("×"), this))
    , m_duration(new QLabel(this))
    , m_durationClear(new QPushButton(QStringLiteral("×"), this))
    , m_validation(new QLabel(this))
    , m_save(new QPushButton(tr("保存"), this))
    , m_predecessorDialog(new TaskCreationPredecessorDialog(m_editor, this))
{
    setObjectName(QStringLiteral("taskEditorDialog"));
    setWindowModality(Qt::WindowModal);
    setModal(true);
    resize(700, 680);
    setMinimumSize(480, 480);

    m_headerTitle->setObjectName(QStringLiteral("taskEditorHeaderTitle"));
    m_headerSubtitle->setObjectName(QStringLiteral("taskEditorHeaderSubtitle"));
    m_headerSubtitle->setWordWrap(true);
    m_scroll->setObjectName(QStringLiteral("taskEditorScrollView"));
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_title->setObjectName(QStringLiteral("taskTitleField"));
    m_title->setPlaceholderText(tr("例如：完成 MVVM 架构图"));
    m_description->setObjectName(QStringLiteral("taskDescriptionArea"));
    m_description->setPlaceholderText(tr("可选：记录任务的具体内容"));
    m_status->setObjectName(QStringLiteral("taskCurrentStatusLabel"));
    m_priority->setObjectName(QStringLiteral("taskPriorityComboBox"));
    m_category->setObjectName(QStringLiteral("taskCategoryComboBox"));
    m_choosePredecessors->setObjectName(QStringLiteral("openCreationPredecessorButton"));
    m_clearPredecessors->setObjectName(QStringLiteral("clearCreationPredecessorButton"));
    m_deadlineClear->setObjectName(QStringLiteral("clearDeadlineButton"));
    m_durationClear->setObjectName(QStringLiteral("clearDurationButton"));
    m_validation->setObjectName(QStringLiteral("taskEditorValidation"));
    m_save->setObjectName(QStringLiteral("saveTaskButton"));
    m_description->setFixedHeight(116);
    m_validation->setWordWrap(true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *header = new QFrame(this);
    header->setObjectName(QStringLiteral("taskEditorHeader"));
    header->setMinimumHeight(66);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(22, 12, 22, 12);
    headerLayout->setSpacing(16);
    headerLayout->addWidget(m_headerTitle);
    headerLayout->addStretch();
    m_headerSubtitle->setMaximumWidth(260);
    m_headerSubtitle->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    headerLayout->addWidget(m_headerSubtitle);
    root->addWidget(header);

    auto *content = new QWidget(m_scroll);
    content->setObjectName(QStringLiteral("taskEditorContent"));
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(18, 16, 18, 18);
    contentLayout->setSpacing(14);

    QVBoxLayout *basicLayout = nullptr;
    auto *basicCard = sectionCard(content, basicLayout);
    basicCard->setObjectName(QStringLiteral("taskEditorBasicSection"));
    basicLayout->addWidget(sectionTitle(tr("基本信息"), basicCard));
    basicLayout->addWidget(fieldLabel(tr("标题"), basicCard));
    basicLayout->addWidget(m_title);
    basicLayout->addWidget(fieldLabel(tr("描述"), basicCard));
    basicLayout->addWidget(m_description);
    contentLayout->addWidget(basicCard);

    QVBoxLayout *planningLayout = nullptr;
    auto *planningCard = sectionCard(content, planningLayout);
    planningCard->setObjectName(QStringLiteral("taskEditorPlanningSection"));
    planningLayout->addWidget(sectionTitle(tr("任务规划"), planningCard));
    m_planningGrid->setContentsMargins(0, 0, 0, 0);
    m_planningGrid->setHorizontalSpacing(14);
    m_planningGrid->setVerticalSpacing(10);
    auto *statusDisplay = readOnlyField(*m_status, planningCard);
    m_statusField = fieldBlock(tr("状态"), statusDisplay, planningCard);
    m_priorityField = fieldBlock(tr("优先级"), m_priority, planningCard);
    m_statusField->setObjectName(QStringLiteral("taskEditorStatusField"));
    m_priorityField->setObjectName(QStringLiteral("taskEditorPriorityField"));

    auto *categoryRow = new QWidget(planningCard);
    auto *categoryLayout = new QHBoxLayout(categoryRow);
    categoryLayout->setContentsMargins(0, 0, 0, 0);
    categoryLayout->setSpacing(8);
    categoryLayout->addWidget(m_category, 1);
    auto *manageCategories = new QPushButton(tr("管理类别"), categoryRow);
    manageCategories->setObjectName(QStringLiteral("manageCategoriesFromEditorButton"));
    categoryLayout->addWidget(manageCategories);
    m_categoryField = fieldBlock(tr("类别"), categoryRow, planningCard);
    m_categoryField->setObjectName(QStringLiteral("taskEditorCategoryField"));
    planningLayout->addLayout(m_planningGrid);
    contentLayout->addWidget(planningCard);

    QVBoxLayout *scheduleLayout = nullptr;
    auto *scheduleCard = sectionCard(content, scheduleLayout);
    scheduleCard->setObjectName(QStringLiteral("taskEditorScheduleSection"));
    scheduleLayout->addWidget(sectionTitle(tr("时间与依赖"), scheduleCard));

    m_predecessorField = new QWidget(scheduleCard);
    auto *predecessorFieldLayout = new QVBoxLayout(m_predecessorField);
    predecessorFieldLayout->setContentsMargins(0, 0, 0, 0);
    predecessorFieldLayout->setSpacing(6);
    predecessorFieldLayout->addWidget(fieldLabel(tr("前置任务"), m_predecessorField));
    auto *predecessorRow = new QWidget(m_predecessorField);
    auto *predecessorLayout = new QHBoxLayout(predecessorRow);
    predecessorLayout->setContentsMargins(0, 0, 0, 0);
    predecessorLayout->setSpacing(8);
    predecessorLayout->addWidget(readOnlyField(*m_predecessors, predecessorRow), 1);
    predecessorLayout->addWidget(m_choosePredecessors);
    predecessorLayout->addWidget(m_clearPredecessors);
    predecessorFieldLayout->addWidget(predecessorRow);
    scheduleLayout->addWidget(m_predecessorField);

    auto *deadlineField = new QWidget(scheduleCard);
    auto *deadlineFieldLayout = new QVBoxLayout(deadlineField);
    deadlineFieldLayout->setContentsMargins(0, 0, 0, 0);
    deadlineFieldLayout->setSpacing(6);
    deadlineFieldLayout->addWidget(fieldLabel(tr("截止时间"), deadlineField));
    auto *deadlineRow = new QWidget(deadlineField);
    auto *deadlineLayout = new QHBoxLayout(deadlineRow);
    deadlineLayout->setContentsMargins(0, 0, 0, 0);
    deadlineLayout->setSpacing(8);
    deadlineLayout->addWidget(readOnlyField(*m_deadline, deadlineRow), 1);
    auto *deadlineChoose = new QPushButton(tr("选择"), deadlineRow);
    deadlineChoose->setObjectName(QStringLiteral("openDeadlinePickerButton"));
    deadlineLayout->addWidget(deadlineChoose);
    deadlineLayout->addWidget(m_deadlineClear);
    deadlineFieldLayout->addWidget(deadlineRow);
    scheduleLayout->addWidget(deadlineField);

    auto *durationField = new QWidget(scheduleCard);
    auto *durationFieldLayout = new QVBoxLayout(durationField);
    durationFieldLayout->setContentsMargins(0, 0, 0, 0);
    durationFieldLayout->setSpacing(6);
    durationFieldLayout->addWidget(fieldLabel(tr("预计用时"), durationField));
    auto *durationRow = new QWidget(durationField);
    auto *durationLayout = new QHBoxLayout(durationRow);
    durationLayout->setContentsMargins(0, 0, 0, 0);
    durationLayout->setSpacing(8);
    durationLayout->addWidget(readOnlyField(*m_duration, durationRow), 1);
    auto *durationChoose = new QPushButton(tr("选择"), durationRow);
    durationChoose->setObjectName(QStringLiteral("openDurationPickerButton"));
    durationLayout->addWidget(durationChoose);
    durationLayout->addWidget(m_durationClear);
    durationFieldLayout->addWidget(durationRow);
    scheduleLayout->addWidget(durationField);
    contentLayout->addWidget(scheduleCard);
    contentLayout->addWidget(m_validation);
    contentLayout->addStretch();
    m_scroll->setWidget(content);
    root->addWidget(m_scroll, 1);

    auto *footer = new QFrame(this);
    footer->setObjectName(QStringLiteral("taskEditorFooter"));
    footer->setMinimumHeight(64);
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(20, 10, 20, 10);
    footerLayout->addStretch();
    auto *cancel = new QPushButton(tr("取消"), footer);
    cancel->setObjectName(QStringLiteral("cancelTaskEditorButton"));
    footerLayout->addWidget(cancel);
    footerLayout->addWidget(m_save);
    root->addWidget(footer);

    updateResponsiveLayout();

    // Widget→Contract：用户编辑事件转发为强类型草稿命令；不在 View 内校验业务规则。
    connect(m_title, &QLineEdit::textEdited, &m_editor,
            &viewmodel::TaskEditorContract::setTitle);
    connect(m_description, &QPlainTextEdit::textChanged, this, [this] {
        m_editor.setDescription(m_description->toPlainText());
    });
    connect(m_priority, &QComboBox::activated, &m_editor,
            &viewmodel::TaskEditorContract::setPriorityIndex);
    connect(m_category, &QComboBox::activated, this, [this](const int index) {
        m_editor.setSelectedCategoryId(m_category->itemData(index).toString());
    });
    connect(manageCategories, &QPushButton::clicked, this,
            &TaskEditorDialog::manageCategoriesRequested);
    connect(m_choosePredecessors, &QPushButton::clicked, m_predecessorDialog,
            &TaskCreationPredecessorDialog::openSelection);
    connect(m_clearPredecessors, &QPushButton::clicked, &m_editor,
            &viewmodel::TaskEditorContract::clearCreationPredecessors);
    connect(deadlineChoose, &QPushButton::clicked, this, &TaskEditorDialog::chooseDeadline);
    connect(m_deadlineClear, &QPushButton::clicked, &m_editor,
            &viewmodel::TaskEditorContract::clearDeadline);
    connect(durationChoose, &QPushButton::clicked, this, &TaskEditorDialog::chooseDuration);
    connect(m_durationClear, &QPushButton::clicked, &m_editor,
            &viewmodel::TaskEditorContract::clearEstimatedDuration);
    connect(cancel, &QPushButton::clicked, this, &TaskEditorDialog::reject);
    // 保存只调用一次 Contract 命令；成功后 sessionActiveChanged 驱动窗口关闭。
    connect(m_save, &QPushButton::clicked, &m_editor,
            &viewmodel::TaskEditorContract::save);

    // Contract→Widget：任一草稿通知到达后重读完整一致快照，避免字段间显示不同步。
    const auto sync = [this] { synchronize(); };
    connect(&m_editor, &viewmodel::TaskEditorContract::modeChanged, this, sync);
    connect(&m_editor, &viewmodel::TaskEditorContract::titleChanged, this, sync);
    connect(&m_editor, &viewmodel::TaskEditorContract::descriptionChanged, this, sync);
    connect(&m_editor, &viewmodel::TaskEditorContract::currentStatusTextChanged, this, sync);
    connect(&m_editor, &viewmodel::TaskEditorContract::priorityIndexChanged, this, sync);
    connect(&m_editor, &viewmodel::TaskEditorContract::deadlineChanged, this, sync);
    connect(&m_editor, &viewmodel::TaskEditorContract::estimatedDurationChanged, this, sync);
    connect(&m_editor, &viewmodel::TaskEditorContract::categoryOptionsChanged, this, sync);
    connect(&m_editor, &viewmodel::TaskEditorContract::categoryChanged, this, sync);
    connect(&m_editor, &viewmodel::TaskEditorContract::formStateChanged, this, sync);
    connect(&m_editor, &viewmodel::TaskEditorContract::predecessorCandidatesChanged,
            this, sync);
    connect(&m_editor, &viewmodel::TaskEditorContract::predecessorSelectionChanged,
            this, sync);
    connect(&m_editor, &viewmodel::TaskEditorContract::sessionActiveChanged,
            this, &TaskEditorDialog::synchronizeSession);
    synchronize();
    synchronizeSession();
}

void TaskEditorDialog::reject()
{
    // 先回滚嵌套前置选择，再取消主编辑会话，保持检查点层级顺序。
    if (m_predecessorDialog->isVisible()) m_predecessorDialog->reject();
    if (m_editor.sessionActive()) m_editor.cancel();
    QDialog::reject();
}

void TaskEditorDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    updateResponsiveLayout();
}

void TaskEditorDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    updateResponsiveLayout();
}

void TaskEditorDialog::updateResponsiveLayout()
{
    if (!m_statusField || !m_priorityField || !m_categoryField) return;
    const bool narrow = width() < 610;
    for (QWidget *field : {m_statusField, m_priorityField, m_categoryField}) {
        m_planningGrid->removeWidget(field);
    }
    if (narrow) {
        m_planningGrid->addWidget(m_statusField, 0, 0);
        m_planningGrid->addWidget(m_priorityField, 1, 0);
        m_planningGrid->addWidget(m_categoryField, 2, 0);
        m_planningGrid->setColumnStretch(0, 1);
        m_planningGrid->setColumnStretch(1, 0);
    } else {
        m_planningGrid->addWidget(m_statusField, 0, 0);
        m_planningGrid->addWidget(m_priorityField, 0, 1);
        m_planningGrid->addWidget(m_categoryField, 1, 0, 1, 2);
        m_planningGrid->setColumnStretch(0, 1);
        m_planningGrid->setColumnStretch(1, 1);
    }
}

void TaskEditorDialog::synchronizeSession()
{
    // 窗口可见性是 Contract 会话状态的展示结果；View 不自行判断保存是否成功。
    if (m_editor.sessionActive()) {
        const bool editing = m_editor.editMode();
        const QString title = editing ? tr("编辑任务") : tr("新建任务");
        setWindowTitle(title);
        m_headerTitle->setText(title);
        m_headerSubtitle->setText(editing
            ? tr("修改任务信息") : tr("创建后状态固定为待办"));
        if (parentWidget()) {
            const int availableWidth = std::max(minimumWidth(), parentWidget()->width() - 48);
            const int availableHeight = std::max(minimumHeight(), parentWidget()->height() - 48);
            resize(std::min(700, availableWidth), std::min(680, availableHeight));
        }
        if (!isVisible()) open();
    } else if (isVisible()) {
        QDialog::accept();
    }
}

void TaskEditorDialog::synchronize()
{
    // 程序性回填不得再次触发 Widget→Contract 命令，防止双向绑定回写循环。
    const QSignalBlocker titleBlocker(m_title);
    const QSignalBlocker descriptionBlocker(m_description);
    const QSignalBlocker priorityBlocker(m_priority);
    const QSignalBlocker categoryBlocker(m_category);
    // 用户输入会先写入 Contract，再通过通知回到这里。值未变化时必须保持
    // 编辑控件原状，否则 setPlainText() 会把光标移到开头，连续输入将倒序。
    if (m_title->text() != m_editor.title()) {
        m_title->setText(m_editor.title());
    }
    if (m_description->toPlainText() != m_editor.description()) {
        m_description->setPlainText(m_editor.description());
    }
    m_status->setText(m_editor.editMode() ? m_editor.currentStatusText() : tr("初始状态：待办"));
    if (m_priority->count() != m_editor.priorityOptions().size()) {
        m_priority->clear();
        m_priority->addItems(m_editor.priorityOptions());
    }
    m_priority->setCurrentIndex(m_editor.priorityIndex());
    m_category->clear();
    const QVariantList categories = m_editor.categoryOptions();
    for (const QVariant &value : categories) {
        const QVariantMap map = value.toMap();
        m_category->addItem(map.value(QStringLiteral("name")).toString(),
                            map.value(QStringLiteral("categoryId")).toString());
    }
    const int categoryIndex = m_category->findData(m_editor.selectedCategoryId());
    m_category->setCurrentIndex(categoryIndex < 0 ? 0 : categoryIndex);
    m_predecessors->setText(m_editor.predecessorSummaryText());
    m_choosePredecessors->setText(m_editor.selectedPredecessorCount() > 0
        ? tr("修改") : tr("选择"));
    m_predecessorField->setVisible(m_editor.canConfigurePredecessors());
    m_choosePredecessors->setVisible(m_editor.canConfigurePredecessors());
    m_choosePredecessors->setEnabled(m_editor.predecessorCandidateCount() > 0);
    m_clearPredecessors->setVisible(m_editor.canConfigurePredecessors()
                                    && m_editor.selectedPredecessorCount() > 0);
    m_deadline->setText(m_editor.deadlineDisplayText());
    m_deadlineClear->setVisible(m_editor.hasDeadline());
    m_duration->setText(m_editor.estimatedDurationDisplayText());
    m_durationClear->setVisible(m_editor.hasEstimatedDuration());
    m_validation->setText(m_editor.validationMessage());
    m_validation->setVisible(!m_editor.validationMessage().isEmpty());
    m_save->setEnabled(m_editor.canSave());

    const auto setProjectedValueColor = [this](QLabel &label, const bool hasValue) {
        QPalette valuePalette = label.palette();
        valuePalette.setColor(QPalette::WindowText,
            palette().color(hasValue ? QPalette::Text : QPalette::PlaceholderText));
        label.setPalette(valuePalette);
    };
    setProjectedValueColor(*m_predecessors, m_editor.selectedPredecessorCount() > 0);
    setProjectedValueColor(*m_deadline, m_editor.hasDeadline());
    setProjectedValueColor(*m_duration, m_editor.hasEstimatedDuration());
}

void TaskEditorDialog::chooseDeadline()
{
    // 对话框只收集类型化本地日期字段；确认后一次交给 Contract 转换和校验。
    DeadlinePickerDialog picker(this);
    if (m_editor.hasDeadline()) {
        picker.setSelection(m_editor.deadlineYear(), m_editor.deadlineMonth(),
                            m_editor.deadlineDay(), m_editor.deadlineHour(),
                            m_editor.deadlineMinute());
    } else {
        const QDate tomorrow = QDate::currentDate().addDays(1);
        const QTime now = QTime::currentTime();
        picker.setSelection(tomorrow.year(), tomorrow.month(), tomorrow.day(),
                            now.hour(), now.minute());
    }
    if (picker.exec() == QDialog::Accepted) {
        m_editor.setDeadlineSelection(
            picker.selectedYear(), picker.selectedMonth(), picker.selectedDay(),
            picker.selectedHour(), picker.selectedMinute());
    }
}

void TaskEditorDialog::chooseDuration()
{
    // 合法分钟边界来自 Contract；View 仅把总时长拆成日/时/分控件。
    DurationPickerDialog picker(m_editor.minimumEstimatedMinutes(),
                                m_editor.maximumEstimatedMinutes(), this);
    picker.setDuration(m_editor.hasEstimatedDuration() ? m_editor.estimatedDays() : 0,
                       m_editor.hasEstimatedDuration() ? m_editor.estimatedHours() : 0,
                       m_editor.hasEstimatedDuration() ? m_editor.estimatedMinutePart() : 30);
    if (picker.exec() == QDialog::Accepted) {
        m_editor.setEstimatedDuration(picker.selectedDays(), picker.selectedHours(),
                                      picker.selectedMinutes());
    }
}

} // namespace smartmate::view::widgets
