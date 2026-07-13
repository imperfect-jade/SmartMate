#pragma once

#include "domain/TaskCategory.h"

#include <QString>
#include <QStringList>

namespace smartmate::viewmodel {

/// 将领域颜色映射为稳定的界面调色板索引；不参与任何类别业务判断。
[[nodiscard]] int taskCategoryColorIndex(model::TaskCategoryColor color) noexcept;

/// 返回类别徽标使用的强调色；颜色只属于展示投影，不写入任务或依赖图。
[[nodiscard]] QString taskCategoryAccent(model::TaskCategoryColor color);

/// 返回固定调色板的中文名称，顺序与 TaskCategoryColor 的有效枚举值一致。
[[nodiscard]] QStringList taskCategoryColorOptions();

/// 将界面调色板索引转换为领域枚举；非法索引返回空值。
[[nodiscard]] std::optional<model::TaskCategoryColor> taskCategoryColorFromIndex(
    int index) noexcept;

} // namespace smartmate::viewmodel
