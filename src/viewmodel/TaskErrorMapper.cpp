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
    case InvalidTaskTransition:
        return QStringLiteral("当前任务状态不允许执行此操作。");
    case DependencyEndpointNotFound:
        return QStringLiteral("依赖关系中的任务不存在或已无法访问。");
    case DependencySelfReference:
        return QStringLiteral("任务不能依赖自身。");
    case DependencyDuplicate:
        return QStringLiteral("不能重复添加同一个前置任务。");
    case DependencyCycle:
        return QStringLiteral("该操作会形成循环依赖。");
    case DependencyTargetNotEditable:
        return QStringLiteral("只有活动的待办任务可以编辑前置依赖。");
    case DependencyPredecessorNotEligible:
        return QStringLiteral("不能新增已归档或已取消任务作为前置任务。");
    case TaskBlocked:
        return QStringLiteral("任务仍被尚未完成或取消的前置任务阻塞，不能开始或完成。");
    case DependencyStateConflict:
        return QStringLiteral("状态修改会使正在进行或已完成的后继任务失去有效前置条件。");
    case TaskDetailsNotEditable:
        return QStringLiteral("只有待办任务可以编辑，请先通过合法状态操作将任务恢复为待办。");
    case TaskDeletionNotAllowed:
        return QStringLiteral("只有归档任务可以永久删除。");
    case EmptyTaskSelection:
        return QStringLiteral("请至少选择一项可以批量处理的任务。");
    case TaskCategoryNotFound:
        return QStringLiteral("所选类别不存在或已被删除，请重新选择。");
    case NotFound:
        return QStringLiteral("任务不存在或已无法访问。");
    case InProgressConflict:
        return QStringLiteral("已有任务正在进行，请先完成或取消它。");
    case PersistenceFailure:
        return QStringLiteral("任务数据访问失败，请稍后重试。");
    }

    return QStringLiteral("未知错误。");
}

} // namespace smartmate::viewmodel
