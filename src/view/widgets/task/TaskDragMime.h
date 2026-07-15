#pragma once

namespace smartmate::view::widgets::task_drag_detail {

/// 任务列表与焦点面板共享的 View 层拖拽协议；载荷只包含稳定 TaskId。
inline constexpr auto mimeType = "application/x-smartmate-task-id";

} // namespace smartmate::view::widgets::task_drag_detail
