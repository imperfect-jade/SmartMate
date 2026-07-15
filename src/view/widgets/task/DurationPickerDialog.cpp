#include "DurationPickerDialog.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

namespace smartmate::view::widgets {
namespace {

QWidget *durationField(const QString &title, QSpinBox &spinBox, QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("durationValueCard"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 14);
    layout->setSpacing(8);
    auto *label = new QLabel(title, card);
    label->setObjectName(QStringLiteral("durationValueLabel"));
    label->setAlignment(Qt::AlignCenter);
    spinBox.setAlignment(Qt::AlignCenter);
    spinBox.setMinimumHeight(46);
    layout->addWidget(label);
    layout->addWidget(&spinBox);
    return card;
}

} // namespace

DurationPickerDialog::DurationPickerDialog(const int minimumMinutes,
                                           const int maximumMinutes,
                                           QWidget *parent)
    : QDialog(parent)
    , m_minimumMinutes(std::max(0, minimumMinutes))
    , m_maximumMinutes(std::max(m_minimumMinutes, maximumMinutes))
    , m_days(new QSpinBox(this))
    , m_hours(new QSpinBox(this))
    , m_minutes(new QSpinBox(this))
    , m_valuesGrid(new QGridLayout)
    , m_daysField(nullptr)
    , m_hoursField(nullptr)
    , m_minutesField(nullptr)
    , m_summary(new QLabel(this))
{
    setObjectName(QStringLiteral("durationPickerDialog"));
    setWindowTitle(tr("选择预计用时"));
    setModal(true);
    resize(520, 400);
    setMinimumSize(400, 360);
    m_days->setObjectName(QStringLiteral("durationDaysSpinBox"));
    m_hours->setObjectName(QStringLiteral("durationHoursSpinBox"));
    m_minutes->setObjectName(QStringLiteral("durationMinutesSpinBox"));
    m_days->setRange(0, m_maximumMinutes / (24 * 60));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto *header = new QFrame(this);
    header->setObjectName(QStringLiteral("pickerHeader"));
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(22, 14, 22, 14);
    headerLayout->setSpacing(3);
    auto *title = new QLabel(tr("选择预计用时"), header);
    title->setObjectName(QStringLiteral("pickerHeaderTitle"));
    auto *subtitle = new QLabel(tr("设置完成任务大约需要的时间"), header);
    subtitle->setObjectName(QStringLiteral("pickerHeaderSubtitle"));
    headerLayout->addWidget(title);
    headerLayout->addWidget(subtitle);
    root->addWidget(header);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("durationPickerScrollView"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("pickerContent"));
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(18, 16, 18, 18);
    contentLayout->setSpacing(14);
    auto *hint = new QLabel(tr("使用数值控件选择天、小时和分钟，范围由任务规则提供。"), content);
    hint->setObjectName(QStringLiteral("pickerHeaderSubtitle"));
    hint->setWordWrap(true);
    contentLayout->addWidget(hint);

    m_valuesGrid->setContentsMargins(0, 0, 0, 0);
    m_valuesGrid->setHorizontalSpacing(10);
    m_valuesGrid->setVerticalSpacing(10);
    m_daysField = durationField(tr("天"), *m_days, content);
    m_hoursField = durationField(tr("小时"), *m_hours, content);
    m_minutesField = durationField(tr("分钟"), *m_minutes, content);
    contentLayout->addLayout(m_valuesGrid);
    m_summary->setObjectName(QStringLiteral("durationSummaryLabel"));
    m_summary->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(m_summary);
    contentLayout->addStretch();
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    auto *footer = new QFrame(this);
    footer->setObjectName(QStringLiteral("pickerFooter"));
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(18, 10, 18, 10);
    footerLayout->addStretch();
    auto *cancel = new QPushButton(tr("取消"), footer);
    cancel->setObjectName(QStringLiteral("cancelDurationSelectionButton"));
    auto *confirm = new QPushButton(tr("确定"), footer);
    confirm->setObjectName(QStringLiteral("confirmDurationSelectionButton"));
    footerLayout->addWidget(cancel);
    footerLayout->addWidget(confirm);
    root->addWidget(footer);

    connect(confirm, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    // 分量变化只联动合法输入范围和摘要，不在 View 中复制任务预计用时业务校验。
    connect(m_days, &QSpinBox::valueChanged, this,
            &DurationPickerDialog::synchronizeRanges);
    connect(m_hours, &QSpinBox::valueChanged, this,
            &DurationPickerDialog::synchronizeRanges);
    connect(m_minutes, &QSpinBox::valueChanged, this,
            &DurationPickerDialog::updateSummary);
    updateResponsiveLayout();
    synchronizeRanges();
}

void DurationPickerDialog::setDuration(const int days, const int hours,
                                       const int minutes)
{
    const int requested = days * 24 * 60 + hours * 60 + minutes;
    const int total = std::clamp(requested, m_minimumMinutes, m_maximumMinutes);
    // 程序性初始化多个分量时阻断中间通知，避免摘要观察到不完整组合。
    const QSignalBlocker daysBlocker(m_days);
    const QSignalBlocker hoursBlocker(m_hours);
    m_days->setValue(total / (24 * 60));
    synchronizeRanges();
    m_hours->setValue((total % (24 * 60)) / 60);
    synchronizeRanges();
    m_minutes->setValue(total % 60);
    updateSummary();
}

int DurationPickerDialog::selectedDays() const { return m_days->value(); }
int DurationPickerDialog::selectedHours() const { return m_hours->value(); }
int DurationPickerDialog::selectedMinutes() const { return m_minutes->value(); }
int DurationPickerDialog::totalMinutes() const
{
    return selectedDays() * 24 * 60 + selectedHours() * 60 + selectedMinutes();
}

void DurationPickerDialog::synchronizeRanges()
{
    // 剩余可选范围由总分钟上下界派生，仅用于保证类型化控件组合可输入。
    const QSignalBlocker hoursBlocker(m_hours);
    const QSignalBlocker minutesBlocker(m_minutes);
    const int dayMinutes = m_days->value() * 24 * 60;
    const int maximumHours = std::clamp(
        (m_maximumMinutes - dayMinutes) / 60, 0, 23);
    m_hours->setRange(0, maximumHours);

    const int baseMinutes = dayMinutes + m_hours->value() * 60;
    const int minimumPart = std::clamp(m_minimumMinutes - baseMinutes, 0, 59);
    const int maximumPart = std::clamp(m_maximumMinutes - baseMinutes, 0, 59);
    m_minutes->setRange(std::min(minimumPart, maximumPart), maximumPart);
    updateSummary();
}

void DurationPickerDialog::updateSummary()
{
    m_summary->setText(tr("合计 %1 分钟").arg(totalMinutes()));
}

void DurationPickerDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    updateResponsiveLayout();
}

void DurationPickerDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    updateResponsiveLayout();
}

void DurationPickerDialog::updateResponsiveLayout()
{
    if (!m_daysField || !m_hoursField || !m_minutesField) return;
    for (QWidget *field : {m_daysField, m_hoursField, m_minutesField}) {
        m_valuesGrid->removeWidget(field);
    }
    if (width() < 470) {
        m_valuesGrid->addWidget(m_daysField, 0, 0);
        m_valuesGrid->addWidget(m_hoursField, 1, 0);
        m_valuesGrid->addWidget(m_minutesField, 2, 0);
    } else {
        m_valuesGrid->addWidget(m_daysField, 0, 0);
        m_valuesGrid->addWidget(m_hoursField, 0, 1);
        m_valuesGrid->addWidget(m_minutesField, 0, 2);
    }
}

} // namespace smartmate::view::widgets
