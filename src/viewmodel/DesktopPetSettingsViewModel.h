#pragma once

#include "domain/DesktopPetSettings.h"
#include "viewmodel/contracts/DesktopPetSettingsContract.h"

namespace smartmate::model {
class DesktopPetSettingsService;
}

namespace smartmate::viewmodel {

/// 将已确认的桌宠设置投影给 View，并在发布通知前完成持久化。
class DesktopPetSettingsViewModel final : public DesktopPetSettingsContract {
    Q_OBJECT

public:
    explicit DesktopPetSettingsViewModel(QObject *parent = nullptr);
    explicit DesktopPetSettingsViewModel(model::DesktopPetSettingsService &service,
                                         QObject *parent = nullptr);

    [[nodiscard]] bool enabled() const noexcept override;
    [[nodiscard]] bool hasFloatingPlacement() const noexcept override;
    [[nodiscard]] QString floatingScreenName() const override;
    [[nodiscard]] qreal floatingXRatio() const noexcept override;
    [[nodiscard]] qreal floatingYRatio() const noexcept override;

    void setEnabled(bool enabled) override;
    void saveFloatingPlacement(const QString &screenName,
                               qreal xRatio, qreal yRatio) override;

private:
    void load();
    void saveCandidate(const model::DesktopPetSettings &candidate);
    void apply(const model::DesktopPetSettings &settings);
    void raiseSaveError();

    model::DesktopPetSettingsService *m_service{nullptr};
    model::DesktopPetSettings m_settings;
};

} // namespace smartmate::viewmodel
