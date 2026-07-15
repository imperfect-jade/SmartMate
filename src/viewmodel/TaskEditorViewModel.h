#pragma once

#include "domain/Task.h"
#include "TaskProjectionSources.h"
#include "viewmodel/contracts/TaskEditorContract.h"

#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QTimeZone>
#include <QVariantList>

#include <optional>

namespace smartmate::model {
class TaskService;
}

namespace smartmate::viewmodel {

/// 编辑器维护与领域实体隔离的输入草稿，并投影新建任务的前置候选。
///
/// 表单字段与候选勾选都只存在于 ViewModel；只有 save() 成功调用 Service 后
/// 才会改变 Model，因此取消任一弹窗都不会污染已经保存的任务或依赖关系。
class TaskEditorViewModel final : public TaskEditorContract {
    Q_OBJECT
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
public:
    TaskEditorViewModel(model::TaskService &taskService,
                        TaskCategoryProjectionSource &categorySource,
                        QObject *parent = nullptr);
    /// 注入时区供测试和确定性显示使用；生产环境默认使用系统时区。
    TaskEditorViewModel(model::TaskService &taskService,
                        TaskCategoryProjectionSource &categorySource,
                        QTimeZone timeZone,
                        QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QString taskId() const override;
    [[nodiscard]] bool editMode() const noexcept override;
    [[nodiscard]] bool sessionActive() const noexcept override;
    [[nodiscard]] QString title() const override;
    void setTitle(const QString &title) override;
    [[nodiscard]] QString description() const override;
    void setDescription(const QString &description) override;
    /// 状态是领域状态机的只读投影；编辑器不得直接修改任务状态。
    [[nodiscard]] QString currentStatusText() const override;
    [[nodiscard]] int priorityIndex() const noexcept override;
    void setPriorityIndex(int priorityIndex) override;
    [[nodiscard]] bool hasDeadline() const noexcept override;
    [[nodiscard]] QString deadlineDisplayText() const override;
    [[nodiscard]] int deadlineYear() const override;
    [[nodiscard]] int deadlineMonth() const override;
    [[nodiscard]] int deadlineDay() const override;
    [[nodiscard]] int deadlineHour() const override;
    [[nodiscard]] int deadlineMinute() const override;
    [[nodiscard]] bool hasEstimatedDuration() const noexcept override;
    [[nodiscard]] QString estimatedDurationDisplayText() const override;
    [[nodiscard]] int estimatedDays() const noexcept override;
    [[nodiscard]] int estimatedHours() const noexcept override;
    [[nodiscard]] int estimatedMinutePart() const noexcept override;
    [[nodiscard]] int minimumEstimatedMinutes() const noexcept override;
    [[nodiscard]] int maximumEstimatedMinutes() const noexcept override;

    [[nodiscard]] QStringList priorityOptions() const override;
    [[nodiscard]] QVariantList categoryOptions() const override;
    [[nodiscard]] QString selectedCategoryId() const override;
    void setSelectedCategoryId(const QString &categoryId) override;
    [[nodiscard]] QString selectedCategoryName() const override;
    [[nodiscard]] QString selectedCategoryAccent() const override;
    [[nodiscard]] bool hasCategory() const noexcept override;
    [[nodiscard]] bool dirty() const noexcept override;
    [[nodiscard]] bool canSave() const noexcept override;
    [[nodiscard]] QString validationMessage() const override;
    [[nodiscard]] QString errorMessage() const;
    [[nodiscard]] int predecessorCandidateCount() const noexcept override;
    [[nodiscard]] int selectedPredecessorCount() const noexcept override;
    [[nodiscard]] QString predecessorSummaryText() const override;
    [[nodiscard]] bool canConfigurePredecessors() const noexcept override;

    /// 读取活动候选并进入新建模式；读取失败时保持原草稿并返回 false。
    bool beginCreate() override;
    /// 根据稳定 TaskId 载入独立草稿；任务不存在或读取失败时返回 false。
    bool beginEdit(const QString &taskId) override;
    /// 以注入时区组合本地日期时间；DST 缺口、重叠或非法日期不会修改草稿。
    bool setDeadlineSelection(int year, int month, int day,
                              int hour, int minute) override;
    void clearDeadline() override;
    /// 将天、小时、分钟换算为领域使用的总分钟；非法分量不会修改草稿。
    bool setEstimatedDuration(int days, int hours, int minutes) override;
    void clearEstimatedDuration() override;
    /// 以当前已接受选择建立弹窗检查点，后续勾选仍只修改本地工作副本。
    void beginPredecessorSelection() override;
    /// 按稳定 TaskId 修改弹窗工作副本，不接受经过重排后不稳定的列表行号。
    bool setCreationPredecessorSelected(const QString &taskId,
                                        bool selected) override;
    /// 将弹窗工作副本合并进主创建草稿。
    void acceptPredecessorSelection() override;
    /// 放弃本次弹窗内勾选并恢复打开前的选择。
    void cancelPredecessorSelection() override;
    void clearCreationPredecessors() override;
    /// 将有效且已修改的草稿交给 Service；只有持久化成功才返回 true。
    bool save() override;
    /// 放弃保存当前草稿并通知 View 关闭编辑流程。
    void cancel() override;

signals:
    void errorMessageChanged();

private:
    /// 记录打开编辑器时的草稿，仅用于计算 dirty/canSave，不是第二份领域模型。
    struct Snapshot {
        QString title;
        QString description;
        int priorityIndex{-1};
        std::optional<QDateTime> deadline;
        std::optional<int> estimatedMinutes;
        std::optional<model::TaskCategoryId> categoryId;
        QSet<model::TaskId> predecessorIds;

        bool operator==(const Snapshot &) const = default;
    };

    /// 构造使用描述表显式选中普通优先级的空白草稿。
    [[nodiscard]] static Snapshot defaultSnapshot();

    /// 捕获当前表单字段与已接受前置，用于 dirty 比较。
    [[nodiscard]] Snapshot currentSnapshot() const;
    /// 原子替换整个草稿并统一发布所有相关 Contract 通知。
    void replaceDraft(const Snapshot &draft,
                      const QString &taskId,
                      bool editMode,
                      model::TaskStatus currentStatus = model::TaskStatus::Todo);
    /// 使用模型重置协议替换创建前置候选。
    void replaceCandidates(QList<model::Task> candidates);
    /// 只通知候选选中 Role，避免因选择变化重建列表。
    void notifyCandidateSelectionChanged();
    /// 将当前草稿记录为已确认检查点，使 dirty 归零。
    void rememberCurrentDraft();
    /// 复用 Model 校验计算 dirty/canSave/validationMessage 并按需通知。
    void updateFormState();
    /// 去重错误属性通知并发布 UiNotification。
    void setErrorMessage(const QString &message);
    /// 切换编辑会话可见状态，不持有或操纵具体 Dialog。
    void setSessionActive(bool active);
    /// 将表单友好字段转换为完整 TaskDraft；校验失败返回空值。
    [[nodiscard]] std::optional<model::TaskDraft> buildTaskDraft();
    /// 将内部截止时间投影到注入时区，供类型化控件显示。
    [[nodiscard]] std::optional<QDateTime> displayedDeadline() const;
    [[nodiscard]] int candidateRow(const model::TaskId &taskId) const;
    /// 刷新类别选项，并安全处理编辑期间类别被外部删除的情况。
    void applyCategories();
    [[nodiscard]] const model::TaskCategory *selectedCategory() const;

    // 非拥有的应用服务引用。
    model::TaskService &m_taskService;
    TaskCategoryProjectionSource &m_categorySource;
    // 当前可编辑草稿采用表单友好形态，便于与 Qt Widgets 显式双向绑定。
    QString m_taskId;
    bool m_editMode{false};
    /// 只表达编辑会话是否打开，不持有或控制任何具体 Dialog。
    bool m_sessionActive{false};
    QString m_title;
    QString m_description;
    /// 仅用于展示当前持久化状态；所有改变都必须通过任务列表的显式状态命令。
    model::TaskStatus m_currentStatus{model::TaskStatus::Todo};
    int m_priorityIndex{-1};
    /// 保存原始精度；只有用户重新选择时才归零到分钟。
    std::optional<QDateTime> m_deadline;
    /// Model 使用的总分钟数，界面仅将其投影为天、小时和分钟。
    std::optional<int> m_estimatedMinutes;
    std::optional<model::TaskCategoryId> m_categoryId;
    /// 解释日历和时钟选择的时区，生产环境为系统时区。
    QTimeZone m_timeZone;
    // 原始快照和由草稿推导出的界面状态。
    Snapshot m_original;
    bool m_dirty{false};
    bool m_canSave{false};
    QString m_validationMessage;
    QString m_errorMessage;
    /// 仅在新建模式加载的活动任务快照；Service 仍负责候选资格的最终校验。
    QList<model::Task> m_predecessorCandidates;
    /// 已由选择弹窗“确定”的前置集合，属于主创建草稿的一部分。
    QSet<model::TaskId> m_selectedCreationPredecessors;
    /// 弹窗内可撤销工作副本；“取消”不会改变主创建草稿。
    QSet<model::TaskId> m_pickerPredecessors;
    bool m_predecessorPickerActive{false};
};

} // namespace smartmate::viewmodel
