#pragma once

#include "domain/Task.h"

#include <QList>
#include <QString>

#include <optional>
#include <stdexcept>

namespace smartmate::model {

/// 持久化端口无法完成操作时抛出的统一异常，具体存储错误不得泄漏到接口类型中。
class RepositoryException final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;

    explicit RepositoryException(const QString &message)
        : std::runtime_error(message.toUtf8().constData())
    {
    }
};

/// 任务持久化端口；删除采用归档语义，因此接口不提供物理 remove。
/// 读写故障必须抛出 RepositoryException，Service 会将其映射为结构化错误。
class ITaskRepository {
public:
    virtual ~ITaskRepository() = default;

    [[nodiscard]] virtual QList<Task> findAll() const = 0;
    /// 找不到任务返回空值，持久化故障仍抛出异常。
    [[nodiscard]] virtual std::optional<Task> findById(const TaskId &id) const = 0;
    virtual void insert(const Task &task) = 0;
    /// ID 不存在返回 false；实际写入故障抛出异常。
    [[nodiscard]] virtual bool update(const Task &task) = 0;
};

} // namespace smartmate::model
