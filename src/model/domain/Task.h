#pragma once

#include "domain/TaskCategory.h"
#include "domain/TaskTypes.h"

#include <QDateTime>
#include <QString>

#include <optional>

namespace smartmate::model {

/// 尚未校验的普通任务字段输入；不包含状态，状态只能由显式领域命令改变。
struct TaskDraft final {
    QString title;
    /// 未填写描述时使用非 null 空字符串；描述可空不等于数据库中的 SQL NULL。
    QString description{QStringLiteral("")};
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
    TaskId m_id;
    QString m_title;
    QString m_description;
    TaskPriority m_priority{TaskPriority::Normal};
    TaskStatus m_status{TaskStatus::Todo};
    std::optional<TaskStatus> m_statusBeforeArchive;
    std::optional<QDateTime> m_deadline;
    std::optional<int> m_estimatedMinutes;
    std::optional<TaskCategoryId> m_categoryId;
    QDateTime m_createdAtUtc;
    QDateTime m_updatedAtUtc;
};

} // namespace smartmate::model
