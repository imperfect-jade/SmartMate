#pragma once

#include "common/presentation/UiNotification.h"

#include <QObject>
#include <QString>

namespace smartmate::viewmodel {

/// 桌宠设置向 View 暴露的稳定契约；位置以屏幕名和归一化坐标表达。
class DesktopPetSettingsContract : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool hasFloatingPlacement READ hasFloatingPlacement
                   NOTIFY floatingPlacementChanged)
    Q_PROPERTY(QString floatingScreenName READ floatingScreenName
                   NOTIFY floatingPlacementChanged)
    Q_PROPERTY(qreal floatingXRatio READ floatingXRatio
                   NOTIFY floatingPlacementChanged)
    Q_PROPERTY(qreal floatingYRatio READ floatingYRatio
                   NOTIFY floatingPlacementChanged)

public:
    ~DesktopPetSettingsContract() override = default;

    [[nodiscard]] virtual bool enabled() const noexcept = 0;
    [[nodiscard]] virtual bool hasFloatingPlacement() const noexcept = 0;
    [[nodiscard]] virtual QString floatingScreenName() const = 0;
    [[nodiscard]] virtual qreal floatingXRatio() const noexcept = 0;
    [[nodiscard]] virtual qreal floatingYRatio() const noexcept = 0;

public slots:
    virtual void setEnabled(bool enabled) = 0;
    virtual void saveFloatingPlacement(const QString &screenName,
                                       qreal xRatio, qreal yRatio) = 0;

signals:
    void enabledChanged();
    void floatingPlacementChanged();
    void notificationRaised(const smartmate::common::UiNotification &notification);

protected:
    explicit DesktopPetSettingsContract(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
};

} // namespace smartmate::viewmodel
