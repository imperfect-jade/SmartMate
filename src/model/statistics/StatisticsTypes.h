#pragma once

#include "domain/TaskCategory.h"

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QTimeZone>

#include <optional>
#include <utility>

namespace smartmate::model {

enum class StatisticsRange {
    Last7Days,
    Last30Days,
    Last12Weeks,
};

enum class StatisticsChangeSemantic {
    Neutral,
    Positive,
    Risk,
};

struct StatisticsQuery final {
    StatisticsRange range{StatisticsRange::Last7Days};
    QDateTime nowUtc;
    QTimeZone timeZone;
};

struct StatisticsComparison final {
    int currentCount{0};
    int previousCount{0};
    int delta{0};
    StatisticsChangeSemantic semantic{StatisticsChangeSemantic::Neutral};

    friend bool operator==(const StatisticsComparison &,
                           const StatisticsComparison &) = default;
};

struct StatisticsTrendBucket final {
    QDate localStartDate;
    QDate localEndDate;
    int completionCount{0};
    bool current{false};

    friend bool operator==(const StatisticsTrendBucket &,
                           const StatisticsTrendBucket &) = default;
};

enum class StatisticsCategoryKind {
    Categorized,
    Unclassified,
    Other,
};

struct StatisticsCategoryBucket final {
    StatisticsCategoryKind kind{StatisticsCategoryKind::Unclassified};
    std::optional<TaskCategoryId> categoryId;
    QString categoryName;
    std::optional<TaskCategoryColor> categoryColor;
    int completionCount{0};

    friend bool operator==(const StatisticsCategoryBucket &,
                           const StatisticsCategoryBucket &) = default;
};

struct TaskHealthSnapshot final {
    int activeTaskCount{0};
    int executableCount{0};
    int blockedCount{0};
    int dueSoonCount{0};
    int overdueCount{0};
    int urgentOverdueCount{0};

    friend bool operator==(const TaskHealthSnapshot &,
                           const TaskHealthSnapshot &) = default;
};

struct StatisticsSnapshot final {
    StatisticsRange range{StatisticsRange::Last7Days};
    StatisticsComparison today;
    StatisticsComparison thisWeek;
    StatisticsComparison selectedPeriod;
    int onTimeCompletionCount{0};
    int deadlineCompletionCount{0};
    TaskHealthSnapshot health;
    QList<StatisticsTrendBucket> trend;
    QList<StatisticsCategoryBucket> categories;
    bool hasCompletionHistory{false};

    [[nodiscard]] std::optional<double> onTimeRate() const noexcept
    {
        if (deadlineCompletionCount == 0) {
            return std::nullopt;
        }
        return static_cast<double>(onTimeCompletionCount)
            / static_cast<double>(deadlineCompletionCount);
    }

    friend bool operator==(const StatisticsSnapshot &,
                           const StatisticsSnapshot &) = default;
};

enum class StatisticsError {
    None,
    InvalidQuery,
    PersistenceFailure,
};

struct StatisticsResult final {
    std::optional<StatisticsSnapshot> value;
    StatisticsError error{StatisticsError::None};
    QString detail;

    [[nodiscard]] bool ok() const noexcept
    {
        return error == StatisticsError::None && value.has_value();
    }

    [[nodiscard]] static StatisticsResult success(StatisticsSnapshot snapshot)
    {
        StatisticsResult result;
        result.value.emplace(std::move(snapshot));
        return result;
    }

    [[nodiscard]] static StatisticsResult failure(StatisticsError error,
                                                  QString detail = {})
    {
        StatisticsResult result;
        result.error = error;
        result.detail = std::move(detail);
        return result;
    }
};

} // namespace smartmate::model
