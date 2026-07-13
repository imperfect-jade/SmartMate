#pragma once

#include <QDateTime>
#include <QString>
#include <QUuid>

#include <optional>

namespace smartmate::model {

/// 类别在所有层之间传递的稳定身份；名称变化不得改变该身份。
using TaskCategoryId = QUuid;

/// 固定调色板的稳定领域值；持久化必须使用英文文本而非枚举序号。
enum class TaskCategoryColor : int {
    Blue = 0,
    Teal = 1,
    Green = 2,
    Amber = 3,
    Orange = 4,
    Rose = 5,
    Violet = 6,
    Slate = 7,
};

/// 用户输入的类别草稿；名称校验与规范化由 TaskCategoryService 完成。
struct TaskCategoryDraft final {
    QString name;
    TaskCategoryColor color{TaskCategoryColor::Blue};

    friend bool operator==(const TaskCategoryDraft &,
                           const TaskCategoryDraft &) = default;
};

/// 独立于任务的类别实体；任务只保存可空的稳定 TaskCategoryId。
struct TaskCategory final {
    TaskCategoryId id;
    QString name;
    TaskCategoryColor color{TaskCategoryColor::Blue};
    QDateTime createdAtUtc;
    QDateTime updatedAtUtc;

    friend bool operator==(const TaskCategory &, const TaskCategory &) = default;
};

/// 生成名称唯一性键；Model 与 Persistence 必须复用此函数避免规范化规则分叉。
[[nodiscard]] QString taskCategoryNameKey(const QString &name);

/// 判断颜色是否属于当前固定调色板。
[[nodiscard]] bool isValidTaskCategoryColor(TaskCategoryColor color) noexcept;

/// 将颜色转换为跨版本稳定的 SQLite 文本。
[[nodiscard]] QString taskCategoryColorToStorageText(TaskCategoryColor color);

/// 从稳定文本恢复颜色；未知文本返回空值，由Repository视为损坏数据。
[[nodiscard]] std::optional<TaskCategoryColor> taskCategoryColorFromStorageText(
    const QString &text);

} // namespace smartmate::model
