#pragma once

#include "domain/TaskCategory.h"

#include <QAbstractListModel>
#include <QStringList>
#include <QtQmlIntegration/qqmlintegration.h>

namespace smartmate::model {
class TaskService;
class TaskCategoryService;
}

namespace smartmate::viewmodel {

/// 投影类别目录并维护单个类别的创建/编辑草稿。
///
/// 名称唯一性、颜色有效性和原子删除均由 TaskCategoryService 判定；本类型
/// 只把结构化结果映射为 QML 可观察状态。
class TaskCategoryViewModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool empty READ empty NOTIFY countChanged)
    Q_PROPERTY(bool editMode READ editMode NOTIFY draftChanged)
    Q_PROPERTY(QString editingCategoryId READ editingCategoryId NOTIFY draftChanged)
    Q_PROPERTY(QString draftName READ draftName WRITE setDraftName NOTIFY draftChanged)
    Q_PROPERTY(int draftColorIndex READ draftColorIndex WRITE setDraftColorIndex
                   NOTIFY draftChanged)
    Q_PROPERTY(QStringList colorOptions READ colorOptions CONSTANT)
    Q_PROPERTY(QStringList colorAccents READ colorAccents CONSTANT)
    Q_PROPERTY(bool dirty READ dirty NOTIFY draftChanged)
    Q_PROPERTY(bool canSave READ canSave NOTIFY draftChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    QML_NAMED_ELEMENT(TaskCategoryViewModel)
    QML_UNCREATABLE("TaskCategoryViewModel is owned by AppViewModel")

public:
    enum Role {
        CategoryIdRole = Qt::UserRole + 1,
        NameRole,
        ColorIndexRole,
        AccentRole,
        TaskCountRole,
    };
    Q_ENUM(Role)

    /// categoryService 可为空以兼容不加载类别功能的隔离测试；生产组合根必须注入。
    explicit TaskCategoryViewModel(model::TaskService &taskService,
                                   model::TaskCategoryService *categoryService = nullptr,
                                   QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int count() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool editMode() const noexcept;
    [[nodiscard]] QString editingCategoryId() const;
    [[nodiscard]] QString draftName() const;
    void setDraftName(const QString &name);
    [[nodiscard]] int draftColorIndex() const noexcept;
    void setDraftColorIndex(int index);
    [[nodiscard]] QStringList colorOptions() const;
    [[nodiscard]] QStringList colorAccents() const;
    [[nodiscard]] bool dirty() const noexcept;
    [[nodiscard]] bool canSave() const noexcept;
    [[nodiscard]] QString errorMessage() const;

    Q_INVOKABLE void reload();
    Q_INVOKABLE void beginCreate();
    Q_INVOKABLE bool beginEdit(const QString &categoryId);
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool deleteCategory(const QString &categoryId);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void clearError();

signals:
    void countChanged();
    void draftChanged();
    void errorMessageChanged();
    void saved(const QString &categoryId);
    void deleted(const QString &categoryId, int unassignedTaskCount);
    void cancelled();

private:
    [[nodiscard]] int rowForCategory(const model::TaskCategoryId &id) const;
    [[nodiscard]] QString categoryErrorText(int error) const;
    void setErrorMessage(const QString &message);

    model::TaskService &m_taskService;
    /// 非拥有指针；nullptr 仅用于旧的隔离测试构造路径。
    model::TaskCategoryService *m_categoryService{nullptr};
    QList<model::TaskCategory> m_categories;
    QHash<model::TaskCategoryId, int> m_taskCounts;
    model::TaskCategoryId m_editingCategoryId;
    QString m_draftName;
    model::TaskCategoryColor m_draftColor{model::TaskCategoryColor::Blue};
    QString m_originalName;
    model::TaskCategoryColor m_originalColor{model::TaskCategoryColor::Blue};
    bool m_editMode{false};
    QString m_errorMessage;
};

} // namespace smartmate::viewmodel
