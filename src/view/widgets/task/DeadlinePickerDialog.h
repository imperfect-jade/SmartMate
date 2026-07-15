#pragma once

#include <QDialog>

class QCalendarWidget;
class QLabel;
class QTimeEdit;

namespace smartmate::view::widgets {

/// 使用日历和时间控件收集截止时间候选值，不解析自由文本。
class DeadlinePickerDialog final : public QDialog {
    Q_OBJECT
public:
    explicit DeadlinePickerDialog(QWidget *parent = nullptr);

    /// 用本地日历字段初始化候选值；对话框不负责 UTC 转换或业务校验。
    void setSelection(int year, int month, int day, int hour, int minute);
    [[nodiscard]] int selectedYear() const;
    [[nodiscard]] int selectedMonth() const;
    [[nodiscard]] int selectedDay() const;
    [[nodiscard]] int selectedHour() const;
    [[nodiscard]] int selectedMinute() const;

private:
    void updateMonthTitle(int year, int month);

    /// 候选日期、时间和月份标题均由对话框父对象拥有。
    QCalendarWidget *m_calendar;
    QTimeEdit *m_time;
    QLabel *m_monthTitle;
};

} // namespace smartmate::view::widgets
