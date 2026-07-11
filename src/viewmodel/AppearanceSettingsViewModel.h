#pragma once

#include "domain/AppearanceSettings.h"

#include <QObject>
#include <QStringList>
#include <QtQmlIntegration/qqmlintegration.h>

namespace smartmate::model {
class AppearanceSettingsService;
}

namespace smartmate::viewmodel {

/// 将外观偏好投影为有限索引并即时保存；不暴露 QSettings。
class AppearanceSettingsViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(int accentThemeIndex READ accentThemeIndex WRITE setAccentThemeIndex
                   NOTIFY appearanceChanged)
    Q_PROPERTY(int fontFamilyIndex READ fontFamilyIndex WRITE setFontFamilyIndex
                   NOTIFY appearanceChanged)
    Q_PROPERTY(int fontScaleIndex READ fontScaleIndex WRITE setFontScaleIndex
                   NOTIFY appearanceChanged)
    Q_PROPERTY(QStringList accentThemeOptions READ accentThemeOptions CONSTANT)
    Q_PROPERTY(QStringList fontFamilyOptions READ fontFamilyOptions CONSTANT)
    Q_PROPERTY(QStringList fontScaleOptions READ fontScaleOptions CONSTANT)
    Q_PROPERTY(QString fontFamilyName READ fontFamilyName NOTIFY appearanceChanged)
    Q_PROPERTY(qreal fontScale READ fontScale NOTIFY appearanceChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    QML_NAMED_ELEMENT(AppearanceSettingsViewModel)
    QML_UNCREATABLE("AppearanceSettingsViewModel is owned by AppViewModel")

public:
    explicit AppearanceSettingsViewModel(QObject *parent = nullptr);
    explicit AppearanceSettingsViewModel(model::AppearanceSettingsService &service,
                                         QObject *parent = nullptr);

    [[nodiscard]] int accentThemeIndex() const noexcept;
    [[nodiscard]] int fontFamilyIndex() const noexcept;
    [[nodiscard]] int fontScaleIndex() const noexcept;
    [[nodiscard]] QStringList accentThemeOptions() const;
    [[nodiscard]] QStringList fontFamilyOptions() const;
    [[nodiscard]] QStringList fontScaleOptions() const;
    [[nodiscard]] QString fontFamilyName() const;
    [[nodiscard]] qreal fontScale() const noexcept;
    [[nodiscard]] QString errorMessage() const;

    void setAccentThemeIndex(int index);
    void setFontFamilyIndex(int index);
    void setFontScaleIndex(int index);
    Q_INVOKABLE void resetDefaults();
    Q_INVOKABLE void clearError();

signals:
    void appearanceChanged();
    void errorMessageChanged();
    void errorOccurred(const QString &message);

private:
    void load();
    void apply(const model::AppearanceSettings &settings);
    void saveCandidate(const model::AppearanceSettings &candidate);
    void setError(const QString &message);

    /// 测试可不注入服务而使用会话默认值；生产环境始终注入。
    model::AppearanceSettingsService *m_service{nullptr};
    model::AppearanceSettings m_settings;
    QString m_errorMessage;
};

} // namespace smartmate::viewmodel
