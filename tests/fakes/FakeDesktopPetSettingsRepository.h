#pragma once

#include "repositories/IDesktopPetSettingsRepository.h"
#include "repositories/RepositoryException.h"

namespace smartmate::tests {

class FakeDesktopPetSettingsRepository final
    : public model::IDesktopPetSettingsRepository {
public:
    [[nodiscard]] model::DesktopPetSettings load() const override
    {
        if (failLoad) {
            throw model::RepositoryException{"desktop pet load failed"};
        }
        return settings;
    }

    void save(const model::DesktopPetSettings &value) override
    {
        if (failSave) {
            throw model::RepositoryException{"desktop pet save failed"};
        }
        settings = value;
        ++saveCount;
    }

    mutable bool failLoad{false};
    bool failSave{false};
    int saveCount{0};
    model::DesktopPetSettings settings;
};

} // namespace smartmate::tests
