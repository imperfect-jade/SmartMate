#include "TaskErrorMapper.h"

namespace smartmate::viewmodel {

QString taskErrorMessage(const model::TaskError error)
{
    using enum model::TaskError;

    switch (error) {
    case None:
        return {};
    case EmptyTitle:
        return QStringLiteral("标题不能为空。");
    case TitleTooLong:
        return QStringLiteral("标题过长，请精简后重试。");
    case DescriptionTooLong:
        return QStringLiteral("任务描述过长，请精简后重试。");
    case InvalidDeadline:
        return QStringLiteral("截止时间无效。");
    case InvalidEstimate:
        return QStringLiteral("预计用时必须为 1 至 525600 分钟。");
    case InvalidPriority:
        return QStringLiteral("任务优先级无效。");
    case InvalidStatus:
        return QStringLiteral("任务状态无效。");
    case NotFound:
        return QStringLiteral("任务不存在或已无法访问。");
    case InProgressConflict:
        return QStringLiteral("已有任务正在进行，请先完成或更改它的状态。");
    case PersistenceFailure:
        return QStringLiteral("任务数据访问失败，请稍后重试。");
    }

    return QStringLiteral("未知错误。");
}

} // namespace smartmate::viewmodel
