#pragma once

#include "common/presentation/UiNotification.h"
#include "view/widgets/MainWindowDependencies.h"

#include <QFont>
#include <QMainWindow>

class QStackedWidget;
class QFrame;
class QLabel;
class QPushButton;
class QResizeEvent;

namespace smartmate::view::widgets {

/// 纯 Widgets 主窗口：组合页面、应用展示主题并呈现 Contract 通知。
class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(MainWindowDependencies dependencies,
                        QWidget *parent = nullptr);
    /// 仅用于隔离测试设置页与窗口主题；生产组合根始终使用完整依赖构造函数。
    explicit MainWindow(viewmodel::AppearanceSettingsContract &appearanceSettings,
                        QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    MainWindow(viewmodel::AppearanceSettingsContract &appearanceSettings,
               QWidget *taskPage, QWidget *graphPage, QWidget *parent);
    void applyAppearance();
    void applyNavigationMode();
    void showNotification(const common::UiNotification &notification);

    /// 非拥有外观 Contract；组合根保证其生命周期长于窗口。
    viewmodel::AppearanceSettingsContract &m_appearanceSettings;
    /// 启动时基准字体，字号缩放每次从该值重算，避免累计误差。
    QFont m_baselineFont;
    /// 页面栈和导航控件均由 Qt 父子对象树拥有。
    QStackedWidget *m_pages;
    QFrame *m_navigation;
    QLabel *m_brand;
    QPushButton *m_taskNavigation;
    QPushButton *m_graphNavigation;
    QPushButton *m_settingsNavigation;
};

} // namespace smartmate::view::widgets
