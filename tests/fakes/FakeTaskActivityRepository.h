#pragma once

#include "repositories/ITaskActivityRepository.h"
#include "repositories/RepositoryException.h"

#include <algorithm>
#include <utility>

namespace smartmate::tests {

class FakeTaskActivityRepository final
    : public model::ITaskActivityRepository {
public:
    explicit FakeTaskActivityRepository(
        QList<model::TaskActivityEvent> events = {})
        : m_events(std::move(events))
    {
    }

    [[nodiscard]] QList<model::TaskActivityEvent> findEventsByOccurredAt(
        const QDateTime &startInclusiveUtc,
        const QDateTime &endExclusiveUtc) const override
    {
        ++m_rangeQueryCount;
        throwIfNeeded();
        QList<model::TaskActivityEvent> result;
        for (const auto &event : m_events) {
            if (event.occurredAtUtc >= startInclusiveUtc
                && event.occurredAtUtc < endExclusiveUtc) {
                result.append(event);
            }
        }
        std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
            return left.occurredAtUtc == right.occurredAtUtc
                ? left.eventId.toString() < right.eventId.toString()
                : left.occurredAtUtc < right.occurredAtUtc;
        });
        return result;
    }

    [[nodiscard]] std::optional<model::TaskActivityEvent>
    findLatestCompletionBefore(const QDateTime &endExclusiveUtc) const override
    {
        throwIfNeeded();
        std::optional<model::TaskActivityEvent> latest;
        for (const auto &event : m_events) {
            if (event.transition != model::TaskTransition::Complete
                || event.occurredAtUtc >= endExclusiveUtc) {
                continue;
            }
            if (!latest.has_value()
                || latest->occurredAtUtc < event.occurredAtUtc) {
                latest = event;
            }
        }
        return latest;
    }

    void setReadFailure(const bool enabled) noexcept { m_failReads = enabled; }
    void setEvents(QList<model::TaskActivityEvent> events)
    {
        m_events = std::move(events);
    }
    [[nodiscard]] int rangeQueryCount() const noexcept
    {
        return m_rangeQueryCount;
    }

private:
    void throwIfNeeded() const
    {
        if (m_failReads) {
            throw model::RepositoryException("Fake activity read failure.");
        }
    }

    QList<model::TaskActivityEvent> m_events;
    mutable int m_rangeQueryCount{0};
    bool m_failReads{false};
};

} // namespace smartmate::tests
