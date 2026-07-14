#pragma once

namespace smartmate::model::TaskConstraints {

/// 预计用时的领域边界，所有输入方式和业务校验必须引用同一组常量。
inline constexpr int minimumEstimatedMinutes = 1;
/// 最大值为 365 天，防止无意义的大数进入输入、显示和持久化流程。
inline constexpr int maximumEstimatedMinutes = 525600;

} // namespace smartmate::model::TaskConstraints
