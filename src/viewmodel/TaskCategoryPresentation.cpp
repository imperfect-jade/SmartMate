#include "TaskCategoryPresentation.h"

namespace smartmate::viewmodel {

int taskCategoryColorIndex(const model::TaskCategoryColor color) noexcept
{
    return model::isValidTaskCategoryColor(color) ? static_cast<int>(color) : 0;
}

QString taskCategoryAccent(const model::TaskCategoryColor color)
{
    switch (color) {
    case model::TaskCategoryColor::Blue: return QStringLiteral("#2563eb");
    case model::TaskCategoryColor::Teal: return QStringLiteral("#0f766e");
    case model::TaskCategoryColor::Green: return QStringLiteral("#15803d");
    case model::TaskCategoryColor::Amber: return QStringLiteral("#b45309");
    case model::TaskCategoryColor::Orange: return QStringLiteral("#c2410c");
    case model::TaskCategoryColor::Rose: return QStringLiteral("#be123c");
    case model::TaskCategoryColor::Violet: return QStringLiteral("#7c3aed");
    case model::TaskCategoryColor::Slate: return QStringLiteral("#475569");
    }
    return QStringLiteral("#475569");
}

QStringList taskCategoryColorOptions()
{
    return {QStringLiteral("蓝色"), QStringLiteral("青色"),
            QStringLiteral("绿色"), QStringLiteral("琥珀色"),
            QStringLiteral("橙色"), QStringLiteral("玫红色"),
            QStringLiteral("紫色"), QStringLiteral("灰蓝色")};
}

std::optional<model::TaskCategoryColor> taskCategoryColorFromIndex(
    const int index) noexcept
{
    const auto color = static_cast<model::TaskCategoryColor>(index);
    if (!model::isValidTaskCategoryColor(color)) {
        return std::nullopt;
    }
    return color;
}

} // namespace smartmate::viewmodel
