import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SmartMate.ViewModel 1.0

// 表单只绑定 ViewModel 草稿并转发输入；校验规则和保存结果不在 QML 中判断。
Dialog {
    id: root
    objectName: "taskEditorDialog"

    required property TaskEditorViewModel editor

    function openDeadlinePicker() {
        if (root.editor.hasDeadline) {
            deadlinePicker.openWithValues(root.editor.deadlineYear,
                                          root.editor.deadlineMonth,
                                          root.editor.deadlineDay,
                                          root.editor.deadlineHour,
                                          root.editor.deadlineMinute)
            return
        }

        // 未设置时只在 View 中准备“明天此刻”的候选值，用户确认后才写入草稿。
        const tomorrow = new Date()
        tomorrow.setDate(tomorrow.getDate() + 1)
        deadlinePicker.openWithValues(tomorrow.getFullYear(),
                                      tomorrow.getMonth() + 1,
                                      tomorrow.getDate(),
                                      tomorrow.getHours(),
                                      tomorrow.getMinutes())
    }

    function openDurationPicker() {
        if (root.editor.hasEstimatedDuration) {
            durationPicker.openWithValues(root.editor.estimatedDays,
                                          root.editor.estimatedHours,
                                          root.editor.estimatedMinutePart)
            return
        }

        // 新时长以30分钟作为候选值；打开选择器本身不会改变ViewModel。
        durationPicker.openWithValues(0, 0, 30)
    }

    width: 560
    height: Math.min(680, parent ? parent.height - 40 : 680)
    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose
    title: editor.editMode ? qsTr("编辑任务") : qsTr("新建任务")

    // 所有控件写回草稿属性，派生的错误文本和 canSave 再通过绑定返回 View。
    contentItem: ScrollView {
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 12

            Label {
                text: qsTr("标题")
                font.bold: true
            }

            TextField {
                Layout.fillWidth: true
                text: root.editor.title
                placeholderText: qsTr("例如：完成 MVVM 架构图")
                onTextEdited: root.editor.title = text
            }

            Label {
                text: qsTr("描述")
                font.bold: true
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 100

                TextArea {
                    text: root.editor.description
                    placeholderText: qsTr("可选：记录任务的具体内容")
                    wrapMode: TextEdit.Wrap
                    onTextChanged: root.editor.description = text
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                ColumnLayout {
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("状态")
                        font.bold: true
                    }

                    ComboBox {
                        Layout.fillWidth: true
                        model: root.editor.statusOptions
                        currentIndex: root.editor.statusIndex
                        onActivated: index => root.editor.statusIndex = index
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("优先级")
                        font.bold: true
                    }

                    ComboBox {
                        Layout.fillWidth: true
                        model: root.editor.priorityOptions
                        currentIndex: root.editor.priorityIndex
                        onActivated: index => root.editor.priorityIndex = index
                    }
                }
            }

            // 只有新建模式显示前置草稿；编辑已保存任务仍使用独立的依赖编辑器。
            Label {
                visible: root.editor.canConfigurePredecessors
                text: qsTr("前置任务")
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true
                visible: root.editor.canConfigurePredecessors
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 40
                    radius: 4
                    color: "#f9fafb"
                    border.color: "#d0d5dd"

                    Label {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        verticalAlignment: Text.AlignVCenter
                        text: root.editor.predecessorSummaryText
                        color: root.editor.selectedPredecessorCount > 0
                               ? "#172033" : "#667085"
                        elide: Text.ElideRight
                    }
                }

                Button {
                    objectName: "openCreationPredecessorButton"
                    text: root.editor.selectedPredecessorCount > 0
                          ? qsTr("修改") : qsTr("选择")
                    enabled: root.editor.predecessorCandidateCount > 0
                    onClicked: {
                        root.editor.beginPredecessorSelection()
                        creationPredecessorDialog.open()
                    }
                }

                Button {
                    objectName: "clearCreationPredecessorButton"
                    visible: root.editor.selectedPredecessorCount > 0
                    text: qsTr("清除")
                    onClicked: root.editor.clearCreationPredecessors()
                }
            }

            Label {
                text: qsTr("截止时间")
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 40
                    radius: 4
                    color: "#f9fafb"
                    border.color: "#d0d5dd"

                    Label {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        verticalAlignment: Text.AlignVCenter
                        text: root.editor.deadlineDisplayText
                        color: root.editor.hasDeadline ? "#172033" : "#667085"
                        elide: Text.ElideRight
                    }
                }

                Button {
                    objectName: "openDeadlinePickerButton"
                    text: root.editor.hasDeadline ? qsTr("修改") : qsTr("选择")
                    onClicked: root.openDeadlinePicker()
                }

                Button {
                    objectName: "clearDeadlineButton"
                    visible: root.editor.hasDeadline
                    text: qsTr("清除")
                    onClicked: root.editor.clearDeadline()
                }
            }

            Label {
                text: qsTr("预计用时")
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 40
                    radius: 4
                    color: "#f9fafb"
                    border.color: "#d0d5dd"

                    Label {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        verticalAlignment: Text.AlignVCenter
                        text: root.editor.estimatedDurationDisplayText
                        color: root.editor.hasEstimatedDuration ? "#172033" : "#667085"
                        elide: Text.ElideRight
                    }
                }

                Button {
                    objectName: "openDurationPickerButton"
                    text: root.editor.hasEstimatedDuration ? qsTr("修改") : qsTr("选择")
                    onClicked: root.openDurationPicker()
                }

                Button {
                    objectName: "clearDurationButton"
                    visible: root.editor.hasEstimatedDuration
                    text: qsTr("清除")
                    onClicked: root.editor.clearEstimatedDuration()
                }
            }

            Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.editor.errorMessage.length > 0
                      ? root.editor.errorMessage
                      : root.editor.validationMessage
                color: "#b42318"
                wrapMode: Text.Wrap
            }
        }
    }

    // 页脚只转发保存/取消命令；Model 未确认成功时对话框保持打开。
    footer: Rectangle {
        implicitHeight: 62
        color: "#ffffff"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("取消")
                onClicked: {
                    root.editor.cancel()
                    root.close()
                }
            }

            Button {
                text: qsTr("保存")
                enabled: root.editor.canSave
                onClicked: {
                    if (root.editor.save())
                        root.close()
                }
            }
        }
    }

    // 两个选择器各自维护临时值；只有确定信号才转发给 ViewModel。
    DateTimePickerDialog {
        id: deadlinePicker
        objectName: "deadlinePickerDialog"
        anchors.centerIn: Overlay.overlay

        onSelectionAccepted: (year, month, day, hour, minute) => {
            root.editor.setDeadlineSelection(year, month, day, hour, minute)
        }
    }

    DurationPickerDialog {
        id: durationPicker
        objectName: "durationPickerDialog"
        anchors.centerIn: Overlay.overlay
        minimumMinutes: root.editor.minimumEstimatedMinutes
        maximumMinutes: root.editor.maximumEstimatedMinutes

        onSelectionAccepted: (days, hours, minutes) => {
            root.editor.setEstimatedDuration(days, hours, minutes)
        }
    }

    TaskCreationPredecessorDialog {
        id: creationPredecessorDialog
        anchors.centerIn: Overlay.overlay
        editor: root.editor
    }
}
