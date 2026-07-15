#pragma once

#include "domain/Task.h"
#include "domain/TaskCategory.h"

#include <QLatin1StringView>
#include <QString>

class QSqlDatabase;
class QSqlError;
class QSqlQuery;

namespace smartmate::model::persistence::sqlite_task_repository_detail {

/// 返回与 taskFromQuery() 索引映射严格对应的稳定任务列顺序。
[[nodiscard]] QLatin1StringView taskSelectColumns() noexcept;
/// 返回与 categoryFromQuery() 索引映射严格对应的稳定类别列顺序。
[[nodiscard]] QLatin1StringView categorySelectColumns() noexcept;

/// 将 Qt SQL 技术错误收敛为 RepositoryException，禁止 SQL 类型越过端口。
[[noreturn]] void throwDatabaseError(const QString &operation,
                                     const QSqlError &error);
[[noreturn]] void throwPersistenceError(const QString &message);
void executeStatement(QSqlDatabase &database, const QString &statement);

/// 严格把当前查询行还原为领域快照；损坏数据抛出 RepositoryException。
[[nodiscard]] Task taskFromQuery(const QSqlQuery &query);
[[nodiscard]] TaskCategory categoryFromQuery(const QSqlQuery &query);
/// 使用命名参数写入稳定存储编码；可空领域值映射为 SQL NULL。
void bindTask(QSqlQuery &query, const Task &task);
void bindCategory(QSqlQuery &query, const TaskCategory &category);

/// 检查旧 Schema 列是否存在，供原子迁移选择升级路径。
[[nodiscard]] bool tableHasColumn(QSqlDatabase &database,
                                  const QString &tableName,
                                  const QString &columnName);

} // namespace smartmate::model::persistence::sqlite_task_repository_detail
