#pragma once

#include "repositories/ITaskBatchTransitionRepository.h"
#include "repositories/ITaskCategoryRepository.h"
#include "repositories/ITaskCreationRepository.h"
#include "repositories/ITaskDeletionRepository.h"
#include "repositories/ITaskDependencyRepository.h"
#include "repositories/ITaskRepository.h"

#include <QString>

namespace smartmate::model::persistence {

/// 构造时打开数据库并初始化Schema；对象生命周期同时管理其专属Qt SQL连接。
class SqliteTaskRepository final : public ITaskRepository,
                                   public ITaskDependencyRepository,
                                   public ITaskCreationRepository,
                                   public ITaskBatchTransitionRepository,
                                   public ITaskDeletionRepository,
                                   public ITaskCategoryRepository {
public:
    explicit SqliteTaskRepository(QString databasePath);
    ~SqliteTaskRepository() override;

    SqliteTaskRepository(const SqliteTaskRepository &) = delete;
    SqliteTaskRepository &operator=(const SqliteTaskRepository &) = delete;
    SqliteTaskRepository(SqliteTaskRepository &&) = delete;
    SqliteTaskRepository &operator=(SqliteTaskRepository &&) = delete;

    [[nodiscard]] QList<Task> findAll() const override;
    [[nodiscard]] std::optional<Task> findById(const TaskId &id) const override;
    void insert(const Task &task) override;
    [[nodiscard]] bool update(const Task &task) override;
    [[nodiscard]] QList<TaskDependency> findAllDependencies() const override;
    void replacePredecessors(const TaskId &successorId,
                             const QList<TaskId> &predecessorIds) override;
    /// 在一个SQLite事务内创建任务及全部前置边；任一步失败都不保留任务或部分边。
    void insertTaskWithPredecessors(
        const Task &task,
        const QList<TaskId> &predecessorIds) override;
    /// 在一个事务内批量更新状态；任一预期状态不匹配时整批回滚。
    [[nodiscard]] TaskBatchWriteResult updateTaskStatesAtomically(
        const QList<TaskStateChange> &changes) override;
    /// 在一个事务内永久删除归档任务及其全部入边、出边。
    [[nodiscard]] TaskDeletionWriteResult deleteArchivedTasksWithDependencies(
        const QList<TaskId> &taskIds) override;
    [[nodiscard]] QList<TaskCategory> findAllCategories() const override;
    [[nodiscard]] std::optional<TaskCategory> findCategoryById(
        const TaskCategoryId &id) const override;
    void insertCategory(const TaskCategory &category) override;
    [[nodiscard]] bool updateCategory(const TaskCategory &category) override;
    /// 原子解除全部任务归属并删除类别；任务状态和依赖边保持不变。
    [[nodiscard]] CategoryDeletionWriteResult deleteCategoryAndUnassignTasks(
        const TaskCategoryId &id,
        const QDateTime &updatedAtUtc) override;

private:
    void configureConnection(bool inMemory);
    void initializeSchema();

    // Qt进程级注册表中的唯一连接名，由本实例独占并在析构时移除。
    QString m_connectionName;
};

} // namespace smartmate::model::persistence
