#include "view/widgets/settings/SettingsPage.h"

#include "view/widgets/binding/WidgetBinding.h"
#include "viewmodel/contracts/AppearanceSettingsContract.h"
#include "viewmodel/contracts/DesktopPetSettingsContract.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QCheckBox>
#include <QFrame>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace smartmate::view::widgets {
namespace {

QLabel *createLabel(const QString &text, const char *objectName = nullptr)
{
    auto *label = new QLabel{text};
    if (objectName != nullptr) {
        label->setObjectName(QString::fromLatin1(objectName));
    }
    label->setWordWrap(true);
    // 自动换行标签必须向父布局报告真实高度，避免窄宽度或大字号时覆盖后续控件。
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    return label;
}

QPushButton *addIndexedButton(QButtonGroup &group,
                              QHBoxLayout &layout,
                              const QString &text,
                              const QString &objectName,
                              const int index)
{
    auto *button = new QPushButton{text};
    button->setObjectName(objectName);
    button->setCheckable(true);
    group.addButton(button, index);
    layout.addWidget(button);
    return button;
}

} // namespace

SettingsPage::SettingsPage(viewmodel::AppearanceSettingsContract &settings,
                           QWidget *parent)
    : SettingsPage(settings, nullptr, parent)
{
}

SettingsPage::SettingsPage(
    viewmodel::AppearanceSettingsContract &settings,
    viewmodel::DesktopPetSettingsContract &desktopPetSettings,
    QWidget *parent)
    : SettingsPage(settings, &desktopPetSettings, parent)
{
}

SettingsPage::SettingsPage(
    viewmodel::AppearanceSettingsContract &settings,
    viewmodel::DesktopPetSettingsContract *desktopPetSettings,
    QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_desktopPetSettings(desktopPetSettings)
    , m_accentButtons(new QButtonGroup(this))
    , m_fontFamilyComboBox(new QComboBox(this))
    , m_fontScaleButtons(new QButtonGroup(this))
    , m_desktopPetCheckBox(nullptr)
{
    setObjectName(QStringLiteral("settingsPage"));
    m_accentButtons->setExclusive(true);
    m_fontScaleButtons->setExclusive(true);

    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(24, 24, 24, 24);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    pageLayout->addWidget(scrollArea);

    auto *content = new QWidget;
    content->setObjectName(QStringLiteral("pageSurface"));
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(8, 4, 8, 24);
    contentLayout->setSpacing(14);
    scrollArea->setWidget(content);

    contentLayout->addWidget(createLabel(tr("设置"), "pageTitle"));
    contentLayout->addWidget(
        createLabel(tr("调整 SmartMate 的强调色和界面字体。"), "secondaryText"));

    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("settingsCard"));
    card->setMaximumWidth(760);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 20, 20, 20);
    cardLayout->setSpacing(12);
    contentLayout->addWidget(card);

    cardLayout->addWidget(createLabel(tr("外观"), "sectionTitle"));
    cardLayout->addWidget(createLabel(tr("强调颜色")));
    auto *accentLayout = new QHBoxLayout;
    const QStringList accentOptions = settings.accentThemeOptions();
    for (int index = 0; index < accentOptions.size(); ++index) {
        addIndexedButton(*m_accentButtons, *accentLayout, accentOptions.at(index),
                         QStringLiteral("accentThemeButton_%1").arg(index), index);
    }
    accentLayout->addStretch();
    cardLayout->addLayout(accentLayout);

    cardLayout->addWidget(createLabel(tr("界面字体")));
    m_fontFamilyComboBox->setObjectName(QStringLiteral("fontFamilyComboBox"));
    m_fontFamilyComboBox->addItems(settings.fontFamilyOptions());
    m_fontFamilyComboBox->setMaximumWidth(280);
    cardLayout->addWidget(m_fontFamilyComboBox);

    cardLayout->addWidget(createLabel(tr("字体大小")));
    auto *scaleLayout = new QHBoxLayout;
    const QStringList scaleOptions = settings.fontScaleOptions();
    for (int index = 0; index < scaleOptions.size(); ++index) {
        addIndexedButton(*m_fontScaleButtons, *scaleLayout, scaleOptions.at(index),
                         QStringLiteral("fontScaleButton_%1").arg(index), index);
    }
    scaleLayout->addStretch();
    cardLayout->addLayout(scaleLayout);

    cardLayout->addWidget(createLabel(tr("预览")));
    auto *preview = new QFrame;
    preview->setObjectName(QStringLiteral("previewCard"));
    preview->setMinimumHeight(116);
    preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto *previewLayout = new QVBoxLayout(preview);
    previewLayout->setContentsMargins(16, 14, 16, 14);
    previewLayout->setSpacing(6);
    previewLayout->setSizeConstraint(QLayout::SetMinimumSize);
    auto *previewTitle = createLabel(tr("完成 SmartMate 主窗口设计"),
                                     "settingsPreviewTitle");
    auto *previewDescription = createLabel(
        tr("保持界面清新、清晰，并突出当前最值得做的任务。"),
        "settingsPreviewDescription");
    auto *previewStatus = createLabel(tr("进行中 · 今天 18:00"),
                                      "previewStatus");
    for (QLabel *label : {previewTitle, previewDescription, previewStatus}) {
        label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    }
    previewLayout->addWidget(previewTitle);
    previewLayout->addWidget(previewDescription);
    previewLayout->addWidget(previewStatus);
    cardLayout->addWidget(preview);

    auto *resetLayout = new QHBoxLayout;
    resetLayout->addStretch();
    auto *resetButton = new QPushButton{tr("恢复默认")};
    resetButton->setObjectName(QStringLiteral("resetAppearanceButton"));
    resetLayout->addWidget(resetButton);
    cardLayout->addLayout(resetLayout);

    if (m_desktopPetSettings != nullptr) {
        auto *petCard = new QFrame;
        petCard->setObjectName(QStringLiteral("desktopPetSettingsCard"));
        petCard->setMaximumWidth(760);
        petCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        auto *petLayout = new QVBoxLayout(petCard);
        petLayout->setContentsMargins(20, 20, 20, 20);
        petLayout->setSpacing(10);
        petLayout->setSizeConstraint(QLayout::SetMinimumSize);
        petLayout->addWidget(createLabel(tr("桌宠"), "sectionTitle"));
        petLayout->addWidget(createLabel(
            tr("在普通窗口显示趴卧的三花猫，最小化后显示可拖动的悬浮桌宠。"),
            "secondaryText"));
        m_desktopPetCheckBox = new QCheckBox{tr("启用三花猫桌宠"), petCard};
        m_desktopPetCheckBox->setObjectName(
            QStringLiteral("desktopPetEnabledCheckBox"));
        petLayout->addWidget(m_desktopPetCheckBox);
        contentLayout->addWidget(petCard);

        const auto syncPetEnabled = [this] {
            const QSignalBlocker blocker{m_desktopPetCheckBox};
            m_desktopPetCheckBox->setChecked(m_desktopPetSettings->enabled());
        };
        connect(m_desktopPetSettings,
                &viewmodel::DesktopPetSettingsContract::enabledChanged,
                this, syncPetEnabled);
        connect(m_desktopPetCheckBox, &QCheckBox::toggled, this,
                [this](const bool enabled) {
                    m_desktopPetSettings->setEnabled(enabled);
                });
        syncPetEnabled();
    }
    contentLayout->addStretch();

    // 单向绑定会先读取当前 getter，再监听 appearanceChanged；页面打开时不会等待下一次通知。
    binding::bindOneWay(settings,
                        &viewmodel::AppearanceSettingsContract::appearanceChanged,
                        *this,
                        [&settings] { return settings.accentThemeIndex(); },
                        [this](const int index) {
                            binding::setCheckedButton(*m_accentButtons, index);
                        });
    // 只有用户点击才写回 Contract；程序性勾选已由绑定辅助器阻断信号。
    connect(m_accentButtons, &QButtonGroup::idClicked, this,
            [&settings](const int index) { settings.setAccentThemeIndex(index); });

    // 组合框绑定明确区分 activated（用户命令）和 setCurrentIndex（状态回填）。
    binding::bindComboBoxIndex(
        settings, &viewmodel::AppearanceSettingsContract::appearanceChanged,
        *m_fontFamilyComboBox,
        [&settings] { return settings.fontFamilyIndex(); },
        [&settings](const int index) { settings.setFontFamilyIndex(index); });

    binding::bindOneWay(settings,
                        &viewmodel::AppearanceSettingsContract::appearanceChanged,
                        *this,
                        [&settings] { return settings.fontScaleIndex(); },
                        [this](const int index) {
                            binding::setCheckedButton(*m_fontScaleButtons, index);
                        });
    connect(m_fontScaleButtons, &QButtonGroup::idClicked, this,
            [&settings](const int index) { settings.setFontScaleIndex(index); });
    // 恢复默认是语义命令，具体默认值与持久化顺序均由 Model/ViewModel 决定。
    connect(resetButton, &QPushButton::clicked, this,
            [&settings] { settings.resetDefaults(); });
}

} // namespace smartmate::view::widgets
