#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QListView;
class QPushButton;

namespace smartmate::viewmodel { class TaskCategoryContract; }

namespace smartmate::view::widgets {

/// 类别目录与单类别草稿的纯 Widgets View；所有命令都使用 Contract 提供的稳定 ID。
class TaskCategoryDialog final : public QDialog {
    Q_OBJECT
public:
    explicit TaskCategoryDialog(viewmodel::TaskCategoryContract &categories,
                                QWidget *parent = nullptr);

    /// 刷新类别目录并显示管理窗口，不隐式清空当前草稿。
    void openManager();

private:
    /// Contract 通知到达后同步草稿字段；QSignalBlocker 防止程序回填形成命令循环。
    void synchronizeDraft();
    /// 根据 Contract 资格启用保存、编辑和删除按钮。
    void synchronizeActions();
    void selectEditingCategory();
    [[nodiscard]] QString selectedCategoryId() const;

    /// 非拥有类别 Contract，同时作为 QListView 的抽象模型。
    viewmodel::TaskCategoryContract &m_categories;
    QListView *m_list;
    QLabel *m_empty;
    QLineEdit *m_name;
    QComboBox *m_color;
    /// 对话框内瞬时通知区域；不作为类别数据源。
    QLabel *m_notification;
    QPushButton *m_edit;
    QPushButton *m_delete;
    QPushButton *m_reset;
    QPushButton *m_save;
};

} // namespace smartmate::view::widgets
