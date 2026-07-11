pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SmartMate.ViewModel 1.0

// 根 View 只组合导航与页面；所有业务命令仍经 C++ 管理的 ViewModel 转发。
ApplicationWindow {
    id: root
    required property AppViewModel appViewModel

    width: 1180
    height: 760
    minimumWidth: 900
    minimumHeight: 620
    visible: true
    title: appViewModel.applicationName
    color: theme.background
    font.family: theme.fontFamily
    font.pixelSize: theme.px(14)
    palette.window: theme.background
    palette.windowText: theme.textPrimary
    palette.base: theme.surface
    palette.alternateBase: theme.surfaceSubtle
    palette.text: theme.textBody
    palette.button: theme.surfaceStrong
    palette.buttonText: theme.textPrimary
    palette.light: theme.surfaceElevated
    palette.midlight: theme.borderSoft
    palette.mid: theme.border
    palette.dark: theme.borderStrong
    palette.placeholderText: theme.textMuted
    palette.link: theme.primary
    palette.highlight: theme.primary
    palette.highlightedText: "#ffffff"

    readonly property bool compactNavigation: width < 1040

    AppearanceTheme {
        id: theme
        settings: root.appViewModel.appearanceSettings
    }

    function openDependencyEditor(taskId) {
        if (root.appViewModel.taskDependencies.beginEdit(taskId)) {
            dependencyDialog.open()
        } else {
            dependencyErrorDialog.message = root.appViewModel.taskDependencies.errorMessage
            dependencyErrorDialog.open()
        }
    }

    component NavigationButton: Button {
        id: navButton
        required property int pageIndex
        property string glyph
        property bool comingSoon: false
        checkable: enabled
        checked: enabled && navigationTabs.currentIndex === pageIndex
        Layout.fillWidth: true
        Layout.preferredHeight: theme.px(46)
        leftPadding: root.compactNavigation ? 0 : 14
        rightPadding: root.compactNavigation ? 0 : 10
        onClicked: navigationTabs.currentIndex = pageIndex
        background: Rectangle {
            radius: 9
            color: navButton.checked ? theme.primarySoft
                                     : navButton.hovered ? theme.surfaceSubtle : "transparent"
            Rectangle {
                visible: navButton.checked
                width: 3; height: parent.height - 16; radius: 2
                anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                color: theme.primary
            }
        }
        contentItem: RowLayout {
            spacing: 12
            Label {
                Layout.preferredWidth: root.compactNavigation ? parent.width : 22
                horizontalAlignment: Text.AlignHCenter
                text: navButton.glyph
                color: navButton.enabled
                       ? (navButton.checked ? theme.primary : theme.textSecondary)
                       : theme.textMuted
                font.pixelSize: theme.px(18)
            }
            Label {
                visible: !root.compactNavigation
                Layout.fillWidth: true
                text: navButton.text
                color: navButton.enabled
                       ? (navButton.checked ? theme.primary : theme.textBody)
                       : theme.textMuted
                font.bold: navButton.checked
            }
            Label {
                visible: !root.compactNavigation && navButton.comingSoon
                text: qsTr("即将推出")
                color: theme.textMuted
                font.pixelSize: theme.px(11)
            }
        }
        ToolTip.visible: root.compactNavigation && hovered
        ToolTip.text: comingSoon ? qsTr("%1（即将推出）").arg(text) : text
        ToolTip.delay: 350
    }

    // 保留稳定 objectName，现有图页测试与页面切换仍使用同一导航索引。
    TabBar {
        id: navigationTabs
        objectName: "mainNavigationTabs"
        visible: false
        TabButton { text: qsTr("任务") }
        TabButton { text: qsTr("依赖图") }
        TabButton { text: qsTr("设置") }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: root.compactNavigation ? 64 : 208
            color: theme.navigation
            border.color: theme.borderSoft

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: root.compactNavigation ? 8 : 12
                spacing: 6

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: theme.px(64)
                    RowLayout {
                        anchors.fill: parent
                        spacing: 10
                        Rectangle {
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            radius: 12
                            color: theme.primary
                            Label {
                                anchors.centerIn: parent
                                text: "S"
                                color: "white"
                                font.bold: true
                                font.pixelSize: theme.px(19)
                            }
                        }
                        Label {
                            visible: !root.compactNavigation
                            text: root.appViewModel.applicationName
                            color: theme.textPrimary
                            font.pixelSize: theme.px(20)
                            font.bold: true
                        }
                    }
                }

                NavigationButton {
                    objectName: "taskNavigationButton"
                    pageIndex: 0; glyph: "✓"; text: qsTr("任务")
                }
                NavigationButton {
                    objectName: "graphNavigationButton"
                    pageIndex: 1; glyph: "↗"; text: qsTr("依赖图")
                }
                NavigationButton {
                    objectName: "focusNavigationButton"
                    pageIndex: -1; glyph: "◷"; text: qsTr("专注")
                    enabled: false; comingSoon: true
                }
                NavigationButton {
                    objectName: "statisticsNavigationButton"
                    pageIndex: -1; glyph: "▥"; text: qsTr("统计")
                    enabled: false; comingSoon: true
                }

                Item { Layout.fillHeight: true }

                NavigationButton {
                    objectName: "settingsNavigationButton"
                    pageIndex: 2; glyph: "⚙"; text: qsTr("设置")
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: navigationTabs.currentIndex

            TaskPage {
                appViewModel: root.appViewModel
                theme: theme
                onEditDependenciesRequested: taskId => root.openDependencyEditor(taskId)
                onShowDependencyGraphRequested: navigationTabs.currentIndex = 1
            }

            DependencyGraphPage {
                appViewModel: root.appViewModel
                theme: theme
                onEditDependenciesRequested: taskId => root.openDependencyEditor(taskId)
            }

            Item {
                SettingsPage {
                    anchors.fill: parent
                    anchors.margins: theme.px(28)
                    appViewModel: root.appViewModel
                    theme: theme
                }
            }
        }
    }

    TaskDependencyDialog {
        id: dependencyDialog
        objectName: "taskDependencyDialog"
        anchors.centerIn: Overlay.overlay
        editor: root.appViewModel.taskDependencies
        theme: theme
    }

    Dialog {
        id: dependencyErrorDialog
        property string message
        anchors.centerIn: Overlay.overlay
        title: qsTr("无法编辑依赖")
        modal: true
        standardButtons: Dialog.Ok
        Label { width: 360; wrapMode: Text.Wrap; text: dependencyErrorDialog.message }
        onClosed: root.appViewModel.taskDependencies.clearError()
    }
}
