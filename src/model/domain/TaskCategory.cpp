#include "domain/TaskCategory.h"

namespace smartmate::model {

QString taskCategoryNameKey(const QString &name)
{
    // NFKC 同时合并兼容字符，随后进行Unicode大小写折叠，保证界面预检与唯一索引一致。
    return name.trimmed()
        .normalized(QString::NormalizationForm_KC)
        .toCaseFolded();
}

bool isValidTaskCategoryColor(const TaskCategoryColor color) noexcept
{
    switch (color) {
    case TaskCategoryColor::Blue:
    case TaskCategoryColor::Teal:
    case TaskCategoryColor::Green:
    case TaskCategoryColor::Amber:
    case TaskCategoryColor::Orange:
    case TaskCategoryColor::Rose:
    case TaskCategoryColor::Violet:
    case TaskCategoryColor::Slate:
        return true;
    }
    return false;
}

QString taskCategoryColorToStorageText(const TaskCategoryColor color)
{
    // 使用稳定英文文本而非枚举序号，避免将来调整 C++ 枚举破坏旧数据。
    switch (color) {
    case TaskCategoryColor::Blue:
        return QStringLiteral("blue");
    case TaskCategoryColor::Teal:
        return QStringLiteral("teal");
    case TaskCategoryColor::Green:
        return QStringLiteral("green");
    case TaskCategoryColor::Amber:
        return QStringLiteral("amber");
    case TaskCategoryColor::Orange:
        return QStringLiteral("orange");
    case TaskCategoryColor::Rose:
        return QStringLiteral("rose");
    case TaskCategoryColor::Violet:
        return QStringLiteral("violet");
    case TaskCategoryColor::Slate:
        return QStringLiteral("slate");
    }
    return {};
}

std::optional<TaskCategoryColor> taskCategoryColorFromStorageText(
    const QString &text)
{
    // 未知文本不猜测默认值，让 Repository 能明确报告损坏数据。
    if (text == QStringLiteral("blue")) {
        return TaskCategoryColor::Blue;
    }
    if (text == QStringLiteral("teal")) {
        return TaskCategoryColor::Teal;
    }
    if (text == QStringLiteral("green")) {
        return TaskCategoryColor::Green;
    }
    if (text == QStringLiteral("amber")) {
        return TaskCategoryColor::Amber;
    }
    if (text == QStringLiteral("orange")) {
        return TaskCategoryColor::Orange;
    }
    if (text == QStringLiteral("rose")) {
        return TaskCategoryColor::Rose;
    }
    if (text == QStringLiteral("violet")) {
        return TaskCategoryColor::Violet;
    }
    if (text == QStringLiteral("slate")) {
        return TaskCategoryColor::Slate;
    }
    return std::nullopt;
}

} // namespace smartmate::model
