#pragma once

#include "repositories/IAppearanceSettingsRepository.h"
#include "repositories/RepositoryException.h"

namespace smartmate::tests {

class FakeAppearanceSettingsRepository final
    : public model::IAppearanceSettingsRepository {
public:
    [[nodiscard]] model::AppearanceSettings load() const override
    {
        if (failLoad) throw model::RepositoryException{"appearance load failed"};
        return settings;
    }
    void save(const model::AppearanceSettings &newSettings) override
    {
        if (failSave) throw model::RepositoryException{"appearance save failed"};
        settings = newSettings;
        ++saveCount;
    }
    model::AppearanceSettings settings;
    bool failLoad{false};
    bool failSave{false};
    int saveCount{0};
};

} // namespace smartmate::tests
