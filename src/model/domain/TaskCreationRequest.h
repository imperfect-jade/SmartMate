#pragma once

#include "domain/Task.h"

#include <QList>

namespace smartmate::model {

/// 新建任务命令；任务字段与全部 Finish-to-Start 前置必须作为一个原子写入提交。
struct TaskCreationRequest final {
    TaskDraft task;
    /// 使用稳定 TaskId 标识已存在的前置任务；空列表表示创建无依赖任务。
    QList<TaskId> predecessorIds;
};

} // namespace smartmate::model
