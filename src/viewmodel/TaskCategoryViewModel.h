#pragma once

#include "domain/TaskCategory.h"
#include "TaskProjectionSources.h"
#include "viewmodel/contracts/TaskCategoryContract.h"


namespace smartmate::model {
class TaskCategoryService;
}

namespace smartmate::viewmodel {

/// 投影类别目录并维护单个类别的创建/编辑草稿。
///
/// 名称唯一性、颜色有效性和原子删除均由 TaskCategoryService 判定；本类型
/// 只把结构化结果映射为 Qt Widgets 可观察状态。
class TaskCategoryViewModel final : public TaskCategoryContract {
    Q_OBJECT
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
public:
    /// categoryService 可为空以兼容不加载类别功能的隔离测试；生产组合根必须注入。
    explicit TaskCategoryViewModel(model::TaskCategoryService *categoryService,
                                   TaskPlanProjectionSource &planSource,
                                   TaskCategoryProjectionSource &categorySource,
                                   QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int count() const noexcept override;
    [[nodiscard]] bool empty() const noexcept override;
    [[nodiscard]] bool editMode() const noexcept override;
    [[nodiscard]] QString editingCategoryId() const override;
    [[nodiscard]] QString draftName() const override;
    void setDraftName(const QString &name) override;
    [[nodiscard]] int draftColorIndex() const noexcept override;
    void setDraftColorIndex(int index) override;
    [[nodiscard]] QStringList colorOptions() const override;
    [[nodiscard]] QStringList colorAccents() const override;
    [[nodiscard]] bool dirty() const noexcept override;
    [[nodiscard]] bool canSave() const noexcept override;
    [[nodiscard]] QString errorMessage() const;

    void reload() override;
    void beginCreate() override;
    bool beginEdit(const QString &categoryId) override;
    bool save() override;
    bool deleteCategory(const QString &categoryId) override;
    void cancel() override;
    Q_INVOKABLE void clearError();

signals:
    void errorMessageChanged();

private:
    /// 按稳定 ID 查找当前投影行；不存在返回 -1，绝不把行号当类别身份。
    [[nodiscard]] int rowForCategory(const model::TaskCategoryId &id) const;
    /// 将类别业务错误枚举转换为中文展示文案。
    [[nodiscard]] QString categoryErrorText(int error) const;
    /// 去重错误属性通知，并为非空错误发布 UiNotification。
    void setErrorMessage(const QString &message);
    /// 仅在两个共享源均有效时原子替换类别行与任务计数。
    void applyProjection();

    /// 非拥有指针；nullptr 表示类别命令不可用的兼容模式。
    model::TaskCategoryService *m_categoryService{nullptr};
    TaskPlanProjectionSource &m_planSource;
    TaskCategoryProjectionSource &m_categorySource;
    /// 当前类别列表快照，是 QAbstractListModel Role 的数据源。
    QList<model::TaskCategory> m_categories;
    /// 按稳定类别 ID 缓存派生任务数量，不写回 Model。
    QHash<model::TaskCategoryId, int> m_taskCounts;
    /// 编辑模式目标；创建模式为空 ID。
    model::TaskCategoryId m_editingCategoryId;
    // 当前表单草稿与打开编辑时原值分离，用于计算 dirty/canSave。
    QString m_draftName;
    model::TaskCategoryColor m_draftColor{model::TaskCategoryColor::Blue};
    QString m_originalName;
    model::TaskCategoryColor m_originalColor{model::TaskCategoryColor::Blue};
    bool m_editMode{false};
    /// 最近一次命令错误；空字符串表示无错误。
    QString m_errorMessage;
};

} // namespace smartmate::viewmodel
