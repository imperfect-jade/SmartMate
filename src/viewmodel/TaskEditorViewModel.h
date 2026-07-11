#pragma once

#include "domain/Task.h"

#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QTimeZone>
#include <QtQmlIntegration/qqmlintegration.h>

#include <optional>

namespace smartmate::model {
class TaskService;
}

namespace smartmate::viewmodel {

/// 编辑器维护与领域实体隔离的输入草稿。只有 save() 成功调用 Service 后才会
/// 改变 Model，因此用户取消编辑不会污染已经保存的任务。
class TaskEditorViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString taskId READ taskId NOTIFY modeChanged)
    Q_PROPERTY(bool editMode READ editMode NOTIFY modeChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QString description READ description WRITE setDescription NOTIFY descriptionChanged)
    Q_PROPERTY(int statusIndex READ statusIndex WRITE setStatusIndex NOTIFY statusIndexChanged)
    Q_PROPERTY(int priorityIndex READ priorityIndex WRITE setPriorityIndex NOTIFY priorityIndexChanged)
    Q_PROPERTY(bool hasDeadline READ hasDeadline NOTIFY deadlineChanged)
    Q_PROPERTY(QString deadlineDisplayText READ deadlineDisplayText NOTIFY deadlineChanged)
    Q_PROPERTY(int deadlineYear READ deadlineYear NOTIFY deadlineChanged)
    Q_PROPERTY(int deadlineMonth READ deadlineMonth NOTIFY deadlineChanged)
    Q_PROPERTY(int deadlineDay READ deadlineDay NOTIFY deadlineChanged)
    Q_PROPERTY(int deadlineHour READ deadlineHour NOTIFY deadlineChanged)
    Q_PROPERTY(int deadlineMinute READ deadlineMinute NOTIFY deadlineChanged)
    Q_PROPERTY(bool hasEstimatedDuration READ hasEstimatedDuration NOTIFY estimatedDurationChanged)
    Q_PROPERTY(QString estimatedDurationDisplayText READ estimatedDurationDisplayText
                   NOTIFY estimatedDurationChanged)
    Q_PROPERTY(int estimatedDays READ estimatedDays NOTIFY estimatedDurationChanged)
    Q_PROPERTY(int estimatedHours READ estimatedHours NOTIFY estimatedDurationChanged)
    Q_PROPERTY(int estimatedMinutePart READ estimatedMinutePart NOTIFY estimatedDurationChanged)
    Q_PROPERTY(int minimumEstimatedMinutes READ minimumEstimatedMinutes CONSTANT)
    Q_PROPERTY(int maximumEstimatedMinutes READ maximumEstimatedMinutes CONSTANT)
    Q_PROPERTY(QStringList statusOptions READ statusOptions CONSTANT)
    Q_PROPERTY(QStringList priorityOptions READ priorityOptions CONSTANT)
    Q_PROPERTY(bool dirty READ dirty NOTIFY formStateChanged)
    Q_PROPERTY(bool canSave READ canSave NOTIFY formStateChanged)
    Q_PROPERTY(QString validationMessage READ validationMessage NOTIFY formStateChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    QML_NAMED_ELEMENT(TaskEditorViewModel)
    QML_UNCREATABLE("TaskEditorViewModel is owned by AppViewModel")

public:
    explicit TaskEditorViewModel(model::TaskService &taskService, QObject *parent = nullptr);
    /// 注入时区供测试和确定性显示使用；生产环境默认使用系统时区。
    TaskEditorViewModel(model::TaskService &taskService,
                        QTimeZone timeZone,
                        QObject *parent = nullptr);

    [[nodiscard]] QString taskId() const;
    [[nodiscard]] bool editMode() const noexcept;
    [[nodiscard]] QString title() const;
    void setTitle(const QString &title);
    [[nodiscard]] QString description() const;
    void setDescription(const QString &description);
    [[nodiscard]] int statusIndex() const noexcept;
    void setStatusIndex(int statusIndex);
    [[nodiscard]] int priorityIndex() const noexcept;
    void setPriorityIndex(int priorityIndex);
    [[nodiscard]] bool hasDeadline() const noexcept;
    [[nodiscard]] QString deadlineDisplayText() const;
    [[nodiscard]] int deadlineYear() const;
    [[nodiscard]] int deadlineMonth() const;
    [[nodiscard]] int deadlineDay() const;
    [[nodiscard]] int deadlineHour() const;
    [[nodiscard]] int deadlineMinute() const;
    [[nodiscard]] bool hasEstimatedDuration() const noexcept;
    [[nodiscard]] QString estimatedDurationDisplayText() const;
    [[nodiscard]] int estimatedDays() const noexcept;
    [[nodiscard]] int estimatedHours() const noexcept;
    [[nodiscard]] int estimatedMinutePart() const noexcept;
    [[nodiscard]] int minimumEstimatedMinutes() const noexcept;
    [[nodiscard]] int maximumEstimatedMinutes() const noexcept;

    [[nodiscard]] QStringList statusOptions() const;
    [[nodiscard]] QStringList priorityOptions() const;
    [[nodiscard]] bool dirty() const noexcept;
    [[nodiscard]] bool canSave() const noexcept;
    [[nodiscard]] QString validationMessage() const;
    [[nodiscard]] QString errorMessage() const;

    /// 清空旧草稿并进入新建模式，不写入 Model。
    Q_INVOKABLE void beginCreate();
    /// 根据稳定 TaskId 载入独立草稿；任务不存在或读取失败时返回 false。
    Q_INVOKABLE bool beginEdit(const QString &taskId);
    /// 以注入时区组合本地日期时间；DST 缺口、重叠或非法日期不会修改草稿。
    Q_INVOKABLE bool setDeadlineSelection(int year, int month, int day,
                                          int hour, int minute);
    Q_INVOKABLE void clearDeadline();
    /// 将天、小时、分钟换算为领域使用的总分钟；非法分量不会修改草稿。
    Q_INVOKABLE bool setEstimatedDuration(int days, int hours, int minutes);
    Q_INVOKABLE void clearEstimatedDuration();
    /// 将有效且已修改的草稿交给 Service；只有持久化成功才返回 true。
    Q_INVOKABLE bool save();
    /// 放弃保存当前草稿并通知 View 关闭编辑流程。
    Q_INVOKABLE void cancel();

signals:
    void modeChanged();
    void titleChanged();
    void descriptionChanged();
    void statusIndexChanged();
    void priorityIndexChanged();
    void deadlineChanged();
    void estimatedDurationChanged();
    void formStateChanged();
    void errorMessageChanged();
    void saved(const QString &taskId);
    void cancelled();

private:
    /// 记录打开编辑器时的草稿，仅用于计算 dirty/canSave，不是第二份领域模型。
    struct Snapshot {
        QString title;
        QString description;
        int statusIndex{0};
        int priorityIndex{1};
        std::optional<QDateTime> deadline;
        std::optional<int> estimatedMinutes;

        bool operator==(const Snapshot &) const = default;
    };

    [[nodiscard]] Snapshot currentSnapshot() const;
    void replaceDraft(const Snapshot &draft, const QString &taskId, bool editMode);
    void rememberCurrentDraft();
    void updateFormState();
    void setErrorMessage(const QString &message);
    [[nodiscard]] std::optional<model::TaskDraft> buildTaskDraft();
    [[nodiscard]] std::optional<QDateTime> displayedDeadline() const;

    // 非拥有的应用服务引用。
    model::TaskService &m_taskService;
    // 当前可编辑草稿采用表单友好形态，便于与 QML 双向绑定。
    QString m_taskId;
    bool m_editMode{false};
    QString m_title;
    QString m_description;
    int m_statusIndex{0};
    int m_priorityIndex{1};
    /// 保存原始精度；只有用户重新选择时才归零到分钟。
    std::optional<QDateTime> m_deadline;
    /// Model 使用的总分钟数，界面仅将其投影为天、小时和分钟。
    std::optional<int> m_estimatedMinutes;
    /// 解释日历和时钟选择的时区，生产环境为系统时区。
    QTimeZone m_timeZone;
    // 原始快照和由草稿推导出的界面状态。
    Snapshot m_original;
    bool m_dirty{false};
    bool m_canSave{false};
    QString m_validationMessage;
    QString m_errorMessage;
};

} // namespace smartmate::viewmodel
