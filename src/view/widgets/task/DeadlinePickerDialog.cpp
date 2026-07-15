#include "DeadlinePickerDialog.h"

#include <QCalendarWidget>
#include <QDate>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTime>
#include <QTimeEdit>
#include <QVBoxLayout>

namespace smartmate::view::widgets {

DeadlinePickerDialog::DeadlinePickerDialog(QWidget *parent)
    : QDialog(parent)
    , m_calendar(new QCalendarWidget(this))
    , m_time(new QTimeEdit(this))
    , m_monthTitle(new QLabel(this))
{
    setObjectName(QStringLiteral("deadlinePickerDialog"));
    setWindowTitle(tr("选择截止时间"));
    setModal(true);
    resize(520, 620);
    setMinimumSize(400, 480);
    m_calendar->setObjectName(QStringLiteral("deadlineCalendar"));
    m_calendar->setNavigationBarVisible(false);
    m_calendar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_time->setObjectName(QStringLiteral("deadlineTimeEdit"));
    m_time->setDisplayFormat(QStringLiteral("HH:mm"));
    m_time->setAlignment(Qt::AlignCenter);
    m_time->setMinimumHeight(46);
    m_monthTitle->setObjectName(QStringLiteral("deadlineMonthTitle"));
    m_monthTitle->setAlignment(Qt::AlignCenter);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *header = new QFrame(this);
    header->setObjectName(QStringLiteral("pickerHeader"));
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(22, 14, 22, 14);
    headerLayout->setSpacing(3);
    auto *title = new QLabel(tr("选择截止时间"), header);
    title->setObjectName(QStringLiteral("pickerHeaderTitle"));
    auto *subtitle = new QLabel(tr("设置任务需要完成的日期与具体时间"), header);
    subtitle->setObjectName(QStringLiteral("pickerHeaderSubtitle"));
    subtitle->setWordWrap(true);
    headerLayout->addWidget(title);
    headerLayout->addWidget(subtitle);
    root->addWidget(header);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("deadlinePickerScrollView"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("pickerContent"));
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(18, 16, 18, 18);
    contentLayout->setSpacing(14);

    auto *calendarCard = new QFrame(content);
    calendarCard->setObjectName(QStringLiteral("deadlineCalendarCard"));
    auto *calendarLayout = new QVBoxLayout(calendarCard);
    calendarLayout->setContentsMargins(14, 14, 14, 14);
    calendarLayout->setSpacing(10);
    auto *navigation = new QHBoxLayout;
    navigation->setSpacing(8);
    auto *previous = new QPushButton(QStringLiteral("‹"), calendarCard);
    previous->setObjectName(QStringLiteral("deadlinePreviousMonthButton"));
    previous->setAccessibleName(tr("上一个月"));
    auto *today = new QPushButton(tr("今天"), calendarCard);
    today->setObjectName(QStringLiteral("deadlineTodayButton"));
    auto *next = new QPushButton(QStringLiteral("›"), calendarCard);
    next->setObjectName(QStringLiteral("deadlineNextMonthButton"));
    next->setAccessibleName(tr("下一个月"));
    navigation->addWidget(previous);
    navigation->addWidget(m_monthTitle, 1);
    navigation->addWidget(today);
    navigation->addWidget(next);
    calendarLayout->addLayout(navigation);
    calendarLayout->addWidget(m_calendar, 1);
    contentLayout->addWidget(calendarCard, 1);

    auto *timeCard = new QFrame(content);
    timeCard->setObjectName(QStringLiteral("deadlineTimeCard"));
    auto *timeLayout = new QVBoxLayout(timeCard);
    timeLayout->setContentsMargins(16, 14, 16, 16);
    timeLayout->setSpacing(8);
    auto *timeTitle = new QLabel(tr("时间（24 小时制）"), timeCard);
    timeTitle->setObjectName(QStringLiteral("pickerSectionTitle"));
    timeLayout->addWidget(timeTitle);
    timeLayout->addWidget(m_time);
    contentLayout->addWidget(timeCard);
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    auto *footer = new QFrame(this);
    footer->setObjectName(QStringLiteral("pickerFooter"));
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(18, 10, 18, 10);
    footerLayout->addStretch();
    auto *cancel = new QPushButton(tr("取消"), footer);
    cancel->setObjectName(QStringLiteral("cancelDeadlineSelectionButton"));
    auto *confirm = new QPushButton(tr("确定"), footer);
    confirm->setObjectName(QStringLiteral("confirmDeadlineSelectionButton"));
    footerLayout->addWidget(cancel);
    footerLayout->addWidget(confirm);
    root->addWidget(footer);

    connect(previous, &QPushButton::clicked, m_calendar, &QCalendarWidget::showPreviousMonth);
    connect(next, &QPushButton::clicked, m_calendar, &QCalendarWidget::showNextMonth);
    connect(today, &QPushButton::clicked, this, [this] {
        m_calendar->setSelectedDate(QDate::currentDate());
        m_calendar->showToday();
    });
    connect(m_calendar, &QCalendarWidget::currentPageChanged,
            this, &DeadlinePickerDialog::updateMonthTitle);
    // accept/reject 只提交对话框结果；调用方在 accept 后才把类型化字段写入 Contract。
    connect(confirm, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    updateMonthTitle(m_calendar->yearShown(), m_calendar->monthShown());
}

void DeadlinePickerDialog::setSelection(const int year, const int month,
                                        const int day, const int hour,
                                        const int minute)
{
    // 非法外部字段保持控件原值，最终业务范围仍由 TaskEditorContract/Model 校验。
    const QDate date{year, month, day};
    const QTime time{hour, minute};
    if (date.isValid()) m_calendar->setSelectedDate(date);
    if (time.isValid()) m_time->setTime(time);
}

int DeadlinePickerDialog::selectedYear() const { return m_calendar->selectedDate().year(); }
int DeadlinePickerDialog::selectedMonth() const { return m_calendar->selectedDate().month(); }
int DeadlinePickerDialog::selectedDay() const { return m_calendar->selectedDate().day(); }
int DeadlinePickerDialog::selectedHour() const { return m_time->time().hour(); }
int DeadlinePickerDialog::selectedMinute() const { return m_time->time().minute(); }

void DeadlinePickerDialog::updateMonthTitle(const int year, const int month)
{
    m_monthTitle->setText(tr("%1 年 %2 月").arg(year).arg(month));
}

} // namespace smartmate::view::widgets
