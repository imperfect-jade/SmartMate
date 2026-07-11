#pragma once

#include "domain/TaskTypes.h"

#include <QDateTime>
#include <QString>

#include <optional>

namespace smartmate::model {

/// 尚未校验的任务输入；是否满足业务规则必须由 TaskService 判定。
struct TaskDraft final {
    QString title;
    /// 未填写描述时使用非 null 空字符串；描述可空不等于数据库中的 SQL NULL。
    QString description{QStringLiteral("")};
    TaskPriority priority{TaskPriority::Normal};
    TaskStatus status{TaskStatus::Todo};
    /// 空值表示没有截止时间；非空时间会在写入领域实体前统一转换为 UTC。
    std::optional<QDateTime> deadline;
    /// 空值表示未估算，非空值的单位为分钟。
    std::optional<int> estimatedMinutes;
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
         QDateTime updatedAtUtc);

    [[nodiscard]] const TaskId &id() const noexcept;
    [[nodiscard]] const QString &title() const noexcept;
    [[nodiscard]] const QString &description() const noexcept;
    [[nodiscard]] TaskPriority priority() const noexcept;
    [[nodiscard]] TaskStatus status() const noexcept;
    /// 返回归档前状态；非归档任务应为空。
    [[nodiscard]] const std::optional<TaskStatus> &statusBeforeArchive() const noexcept;
    [[nodiscard]] const std::optional<QDateTime> &deadline() const noexcept;
    [[nodiscard]] const std::optional<int> &estimatedMinutes() const noexcept;
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
    QDateTime m_createdAtUtc;
    QDateTime m_updatedAtUtc;
};

} // namespace smartmate::model
