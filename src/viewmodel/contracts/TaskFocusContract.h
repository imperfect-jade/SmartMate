#pragma once

#include "common/presentation/UiNotification.h"

#include <QObject>
#include <QString>

namespace smartmate::viewmodel {

/// “现在做”区域的抽象展示契约；只提供 Model 计划结果的焦点投影。
///
/// 本 Contract 没有状态写命令：开始/完成仍通过 TaskListContract 等命令端口提交，
/// 从而保持焦点投影与任务命令职责分离。Widget 先读 getter，再监听 focusTaskChanged()。
class TaskFocusContract : public QObject {
    Q_OBJECT
    Q_PROPERTY(FocusState focusState READ focusState NOTIFY focusTaskChanged)
    Q_PROPERTY(QString focusTaskId READ focusTaskId NOTIFY focusTaskChanged)
    Q_PROPERTY(QString focusTitle READ focusTitle NOTIFY focusTaskChanged)
    Q_PROPERTY(QString focusDescription READ focusDescription NOTIFY focusTaskChanged)
    Q_PROPERTY(QString focusStatusText READ focusStatusText NOTIFY focusTaskChanged)
    Q_PROPERTY(QString focusPriorityText READ focusPriorityText NOTIFY focusTaskChanged)
    Q_PROPERTY(QString focusDeadlineText READ focusDeadlineText NOTIFY focusTaskChanged)
    Q_PROPERTY(int focusEstimatedMinutes READ focusEstimatedMinutes NOTIFY focusTaskChanged)
    Q_PROPERTY(QString focusEstimatedText READ focusEstimatedText NOTIFY focusTaskChanged)
    Q_PROPERTY(QString focusReasonText READ focusReasonText NOTIFY focusTaskChanged)
    Q_PROPERTY(bool focusOverdue READ focusOverdue NOTIFY focusTaskChanged)
    Q_PROPERTY(bool focusCanStart READ focusCanStart NOTIFY focusTaskChanged)
    Q_PROPERTY(bool focusCanComplete READ focusCanComplete NOTIFY focusTaskChanged)
    Q_PROPERTY(QString focusCategoryName READ focusCategoryName NOTIFY focusTaskChanged)
    Q_PROPERTY(QString focusCategoryAccent READ focusCategoryAccent NOTIFY focusTaskChanged)
    Q_PROPERTY(bool focusHasCategory READ focusHasCategory NOTIFY focusTaskChanged)

public:
    /// 焦点区域的展示状态，不等同于 TaskStatus。
    enum class FocusState {
        /// 没有可显示的活动任务。
        NoTasks = 0,
        /// Model 推荐了下一项可执行任务。
        Suggested = 1,
        /// 当前存在唯一进行中任务。
        InProgress = 2,
        /// 存在任务但全部被依赖阻塞。
        AllBlocked = 3,
    };
    Q_ENUM(FocusState)

    ~TaskFocusContract() override = default;

    // 全部 getter 组成同一焦点快照；无焦点时返回安全空值与 false/0。
    [[nodiscard]] virtual FocusState focusState() const noexcept = 0;
    [[nodiscard]] virtual QString focusTaskId() const = 0;
    [[nodiscard]] virtual QString focusTitle() const = 0;
    [[nodiscard]] virtual QString focusDescription() const = 0;
    [[nodiscard]] virtual QString focusStatusText() const = 0;
    [[nodiscard]] virtual QString focusPriorityText() const = 0;
    [[nodiscard]] virtual QString focusDeadlineText() const = 0;
    [[nodiscard]] virtual int focusEstimatedMinutes() const noexcept = 0;
    [[nodiscard]] virtual QString focusEstimatedText() const = 0;
    [[nodiscard]] virtual QString focusReasonText() const = 0;
    [[nodiscard]] virtual bool focusOverdue() const noexcept = 0;
    [[nodiscard]] virtual bool focusCanStart() const noexcept = 0;
    [[nodiscard]] virtual bool focusCanComplete() const noexcept = 0;
    [[nodiscard]] virtual QString focusCategoryName() const = 0;
    [[nodiscard]] virtual QString focusCategoryAccent() const = 0;
    [[nodiscard]] virtual bool focusHasCategory() const noexcept = 0;

signals:
    /// 焦点身份或任一派生展示字段变化后发送，绑定方应重新读取全部焦点 getter。
    void focusTaskChanged();
    /// 请求 View 展示一次性业务通知，不承担焦点数据同步。
    void notificationRaised(const smartmate::common::UiNotification &notification);

protected:
    /// 仅允许具体焦点 ViewModel 派生构造。
    explicit TaskFocusContract(QObject *parent = nullptr) : QObject(parent) {}
};

} // namespace smartmate::viewmodel
