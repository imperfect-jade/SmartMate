#pragma once

#include "services/TaskResult.h"

#include <QString>

namespace smartmate::viewmodel {

/// 将与界面语言无关的领域错误翻译为用户可读的中文展示文本。
[[nodiscard]] QString taskErrorMessage(model::TaskError error);

} // namespace smartmate::viewmodel
