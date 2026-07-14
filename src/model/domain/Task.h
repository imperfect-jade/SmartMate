#pragma once

#include "domain/TaskCategory.h"
#include "domain/TaskTypes.h"

#include <QDateTime>
#include <QString>

#include <optional>

namespace smartmate::model {

/// 尚未校验的普通任务字段输入；不包含状态，状态只能由显式领域命令改变。
/// 任务草稿
struct TaskDraft final {
    /// 用户可见标题；去空白和长度规则由 Model 校验。
    QString title;
    /// 未填写描述时使用非 null 空字符串；描述可空不等于数据库中的 SQL NULL。
    QString description{QStringLiteral("")};
    /// 用户选择的业务优先级，不代表最终推荐顺序。
    TaskPriority priority{TaskPriority::Normal};
    /// 空值表示没有截止时间；非空时间会在写入领域实体前统一转换为 UTC。
    std::optional<QDateTime> deadline;
    /// 空值表示未估算，非空值的单位为分钟。
    std::optional<int> estimatedMinutes;
    /// 空值表示“未分类”；只保存稳定ID，类别名称和颜色不嵌入任务快照。
    std::optional<TaskCategoryId> categoryId;
};

/// 不依赖 QObject 的只读领域快照，由 Service 创建、由 Repository 重建。
class Task final {
public:
    /// 时间戳必须使用 UTC；statusBeforeArchive 仅在归档状态下记录恢复目标。
    Task(TaskId id,
         QString title,
         QString description,
         TaskPriority priority,
         TaskStatus status,
         std::optional<TaskStatus> statusBeforeArchive,
         std::optional<QDateTime> deadline,
         std::optional<int> estimatedMinutes,
         QDateTime createdAtUtc,
         QDateTime updatedAtUtc,
         std::optional<TaskCategoryId> categoryId = std::nullopt);

    [[nodiscard]] const TaskId &id() const noexcept;
    [[nodiscard]] const QString &title() const noexcept;
    [[nodiscard]] const QString &description() const noexcept;
    [[nodiscard]] TaskPriority priority() const noexcept;
    [[nodiscard]] TaskStatus status() const noexcept;
    /// 仅待办任务允许修改普通字段；其他状态只能通过显式状态命令流转。
    [[nodiscard]] bool canEditDetails() const noexcept;
    /// 仅归档任务允许进入不可撤销的永久删除流程。
    [[nodiscard]] bool canDeletePermanently() const noexcept;
    /// 返回归档前状态；非归档任务应为空。
    [[nodiscard]] const std::optional<TaskStatus> &statusBeforeArchive() const noexcept;
    [[nodiscard]] const std::optional<QDateTime> &deadline() const noexcept;
    [[nodiscard]] const std::optional<int> &estimatedMinutes() const noexcept;
    /// 空值表示任务当前未归入任何类别。
    [[nodiscard]] const std::optional<TaskCategoryId> &categoryId() const noexcept;
    /// 创建与更新时间均以 UTC 表示。
    [[nodiscard]] const QDateTime &createdAtUtc() const noexcept;
    [[nodiscard]] const QDateTime &updatedAtUtc() const noexcept;

    friend bool operator==(const Task &, const Task &) = default;

private:
    /// 永不随标题、状态或排序变化的任务身份。
    TaskId m_id;
    /// 用户可见标题，仅 Todo 状态允许修改。
    QString m_title;
    /// 可选描述在领域内始终使用非 null 字符串。
    QString m_description;
    /// 业务优先级，由规划策略参与推荐排序。
    TaskPriority m_priority{TaskPriority::Normal};
    /// 当前生命周期状态，只能通过 TaskStateMachine 允许的命令变化。
    TaskStatus m_status{TaskStatus::Todo};
    /// 归档前的恢复目标；非 Archived 状态应为空。
    std::optional<TaskStatus> m_statusBeforeArchive;
    /// 可选截止时间，存在时统一使用 UTC。
    std::optional<QDateTime> m_deadline;
    /// 可选预计用时，单位为分钟。
    std::optional<int> m_estimatedMinutes;
    /// 可空类别身份；任务快照不复制类别名称和颜色。
    std::optional<TaskCategoryId> m_categoryId;
    /// 首次创建时间，使用 UTC 且普通编辑不会改变。
    QDateTime m_createdAtUtc;
    /// 最近一次成功写入时间，使用 UTC。
    QDateTime m_updatedAtUtc;
};

} // namespace smartmate::model
