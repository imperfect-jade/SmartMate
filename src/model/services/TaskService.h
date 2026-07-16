#pragma once

#include "domain/TaskCreationRequest.h"
#include "domain/TaskStateMachine.h"
#include "repositories/ITaskTransitionRepository.h"
#include "repositories/ITaskCreationRepository.h"
#include "repositories/ITaskCategoryRepository.h"
#include "repositories/ITaskDeletionRepository.h"
#include "repositories/ITaskDependencyRepository.h"
#include "repositories/ITaskRepository.h"
#include "services/TaskResult.h"

#include <QObject>

namespace smartmate::model {

/// 任务业务规则与持久化编排的唯一入口，不拥有注入的 Repository。
///
/// public 写方法是以 TaskId/TaskDraft 表达的强类型语义命令，禁止字符串命令名或
/// QVariant 路由；查询方法向具体 ViewModel 提供领域快照。写成功后的信号只负责
/// 宣告相关快照失效，ViewModel 必须重新查询、投影为 Contract getter/Role，再由
/// Widget 绑定；Service 不保存控件引用，也不直接发送界面通知。
class TaskService final : public QObject {
    Q_OBJECT

public:
    /// 注入查询端口及跨表原子命令端口；QObject 仅用于发布强类型变化信号。
    TaskService(ITaskRepository &repository,
                ITaskDependencyRepository &dependencyRepository,
                ITaskCreationRepository &creationRepository,
                ITaskTransitionRepository &transitionRepository,
                ITaskDeletionRepository &deletionRepository,
                ITaskCategoryRepository &categoryRepository,
                QObject *parent = nullptr);

    /// 执行无副作用的权威业务校验：不访问 Repository，也不发出状态通知。
    [[nodiscard]] TaskValidationResult validateDraft(const TaskDraft &draft) const;
    /// 单独校验类型化选择器产生的总分钟数，规则与完整草稿校验一致。
    [[nodiscard]] TaskValidationResult validateEstimatedMinutes(int minutes) const;
    /// 查询命令：读取全部任务领域快照；不附加展示筛选，也不发送变化通知。
    [[nodiscard]] TaskListResult listTasks() const;
    /// 返回新建关系可选的前置任务，排序稳定且不包含归档或取消任务。
    [[nodiscard]] TaskListResult listEligibleCreationPredecessors() const;
    /// 查询命令：计算推荐顺序、依赖状态与命令资格，供列表/详情 ViewModel 数据绑定。
    [[nodiscard]] TaskPlanResult listRecommendedTasks() const;
    /// 查询命令：读取全部 Finish-to-Start 依赖边，不发送变化通知。
    [[nodiscard]] TaskDependencyListResult listDependencies() const;
    /// 读取依赖编辑器的完整业务上下文；候选范围与选择资格全部由 Model 判定。
    [[nodiscard]] TaskDependencyEditContextResult taskDependencyEditContext(
        const TaskId &taskId) const;
    /// 生成依赖图领域快照；只保留活动节点及与其处于同一依赖组件的归档节点。
    [[nodiscard]] TaskGraphResult taskGraphSnapshot() const;
    /// 按类别裁剪图：保留核心节点及其直接跨类别邻居，隐藏外部节点之间的边。
    [[nodiscard]] TaskGraphResult taskGraphSnapshot(
        const TaskGraphQuery &query) const;
    /// 依赖保存命令：原子替换全部前置；实际变化后发送 dependenciesChanged()。
    [[nodiscard]] TaskDependencyListResult replaceTaskPredecessors(
        const TaskId &taskId,
        const QList<TaskId> &predecessorIds);
    /// 按稳定 TaskId 查找任务；不存在时返回结构化 NotFound。
    [[nodiscard]] TaskResult findTask(const TaskId &id) const;
    /// 加载可编辑任务；非 Todo 返回 TaskDetailsNotEditable，调用方不得建立编辑草稿。
    [[nodiscard]] TaskResult findEditableTask(const TaskId &id) const;
    /// 创建命令：校验并持久化无前置任务；成功后发送 tasksChanged()。
    [[nodiscard]] TaskResult createTask(const TaskDraft &draft);
    /// 创建命令：原子新建任务和前置；成功发送 tasksChanged()，有前置时再通知依赖变化。
    [[nodiscard]] TaskResult createTask(const TaskCreationRequest &request);
    /// 详情保存命令：按稳定 TaskId 更新 Todo；成功后发送 tasksChanged()。
    [[nodiscard]] TaskResult updateTask(const TaskId &id, const TaskDraft &draft);
    /// 开始命令：将 Todo 转为 InProgress；成功后发送 tasksChanged()。
    [[nodiscard]] TaskResult startTask(const TaskId &id);
    /// 取消命令：将 Todo/InProgress 转为 Cancelled；成功后发送 tasksChanged()。
    [[nodiscard]] TaskResult cancelTask(const TaskId &id);
    /// 完成命令：将 InProgress 转为 Done；成功后发送 tasksChanged()。
    [[nodiscard]] TaskResult completeTask(const TaskId &id);
    /// 重做命令：将 Done/Cancelled 转为 Todo；成功后发送 tasksChanged()。
    [[nodiscard]] TaskResult redoTask(const TaskId &id);
    /// 仅将 Done/Cancelled 软归档，不做物理删除。
    [[nodiscard]] TaskResult archiveTask(const TaskId &id);
    /// 原子归档全部 Done/Cancelled 任务；任一目标不合格时整批不写入。
    [[nodiscard]] TaskBatchResult archiveTasks(const QList<TaskId> &taskIds);
    /// 恢复正常归档前状态；旧 Todo/InProgress 恢复点统一安全降级为 Todo。
    [[nodiscard]] TaskResult restoreTask(const TaskId &id);
    /// 原子恢复全部归档任务，并基于最终假想快照统一校验依赖一致性。
    [[nodiscard]] TaskBatchResult restoreTasks(const QList<TaskId> &taskIds);
    /// 永久删除归档任务及全部关联依赖；该操作不可撤销。
    [[nodiscard]] TaskResult deleteArchivedTask(const TaskId &id);
    /// 永久删除命令：成功通知任务变化；实际删除依赖边时再通知依赖变化。
    [[nodiscard]] TaskBatchResult deleteArchivedTasks(
        const QList<TaskId> &taskIds);

signals:
    /// 任务快照失效通知：仅在一次实际写入成功后发出，订阅 ViewModel 应重新查询投影。
    void tasksChanged();
    /// 依赖快照失效通知：关系实际增删后发出，列表/详情/焦点/图 ViewModel 分别重投影。
    void dependenciesChanged();

private:
    /// 复用唯一状态机执行转换，并确保一次成功命令只写入和通知一次。
    [[nodiscard]] TaskResult applyTransition(const TaskId &id,
                                             TaskTransition transition);
    /// 归档与恢复共用整批校验和原子写入；单项命令也必须经过此路径。
    [[nodiscard]] TaskBatchResult applyBatchTransition(
        const QList<TaskId> &taskIds,
        TaskTransition transition);

    // 以下均为非拥有端口引用，其生命周期必须长于 TaskService。
    /// 任务快照的基础查询与单项条件更新端口。
    ITaskRepository &m_repository;
    /// 依赖边查询与“替换全部前置”的原子写入端口。
    ITaskDependencyRepository &m_dependencyRepository;
    /// 独立命令端口保证跨 tasks 与 task_dependencies 的写入具有事务边界。
    ITaskCreationRepository &m_creationRepository;
    /// 转换端口以条件更新防御 Service 预检后的并发变化，并原子提交状态与事件。
    ITaskTransitionRepository &m_transitionRepository;
    /// 永久删除端口保证任务与全部入边、出边在同一事务内移除。
    ITaskDeletionRepository &m_deletionRepository;
    /// 仅用于验证任务草稿中的稳定类别 ID 和分类图查询，不承担类别生命周期命令。
    ITaskCategoryRepository &m_categoryRepository;
};

} // namespace smartmate::model
