pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 时长使用受限数值控件生成候选值，避免将自由文本或格式解析带入 ViewModel。
Dialog {
    id: root
    required property AppearanceTheme theme

    // 边界由 ViewModel 投影，选择器只据此限制控件，不复制 Model 业务常量。
    property int minimumMinutes: 1
    property int maximumMinutes: 525600
    readonly property int maximumDays: Math.floor(maximumMinutes / (24 * 60))
    property int pendingDays: 0
    property int pendingHours: 0
    property int pendingMinutes: 30

    signal selectionAccepted(int days, int hours, int minutes)

    function minimumMinutesForSelection(days, hours) {
        return days === 0 && hours === 0 ? root.minimumMinutes : 0
    }

    function maximumHoursForDays(days) {
        if (days < root.maximumDays)
            return 23
        return Math.floor((root.maximumMinutes - days * 24 * 60) / 60)
    }

    function maximumMinutesForSelection(days, hours) {
        if (days < root.maximumDays)
            return 59
        return Math.min(59, root.maximumMinutes - days * 24 * 60 - hours * 60)
    }

    function openWithValues(days, hours, minutes) {
        root.pendingDays = Math.max(0, Math.min(root.maximumDays, days))
        root.pendingHours = Math.max(0, Math.min(
                                         root.maximumHoursForDays(root.pendingDays), hours))
        root.pendingMinutes = Math.max(
                    root.minimumMinutesForSelection(root.pendingDays,
                                                    root.pendingHours),
                    Math.min(root.maximumMinutesForSelection(
                                 root.pendingDays, root.pendingHours), minutes))
        root.open()
    }

    onPendingDaysChanged: {
        pendingHours = Math.min(pendingHours, maximumHoursForDays(pendingDays))
        pendingMinutes = Math.max(
                    minimumMinutesForSelection(pendingDays, pendingHours),
                    Math.min(pendingMinutes,
                             maximumMinutesForSelection(pendingDays, pendingHours)))
    }
    onPendingHoursChanged: {
        const allowedHours = maximumHoursForDays(pendingDays)
        if (pendingHours > allowedHours) {
            pendingHours = allowedHours
            return
        }
        pendingMinutes = Math.max(
                    minimumMinutesForSelection(pendingDays, pendingHours),
                    Math.min(pendingMinutes,
                             maximumMinutesForSelection(pendingDays, pendingHours)))
    }
    onPendingMinutesChanged: {
        const requiredMinutes = minimumMinutesForSelection(pendingDays, pendingHours)
        const allowedMinutes = maximumMinutesForSelection(pendingDays, pendingHours)
        if (pendingMinutes < requiredMinutes)
            pendingMinutes = requiredMinutes
        else if (pendingMinutes > allowedMinutes)
            pendingMinutes = allowedMinutes
    }

    width: Math.max(root.theme.px(420),
                    Math.min(root.theme.px(520), parent ? parent.width - root.theme.px(48) : root.theme.px(520)))
    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose
    title: qsTr("选择预计用时")

    background: Rectangle {
        radius: 14
        color: root.theme.surfaceElevated
        border.color: root.theme.border
    }

    onAccepted: root.selectionAccepted(root.pendingDays,
                                       root.pendingHours,
                                       root.pendingMinutes)

    contentItem: ColumnLayout {
        spacing: 16

        Label {
            Layout.fillWidth: true
            text: qsTr("使用按钮选择天、小时和分钟，最短为1分钟。")
            color: root.theme.textSecondary
            wrapMode: Text.Wrap
        }

        GridLayout {
            Layout.fillWidth: true
            columns: root.width < root.theme.px(470) ? 1 : 3
            columnSpacing: 10
            rowSpacing: 10

            ColumnLayout {
                Layout.fillWidth: true

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("天")
                    font.bold: true
                }

                SpinBox {
                    objectName: "durationDaysSpinBox"
                    Layout.fillWidth: true
                    from: 0
                    to: root.maximumDays
                    value: root.pendingDays
                    editable: false
                    onValueModified: root.pendingDays = value
                }
            }

            ColumnLayout {
                Layout.fillWidth: true

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("小时")
                    font.bold: true
                }

                SpinBox {
                    objectName: "durationHoursSpinBox"
                    Layout.fillWidth: true
                    from: 0
                    to: root.maximumHoursForDays(root.pendingDays)
                    value: root.pendingHours
                    editable: false
                    onValueModified: root.pendingHours = value
                }
            }

            ColumnLayout {
                Layout.fillWidth: true

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("分钟")
                    font.bold: true
                }

                SpinBox {
                    objectName: "durationMinutesSpinBox"
                    Layout.fillWidth: true
                    from: root.minimumMinutesForSelection(root.pendingDays,
                                                          root.pendingHours)
                    to: root.maximumMinutesForSelection(root.pendingDays,
                                                        root.pendingHours)
                    value: root.pendingMinutes
                    editable: false
                    onValueModified: root.pendingMinutes = value
                }
            }
        }

    }

    footer: Rectangle {
        implicitHeight: 56
        color: root.theme.surfaceStrong

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("取消")
                onClicked: root.reject()
            }

            Button {
                objectName: "confirmDurationSelectionButton"
                text: qsTr("确定")
                onClicked: root.accept()
            }
        }
    }
}
