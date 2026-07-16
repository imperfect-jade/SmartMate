#pragma once

#include "repositories/ITaskActivityRepository.h"
#include "repositories/ITaskCategoryRepository.h"
#include "repositories/ITaskCreationRepository.h"
#include "repositories/ITaskDeletionRepository.h"
#include "repositories/ITaskDependencyRepository.h"
#include "repositories/ITaskRepository.h"
#include "repositories/ITaskTransitionRepository.h"

#include <QString>

namespace smartmate::model::persistence {

/// SQLite 持久化 Adapter：构造时打开数据库并初始化 Schema，并独占一个 Qt SQL 连接。
///
/// 一个具体类实现多个窄 Repository 接口，是为了让跨 tasks/category/dependency 的命令
/// 共享同一连接和事务；Service 仍只依赖所需抽象端口。该类不继承 QObject、不发送
/// Model/Contract 通知，也不向 View 暴露 QSqlQuery 或 QSqlTableModel。
class SqliteTaskRepository final : public ITaskRepository,
                                   public ITaskDependencyRepository,
                                   public ITaskCreationRepository,
                                   public ITaskTransitionRepository,
                                   public ITaskDeletionRepository,
                                   public ITaskCategoryRepository,
                                   public ITaskActivityRepository {
public:
    /// 打开指定文件或 :memory: 数据库，配置连接并在原子迁移中升级 Schema。
    explicit SqliteTaskRepository(QString databasePath);
    /// 关闭专属连接，并从 Qt 进程级连接注册表移除连接名。
    ~SqliteTaskRepository() override;

    SqliteTaskRepository(const SqliteTaskRepository &) = delete;
    SqliteTaskRepository &operator=(const SqliteTaskRepository &) = delete;
    SqliteTaskRepository(SqliteTaskRepository &&) = delete;
    SqliteTaskRepository &operator=(SqliteTaskRepository &&) = delete;

    /// 查询任务快照；SQL 顺序仅保证确定性，业务排序由 Planner 负责。
    [[nodiscard]] QList<Task> findAll() const override;
    /// 按稳定 ID 查询任务；不存在返回空值。
    [[nodiscard]] std::optional<Task> findById(const TaskId &id) const override;
    /// 插入完整任务快照，不执行 ViewModel 通知。
    void insert(const Task &task) override;
    /// 覆盖任务快照；ID 不存在返回 false。
    [[nodiscard]] bool update(const Task &task) override;
    /// 查询全部依赖边，并按后继、前置稳定 ID 排序。
    [[nodiscard]] QList<TaskDependency> findAllDependencies() const override;
    /// 在单个事务内删除旧入边并插入新的完整前置集合。
    void replacePredecessors(const TaskId &successorId,
                             const QList<TaskId> &predecessorIds) override;
    /// 在一个SQLite事务内创建任务及全部前置边；任一步失败都不保留任务或部分边。
    void insertTaskWithPredecessors(
        const Task &task,
        const QList<TaskId> &predecessorIds) override;
    /// 在一个事务内批量更新状态；任一预期状态不匹配时整批回滚。
    [[nodiscard]] TaskTransitionWriteResult applyTransitionsAtomically(
        const QList<TaskTransitionWrite> &writes) override;
    /// 在一个事务内永久删除归档任务及其全部入边、出边。
    [[nodiscard]] TaskDeletionWriteResult deleteArchivedTasksWithDependencies(
        const QList<TaskId> &taskIds) override;
    /// 查询全部类别快照，并按名称键、稳定 ID 排序。
    [[nodiscard]] QList<TaskCategory> findAllCategories() const override;
    /// 按稳定类别 ID 查询；不存在返回空值。
    [[nodiscard]] std::optional<TaskCategory> findCategoryById(
        const TaskCategoryId &id) const override;
    /// 插入包含规范化名称键映射的类别快照。
    void insertCategory(const TaskCategory &category) override;
    /// 覆盖类别快照；稳定 ID 不存在返回 false。
    [[nodiscard]] bool updateCategory(const TaskCategory &category) override;
    /// 原子解除全部任务归属并删除类别；任务状态和依赖边保持不变。
    [[nodiscard]] CategoryDeletionWriteResult deleteCategoryAndUnassignTasks(
        const TaskCategoryId &id,
        const QDateTime &updatedAtUtc) override;
    [[nodiscard]] QList<TaskActivityEvent> findEventsByOccurredAt(
        const QDateTime &startInclusiveUtc,
        const QDateTime &endExclusiveUtc) const override;
    [[nodiscard]] std::optional<TaskActivityEvent> findLatestCompletionBefore(
        const QDateTime &endExclusiveUtc) const override;

private:
    /// 启用外键、锁等待和文件数据库 WAL；inMemory 为 true 时不启用 WAL。
    void configureConnection(bool inMemory);
    /// 在事务内创建或迁移 Schema，任何失败都回滚 DDL 与版本号。
    void initializeSchema();

    /// Qt 进程级注册表中的唯一连接名，由本实例独占并在析构时移除。
    QString m_connectionName;
};

} // namespace smartmate::model::persistence
