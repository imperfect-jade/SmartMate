#pragma once

#include "common/presentation/UiNotification.h"
#include "view/widgets/MainWindowDependencies.h"

#include <QFont>
#include <QMainWindow>

#include <memory>

class QStackedWidget;
class QFrame;
class QLabel;
class QPushButton;
class QResizeEvent;
class QMoveEvent;
class QShowEvent;
class QHideEvent;
class QCloseEvent;
class QScreen;

namespace smartmate::viewmodel {
class DesktopPetSettingsContract;
}

namespace smartmate::view::widgets::pet {
class AttachedDesktopPetWindow;
class FloatingDesktopPetWindow;
}

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
    ~MainWindow() override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    MainWindow(viewmodel::AppearanceSettingsContract &appearanceSettings,
               viewmodel::DesktopPetSettingsContract *desktopPetSettings,
               QWidget *taskPage, QWidget *graphPage,
               QWidget *focusPage, QWidget *statisticsPage, QWidget *parent);
    void applyAppearance();
    void applyNavigationMode();
    void showNotification(const common::UiNotification &notification);
    void syncDesktopPetVisibility();
    void updateAttachedPetAnchor();
    void openFromDesktopPet();
    void connectDesktopPetScreenSignals(QScreen *screen);
    [[nodiscard]] QScreen *mainWindowScreen() const;

    /// 非拥有外观 Contract；组合根保证其生命周期长于窗口。
    viewmodel::AppearanceSettingsContract &m_appearanceSettings;
    viewmodel::DesktopPetSettingsContract *m_desktopPetSettings;
    /// 启动时基准字体，字号缩放每次从该值重算，避免累计误差。
    QFont m_baselineFont;
    /// 页面栈和导航控件均由 Qt 父子对象树拥有。
    QStackedWidget *m_pages;
    QFrame *m_navigation;
    QLabel *m_brand;
    QPushButton *m_taskNavigation;
    QPushButton *m_graphNavigation;
    QPushButton *m_focusNavigation;
    QPushButton *m_statisticsNavigation;
    QPushButton *m_settingsNavigation;
    std::unique_ptr<pet::AttachedDesktopPetWindow> m_attachedPet;
    std::unique_ptr<pet::FloatingDesktopPetWindow> m_floatingPet;
    Qt::WindowStates m_restoreWindowState{Qt::WindowNoState};
    QString m_lastMainScreenName;
    bool m_closing{false};
};

} // namespace smartmate::view::widgets
