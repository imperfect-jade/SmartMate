import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SmartMate.ViewModel 1.0

// 表单只绑定ViewModel草稿；分区与响应式换行纯属View展示职责。
Dialog {
    id: root
    objectName: "taskEditorDialog"
    required property TaskEditorViewModel editor
    required property AppearanceTheme theme
    signal manageCategoriesRequested()

    readonly property bool narrowForm: width < theme.px(610)

    function openDeadlinePicker() {
        if (root.editor.hasDeadline) {
            deadlinePicker.openWithValues(root.editor.deadlineYear,
                                          root.editor.deadlineMonth,
                                          root.editor.deadlineDay,
                                          root.editor.deadlineHour,
                                          root.editor.deadlineMinute)
            return
        }
        const tomorrow = new Date()
        tomorrow.setDate(tomorrow.getDate() + 1)
        deadlinePicker.openWithValues(tomorrow.getFullYear(), tomorrow.getMonth() + 1,
                                      tomorrow.getDate(), tomorrow.getHours(),
                                      tomorrow.getMinutes())
    }

    function openDurationPicker() {
        if (root.editor.hasEstimatedDuration) {
            durationPicker.openWithValues(root.editor.estimatedDays,
                                          root.editor.estimatedHours,
                                          root.editor.estimatedMinutePart)
            return
        }
        durationPicker.openWithValues(0, 0, 30)
    }

    width: Math.max(theme.px(430),
                    Math.min(theme.px(700), parent ? parent.width - theme.px(48) : theme.px(700)))
    height: Math.max(theme.px(480),
                     Math.min(theme.px(720), parent ? parent.height - theme.px(48) : theme.px(720)))
    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose
    title: editor.editMode ? qsTr("编辑任务") : qsTr("新建任务")

    background: Rectangle {
        radius: 14
        color: root.theme.surfaceElevated
        border.color: root.theme.border
    }

    header: Rectangle {
        implicitHeight: root.theme.px(66)
        color: root.theme.surfaceStrong
        radius: 14
        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 14; color: parent.color
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: root.theme.px(22)
            anchors.rightMargin: root.theme.px(22)
            Label {
                text: root.title
                color: root.theme.textPrimary
                font.pixelSize: root.theme.px(21)
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Label {
                text: root.editor.editMode ? qsTr("修改任务信息") : qsTr("创建后状态固定为待办")
                color: root.theme.textSecondary
                font.pixelSize: root.theme.px(12)
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignRight
                Layout.maximumWidth: root.theme.px(240)
            }
        }
    }

    contentItem: ScrollView {
        id: editorScroll
        objectName: "taskEditorScrollView"
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: editorContent
            objectName: "taskEditorContent"
            width: editorScroll.availableWidth
            spacing: root.theme.px(14)

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: basicSection.implicitHeight + root.theme.px(32)
                radius: 11
                color: root.theme.surface
                border.color: root.theme.borderSoft
                ColumnLayout {
                    id: basicSection
                    anchors.fill: parent
                    anchors.margins: root.theme.px(16)
                    spacing: root.theme.px(9)
                    Label { text: qsTr("基本信息"); color: root.theme.primary; font.bold: true; font.pixelSize: root.theme.px(16) }
                    Label { text: qsTr("标题"); color: root.theme.textBody; font.bold: true }
                    TextField {
                        objectName: "taskTitleField"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: root.editor.title
                        placeholderText: qsTr("例如：完成 MVVM 架构图")
                        selectByMouse: true
                        onTextEdited: root.editor.title = text
                    }
                    Label { text: qsTr("描述"); color: root.theme.textBody; font.bold: true }
                    ScrollView {
                        id: descriptionScroll
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.theme.px(116)
                        contentWidth: availableWidth
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        TextArea {
                            objectName: "taskDescriptionArea"
                            width: descriptionScroll.availableWidth
                            text: root.editor.description
                            placeholderText: qsTr("可选：记录任务的具体内容")
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            onTextChanged: root.editor.description = text
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: planningSection.implicitHeight + root.theme.px(32)
                radius: 11
                color: root.theme.surface
                border.color: root.theme.borderSoft
                ColumnLayout {
                    id: planningSection
                    anchors.fill: parent
                    anchors.margins: root.theme.px(16)
                    spacing: root.theme.px(10)
                    Label { text: qsTr("任务规划"); color: root.theme.primary; font.bold: true; font.pixelSize: root.theme.px(16) }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: root.narrowForm ? 1 : 2
                        columnSpacing: root.theme.px(14)
                        rowSpacing: root.theme.px(10)
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { text: qsTr("状态"); color: root.theme.textBody; font.bold: true }
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: root.theme.px(42)
                                radius: 8
                                color: root.theme.surfaceStrong
                                border.color: root.theme.border
                                Label {
                                    objectName: "taskCurrentStatusLabel"
                                    anchors.fill: parent
                                    anchors.leftMargin: 12; anchors.rightMargin: 12
                                    verticalAlignment: Text.AlignVCenter
                                    text: root.editor.editMode ? root.editor.currentStatusText
                                                               : qsTr("初始状态：待办")
                                    color: root.theme.textSecondary
                                    elide: Text.ElideRight
                                }
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { text: qsTr("优先级"); color: root.theme.textBody; font.bold: true }
                            ComboBox {
                                Layout.fillWidth: true
                                model: root.editor.priorityOptions
                                currentIndex: root.editor.priorityIndex
                                onActivated: index => root.editor.priorityIndex = index
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.columnSpan: root.narrowForm ? 1 : 2
                            Label {
                                text: qsTr("类别")
                                color: root.theme.textBody
                                font.bold: true
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: root.theme.px(8)
                                ComboBox {
                                    id: taskCategoryComboBox
                                    objectName: "taskCategoryComboBox"
                                    Layout.fillWidth: true
                                    model: root.editor.categoryOptions
                                    textRole: "name"
                                    valueRole: "categoryId"
                                    currentIndex: taskCategoryComboBox.indexOfValue(
                                                      root.editor.selectedCategoryId)
                                    onActivated: root.editor.selectedCategoryId = currentValue
                                }
                                Button {
                                    objectName: "manageCategoriesFromEditorButton"
                                    text: qsTr("管理类别")
                                    onClicked: root.manageCategoriesRequested()
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: scheduleSection.implicitHeight + root.theme.px(32)
                radius: 11
                color: root.theme.surface
                border.color: root.theme.borderSoft
                ColumnLayout {
                    id: scheduleSection
                    anchors.fill: parent
                    anchors.margins: root.theme.px(16)
                    spacing: root.theme.px(10)
                    Label { text: qsTr("时间与依赖"); color: root.theme.primary; font.bold: true; font.pixelSize: root.theme.px(16) }

                    Label { visible: root.editor.canConfigurePredecessors; text: qsTr("前置任务"); color: root.theme.textBody; font.bold: true }
                    RowLayout {
                        Layout.fillWidth: true
                        visible: root.editor.canConfigurePredecessors
                        spacing: 8
                        Rectangle {
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            implicitHeight: root.theme.px(42); radius: 8
                            color: root.theme.inputBackground; border.color: root.theme.border
                            Label {
                                anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12
                                verticalAlignment: Text.AlignVCenter
                                text: root.editor.predecessorSummaryText
                                color: root.editor.selectedPredecessorCount > 0
                                       ? root.theme.textPrimary : root.theme.textMuted
                                elide: Text.ElideRight
                            }
                        }
                        Button {
                            objectName: "openCreationPredecessorButton"
                            Layout.minimumWidth: root.theme.px(72)
                            text: root.editor.selectedPredecessorCount > 0 ? qsTr("修改") : qsTr("选择")
                            enabled: root.editor.predecessorCandidateCount > 0
                            onClicked: { root.editor.beginPredecessorSelection(); creationPredecessorDialog.open() }
                        }
                        ToolButton {
                            objectName: "clearCreationPredecessorButton"
                            visible: root.editor.selectedPredecessorCount > 0
                            text: "×"
                            ToolTip.visible: hovered; ToolTip.text: qsTr("清除前置任务")
                            onClicked: root.editor.clearCreationPredecessors()
                        }
                    }

                    Label { text: qsTr("截止时间"); color: root.theme.textBody; font.bold: true }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        Rectangle {
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            implicitHeight: root.theme.px(42); radius: 8
                            color: root.theme.inputBackground; border.color: root.theme.border
                            Label {
                                anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12
                                verticalAlignment: Text.AlignVCenter
                                text: root.editor.deadlineDisplayText
                                color: root.editor.hasDeadline ? root.theme.textPrimary : root.theme.textMuted
                                elide: Text.ElideRight
                            }
                        }
                        Button { objectName: "openDeadlinePickerButton"; Layout.minimumWidth: root.theme.px(72); text: root.editor.hasDeadline ? qsTr("修改") : qsTr("选择"); onClicked: root.openDeadlinePicker() }
                        ToolButton { objectName: "clearDeadlineButton"; visible: root.editor.hasDeadline; text: "×"; ToolTip.visible: hovered; ToolTip.text: qsTr("清除截止时间"); onClicked: root.editor.clearDeadline() }
                    }

                    Label { text: qsTr("预计用时"); color: root.theme.textBody; font.bold: true }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        Rectangle {
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            implicitHeight: root.theme.px(42); radius: 8
                            color: root.theme.inputBackground; border.color: root.theme.border
                            Label {
                                anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12
                                verticalAlignment: Text.AlignVCenter
                                text: root.editor.estimatedDurationDisplayText
                                color: root.editor.hasEstimatedDuration ? root.theme.textPrimary : root.theme.textMuted
                                elide: Text.ElideRight
                            }
                        }
                        Button { objectName: "openDurationPickerButton"; Layout.minimumWidth: root.theme.px(72); text: root.editor.hasEstimatedDuration ? qsTr("修改") : qsTr("选择"); onClicked: root.openDurationPicker() }
                        ToolButton { objectName: "clearDurationButton"; visible: root.editor.hasEstimatedDuration; text: "×"; ToolTip.visible: hovered; ToolTip.text: qsTr("清除预计用时"); onClicked: root.editor.clearEstimatedDuration() }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.editor.errorMessage.length > 0
                      ? root.editor.errorMessage : root.editor.validationMessage
                color: root.theme.danger
                wrapMode: Text.Wrap
            }
        }
    }

    footer: Rectangle {
        implicitHeight: root.theme.px(64)
        color: root.theme.surfaceStrong
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: root.theme.px(20)
            anchors.rightMargin: root.theme.px(20)
            Item { Layout.fillWidth: true }
            Button { text: qsTr("取消"); onClicked: { root.editor.cancel(); root.close() } }
            Button { objectName: "saveTaskButton"; text: qsTr("保存"); enabled: root.editor.canSave; onClicked: { if (root.editor.save()) root.close() } }
        }
    }

    DateTimePickerDialog {
        id: deadlinePicker
        objectName: "deadlinePickerDialog"
        anchors.centerIn: Overlay.overlay
        theme: root.theme
        onSelectionAccepted: (year, month, day, hour, minute) =>
            root.editor.setDeadlineSelection(year, month, day, hour, minute)
    }
    DurationPickerDialog {
        id: durationPicker
        objectName: "durationPickerDialog"
        anchors.centerIn: Overlay.overlay
        theme: root.theme
        minimumMinutes: root.editor.minimumEstimatedMinutes
        maximumMinutes: root.editor.maximumEstimatedMinutes
        onSelectionAccepted: (days, hours, minutes) =>
            root.editor.setEstimatedDuration(days, hours, minutes)
    }
    TaskCreationPredecessorDialog {
        id: creationPredecessorDialog
        anchors.centerIn: Overlay.overlay
        editor: root.editor
        theme: root.theme
    }
}
