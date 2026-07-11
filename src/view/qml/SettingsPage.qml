import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SmartMate.ViewModel 1.0

Page {
    id: root
    required property AppViewModel appViewModel
    required property AppearanceTheme theme

    background: Rectangle { color: root.theme.background }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: root.theme.px(18)

            Label {
                text: qsTr("设置")
                color: root.theme.textPrimary
                font.pixelSize: root.theme.px(28)
                font.bold: true
            }
            Label {
                text: qsTr("调整 SmartMate 的强调色和界面字体。")
                color: root.theme.textSecondary
                font.pixelSize: root.theme.px(14)
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.maximumWidth: 760
                implicitHeight: settingsContent.implicitHeight + root.theme.px(40)
                radius: 12
                color: root.theme.surface
                border.color: root.theme.borderSoft

                ColumnLayout {
                    id: settingsContent
                    anchors.fill: parent
                    anchors.margins: root.theme.px(20)
                    spacing: root.theme.px(18)

                    Label {
                        text: qsTr("外观")
                        color: root.theme.textPrimary
                        font.pixelSize: root.theme.px(18)
                        font.bold: true
                    }
                    Label { text: qsTr("强调颜色"); color: root.theme.textBody; font.bold: true }
                    RowLayout {
                        spacing: root.theme.px(12)
                        Button {
                            objectName: "accentThemeButton_0"
                            checkable: true
                            checked: root.appViewModel.appearanceSettings.accentThemeIndex === 0
                            text: qsTr("青绿清新")
                            onClicked: root.appViewModel.appearanceSettings.accentThemeIndex = 0
                        }
                        Button {
                            objectName: "accentThemeButton_1"
                            checkable: true
                            checked: root.appViewModel.appearanceSettings.accentThemeIndex === 1
                            text: qsTr("清蓝专注")
                            onClicked: root.appViewModel.appearanceSettings.accentThemeIndex = 1
                        }
                    }

                    Label { text: qsTr("界面字体"); color: root.theme.textBody; font.bold: true }
                    ComboBox {
                        objectName: "fontFamilyComboBox"
                        Layout.preferredWidth: 280
                        model: root.appViewModel.appearanceSettings.fontFamilyOptions
                        currentIndex: root.appViewModel.appearanceSettings.fontFamilyIndex
                        onActivated: index => root.appViewModel.appearanceSettings.fontFamilyIndex = index
                    }

                    Label { text: qsTr("字体大小"); color: root.theme.textBody; font.bold: true }
                    RowLayout {
                        spacing: 6
                        Button { objectName: "fontScaleButton_0"; checkable: true; checked: root.appViewModel.appearanceSettings.fontScaleIndex === 0; text: qsTr("较小"); onClicked: root.appViewModel.appearanceSettings.fontScaleIndex = 0 }
                        Button { objectName: "fontScaleButton_1"; checkable: true; checked: root.appViewModel.appearanceSettings.fontScaleIndex === 1; text: qsTr("标准"); onClicked: root.appViewModel.appearanceSettings.fontScaleIndex = 1 }
                        Button { objectName: "fontScaleButton_2"; checkable: true; checked: root.appViewModel.appearanceSettings.fontScaleIndex === 2; text: qsTr("较大"); onClicked: root.appViewModel.appearanceSettings.fontScaleIndex = 2 }
                    }

                    Label { text: qsTr("预览"); color: root.theme.textBody; font.bold: true }
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: root.theme.px(116)
                        radius: 10
                        color: root.theme.surfaceSubtle
                        border.color: root.theme.borderSoft
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            Label {
                                text: qsTr("完成 SmartMate 主窗口设计")
                                color: root.theme.textPrimary
                                font.pixelSize: root.theme.px(17)
                                font.bold: true
                            }
                            Label {
                                text: qsTr("保持界面清新、清晰，并突出当前最值得做的任务。")
                                color: root.theme.textSecondary
                                font.pixelSize: root.theme.px(14)
                            }
                            Label {
                                text: qsTr("进行中 · 今天 18:00")
                                color: root.theme.inProgress
                                font.pixelSize: root.theme.px(13)
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Item { Layout.fillWidth: true }
                        Button {
                            objectName: "resetAppearanceButton"
                            text: qsTr("恢复默认")
                            onClicked: root.appViewModel.appearanceSettings.resetDefaults()
                        }
                    }
                    Label {
                        visible: text.length > 0
                        text: root.appViewModel.appearanceSettings.errorMessage
                        color: root.theme.danger
                        wrapMode: Text.Wrap
                    }
                }
            }
            Item { Layout.fillHeight: true }
        }
    }
}
