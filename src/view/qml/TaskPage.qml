pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SmartMate.ViewModel 1.0

Page {
    id: root
    required property AppViewModel appViewModel
    required property AppearanceTheme theme
    signal editDependenciesRequested(string taskId)
    signal showDependencyGraphRequested()
    property bool dragActive: false

    function openEditor(taskId) {
        if (root.appViewModel.taskEditor.beginEdit(taskId)) {
            editorDialog.open()
        } else {
            errorDialog.message = root.appViewModel.taskEditor.errorMessage
            errorDialog.open()
        }
    }

    function openDetails(taskId) {
        if (root.appViewModel.taskList.selectTask(taskId))
            detailsDialog.open()
    }

    function beginCreate() {
        if (root.appViewModel.taskEditor.beginCreate()) {
            editorDialog.open()
        } else {
            errorDialog.message = root.appViewModel.taskEditor.errorMessage
            errorDialog.open()
        }
    }

    background: Rectangle { color: root.theme.background }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.theme.px(24)
        spacing: root.theme.px(14)

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("任务")
                color: root.theme.textPrimary
                font.pixelSize: root.theme.px(26)
                font.bold: true
            }
            Label {
                text: qsTr("找到现在最值得完成的事情")
                color: root.theme.textMuted
                font.pixelSize: root.theme.px(13)
            }
            Item { Layout.fillWidth: true }
        }

        // 任务槽始终基于未筛选计划，拖入只转发稳定 TaskId 的开始命令。
        Rectangle {
            id: focusSlot
            objectName: "focusTaskSlot"
            Layout.fillWidth: true
            Layout.preferredHeight: root.theme.px(148)
            radius: 14
            color: focusDrop.containsDrag ? root.theme.primarySoft
                  : root.appViewModel.taskList.focusState === TaskListViewModel.InProgress
                    ? root.theme.primarySoft : root.theme.surface
            border.width: focusDrop.containsDrag ? 2 : 1
            border.color: focusDrop.containsDrag ? root.theme.primary : root.theme.borderSoft

            Behavior on color { ColorAnimation { duration: 180 } }

            DropArea {
                id: focusDrop
                objectName: "focusTaskDropArea"
                anchors.fill: parent
                keys: ["smartmate-start-task"]
                enabled: root.appViewModel.taskList.focusState !== TaskListViewModel.InProgress
                onDropped: drop => {
                    if (drop.source !== dragPreview || dragPreview.taskId.length === 0)
                        return
                    const droppedTaskId = dragPreview.taskId
                    drop.acceptProposedAction()
                    // Drop事件完成后再执行命令，避免模型重置销毁仍参与拖动的delegate。
                    Qt.callLater(() => root.appViewModel.taskList.startTask(droppedTaskId))
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: root.theme.px(18)
                spacing: root.theme.px(18)

                Rectangle {
                    Layout.preferredWidth: root.theme.px(48)
                    Layout.preferredHeight: root.theme.px(48)
                    radius: 14
                    color: root.theme.primary
                    Label {
                        anchors.centerIn: parent
                        text: root.appViewModel.taskList.focusState === TaskListViewModel.InProgress
                              ? "▶" : root.appViewModel.taskList.focusState === TaskListViewModel.AllBlocked
                                ? "⌁" : "+"
                        color: "white"
                        font.pixelSize: root.theme.px(20)
                        font.bold: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5
                    Label {
                        text: root.appViewModel.taskList.focusState === TaskListViewModel.InProgress
                              ? qsTr("现在做 · 正在进行") : qsTr("现在做")
                        color: root.theme.primary
                        font.pixelSize: root.theme.px(13)
                        font.bold: true
                    }
                    Label {
                        Layout.fillWidth: true
                        text: focusDrop.containsDrag ? qsTr("释放以开始任务")
                              : root.appViewModel.taskList.focusState === TaskListViewModel.AllBlocked
                                ? qsTr("当前没有可执行任务")
                                : root.appViewModel.taskList.focusState === TaskListViewModel.NoTasks
                                  ? qsTr("还没有待办任务")
                                  : root.appViewModel.taskList.focusTitle
                        color: root.theme.textPrimary
                        font.pixelSize: root.theme.px(20)
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        text: root.appViewModel.taskList.focusState === TaskListViewModel.AllBlocked
                              ? qsTr("任务正在等待前置任务完成，可前往依赖图查看。")
                              : root.appViewModel.taskList.focusState === TaskListViewModel.NoTasks
                                ? qsTr("创建第一项任务，开始安排你的计划。")
                                : root.appViewModel.taskList.focusState === TaskListViewModel.Suggested
                                  ? qsTr("淡化推荐 · %1 · 可拖入任意可执行卡片")
                                      .arg(root.appViewModel.taskList.focusReasonText)
                                  : root.appViewModel.taskList.focusDescription
                        color: root.theme.textSecondary
                        font.pixelSize: root.theme.px(13)
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: root.appViewModel.taskList.focusTaskId.length > 0
                        text: {
                            let values = [root.appViewModel.taskList.focusStatusText,
                                          root.appViewModel.taskList.focusPriorityText + qsTr("优先级")]
                            if (root.appViewModel.taskList.focusDeadlineText.length > 0)
                                values.push(qsTr("截止 %1").arg(root.appViewModel.taskList.focusDeadlineText))
                            return values.join("  ·  ")
                        }
                        color: root.theme.textMuted
                        font.pixelSize: root.theme.px(12)
                        elide: Text.ElideRight
                    }
                }

                ColumnLayout {
                    spacing: 8
                    Button {
                        visible: root.appViewModel.taskList.focusTaskId.length > 0
                        text: qsTr("查看详情")
                        onClicked: root.openDetails(root.appViewModel.taskList.focusTaskId)
                    }
                    Button {
                        objectName: "focusPrimaryActionButton"
                        text: root.appViewModel.taskList.focusState === TaskListViewModel.InProgress
                              ? qsTr("完成任务")
                              : root.appViewModel.taskList.focusState === TaskListViewModel.Suggested
                                ? qsTr("开始推荐任务")
                                : root.appViewModel.taskList.focusState === TaskListViewModel.AllBlocked
                                  ? qsTr("查看依赖图") : qsTr("新建任务")
                        onClicked: {
                            if (root.appViewModel.taskList.focusState === TaskListViewModel.InProgress)
                                root.appViewModel.taskList.completeTask(root.appViewModel.taskList.focusTaskId)
                            else if (root.appViewModel.taskList.focusState === TaskListViewModel.Suggested)
                                root.appViewModel.taskList.startTask(root.appViewModel.taskList.focusTaskId)
                            else if (root.appViewModel.taskList.focusState === TaskListViewModel.AllBlocked)
                                root.showDependencyGraphRequested()
                            else
                                root.beginCreate()
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: toolbarLayout.implicitHeight + root.theme.px(20)
            radius: 11
            color: root.theme.surface
            border.color: root.theme.borderSoft
            RowLayout {
                id: toolbarLayout
                anchors.fill: parent
                anchors.margins: root.theme.px(10)
                spacing: root.theme.px(8)
                Button {
                    text: qsTr("活动任务")
                    checkable: true
                    checked: !root.appViewModel.taskList.showArchived
                    onClicked: root.appViewModel.taskList.showArchived = false
                }
                Button {
                    text: qsTr("归档")
                    checkable: true
                    checked: root.appViewModel.taskList.showArchived
                    onClicked: root.appViewModel.taskList.showArchived = true
                }
                TextField {
                    id: taskSearchField
                    objectName: "taskSearchField"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 180
                    Layout.maximumWidth: 420
                    text: root.appViewModel.taskList.searchText
                    placeholderText: qsTr("搜索任务标题或描述")
                    selectByMouse: true
                    onTextEdited: root.appViewModel.taskList.searchText = text
                }
                ComboBox {
                    id: priorityFilterComboBox
                    objectName: "priorityFilterComboBox"
                    Layout.preferredWidth: root.theme.px(145)
                    model: root.appViewModel.taskList.priorityFilterOptions
                    currentIndex: root.appViewModel.taskList.priorityFilterIndex
                    onActivated: index => root.appViewModel.taskList.priorityFilterIndex = index
                }
                Button {
                    id: clearFiltersButton
                    objectName: "clearFiltersButton"
                    text: qsTr("清除")
                    visible: root.appViewModel.taskList.hasActiveFilters
                    onClicked: root.appViewModel.taskList.clearFilters()
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: qsTr("%1 项").arg(root.appViewModel.taskList.count)
                    color: root.theme.textMuted
                }
                Button {
                    objectName: "newTaskButton"
                    text: qsTr("＋ 新建任务")
                    onClicked: root.beginCreate()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 12
            color: "transparent"

            ListView {
                id: taskListView
                objectName: "taskListView"
                anchors.fill: parent
                clip: true
                spacing: root.theme.px(10)
                model: root.appViewModel.taskList

                delegate: Rectangle {
                    id: taskDelegate
                    required property string taskId
                    required property string title
                    required property string description
                    required property int status
                    required property string statusText
                    required property int priority
                    required property string priorityText
                    required property string deadlineText
                    required property string orderReasonText
                    required property bool blocked
                    required property string blockingReasonText
                    required property int predecessorCount
                    required property int unlockCount
                    required property bool canEditTask
                    required property bool canEditDependencies
                    required property bool canStart
                    required property bool canCancel
                    required property bool canComplete
                    required property bool canRedo
                    required property bool canArchive
                    required property bool canRestore
                    required property int estimatedMinutes

                    width: ListView.view.width
                    height: Math.max(root.theme.px(138), cardContent.implicitHeight + root.theme.px(24))
                    radius: 11
                    color: cardHover.hovered ? root.theme.surfaceSubtle : root.theme.surface
                    border.width: activeFocus ? 2 : 1
                    border.color: activeFocus ? root.theme.primary : root.theme.borderSoft
                    activeFocusOnTab: true

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 4
                        radius: 2
                        color: taskDelegate.blocked ? root.theme.warning
                              : root.theme.statusColor(taskDelegate.status)
                    }

                    HoverHandler { id: cardHover }
                    TapHandler { onTapped: root.openDetails(taskDelegate.taskId) }
                    Keys.onReturnPressed: root.openDetails(taskDelegate.taskId)
                    Keys.onSpacePressed: root.openDetails(taskDelegate.taskId)

                    RowLayout {
                        id: cardContent
                        anchors.fill: parent
                        anchors.margins: root.theme.px(14)
                        anchors.leftMargin: root.theme.px(18)
                        spacing: root.theme.px(12)

                        Rectangle {
                            id: dragHandle
                            objectName: "dragHandle_" + taskDelegate.taskId
                            visible: taskDelegate.canStart
                            Layout.preferredWidth: root.theme.px(34)
                            Layout.preferredHeight: root.theme.px(44)
                            radius: 8
                            color: dragArea.containsMouse ? root.theme.controlHover
                                                          : root.theme.surfaceStrong
                            border.color: root.theme.border

                            Label {
                                anchors.centerIn: parent
                                text: "⠿"
                                color: root.theme.textSecondary
                                font.pixelSize: root.theme.px(18)
                            }

                            MouseArea {
                                id: dragArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                drag.target: dragPreview
                                drag.threshold: 5
                                onPressed: mouse => {
                                    const point = dragHandle.mapToItem(Overlay.overlay,
                                                                       mouse.x, mouse.y)
                                    dragPreview.x = point.x - dragPreview.Drag.hotSpot.x
                                    dragPreview.y = point.y - dragPreview.Drag.hotSpot.y
                                    dragPreview.taskId = taskDelegate.taskId
                                    dragPreview.taskTitle = taskDelegate.title
                                    dragPreview.taskStatus = taskDelegate.statusText
                                    root.dragActive = true
                                }
                                onReleased: {
                                    // 手动 Drag 会话必须显式 drop；仅把 active 设为 false 只会发送离开事件。
                                    dragPreview.Drag.drop()
                                    root.dragActive = false
                                }
                                onCanceled: {
                                    dragPreview.Drag.cancel()
                                    root.dragActive = false
                                }
                            }

                            ToolTip.visible: dragArea.containsMouse && !dragArea.pressed
                            ToolTip.text: qsTr("拖到“现在做”开始任务")
                            ToolTip.delay: 350
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    Layout.fillWidth: true
                                    text: taskDelegate.title
                                    color: root.theme.textPrimary
                                    font.pixelSize: root.theme.px(17)
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                                Label {
                                    text: taskDelegate.statusText
                                    color: taskDelegate.blocked ? root.theme.warning
                                          : root.theme.statusColor(taskDelegate.status)
                                    font.bold: true
                                }
                                Rectangle {
                                    implicitWidth: priorityLabel.implicitWidth + 16
                                    implicitHeight: priorityLabel.implicitHeight + 8
                                    radius: height / 2
                                    color: root.theme.surfaceSubtle
                                    border.color: root.theme.priorityColor(taskDelegate.priority)
                                    Label {
                                        id: priorityLabel
                                        anchors.centerIn: parent
                                        text: qsTr("%1优先级").arg(taskDelegate.priorityText)
                                        color: root.theme.priorityColor(taskDelegate.priority)
                                        font.pixelSize: root.theme.px(12)
                                    }
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: taskDelegate.description.length > 0
                                text: taskDelegate.description
                                color: root.theme.textSecondary
                                maximumLineCount: 2
                                wrapMode: Text.Wrap
                                elide: Text.ElideRight
                                font.pixelSize: root.theme.px(13)
                            }
                            Label {
                                Layout.fillWidth: true
                                text: {
                                    let values = []
                                    if (taskDelegate.deadlineText.length > 0)
                                        values.push(qsTr("截止 %1").arg(taskDelegate.deadlineText))
                                    if (taskDelegate.estimatedMinutes > 0)
                                        values.push(qsTr("预计 %1 分钟").arg(taskDelegate.estimatedMinutes))
                                    if (taskDelegate.predecessorCount > 0)
                                        values.push(qsTr("前置 %1 项").arg(taskDelegate.predecessorCount))
                                    return values.length > 0 ? values.join("  ·  ") : qsTr("未设置时间与前置任务")
                                }
                                color: root.theme.textMuted
                                font.pixelSize: root.theme.px(12)
                                elide: Text.ElideRight
                            }
                            Label {
                                objectName: taskDelegate.blocked
                                            ? "blockingReasonLabel_" + taskDelegate.taskId
                                            : taskDelegate.unlockCount > 0
                                              ? "unlockCountLabel_" + taskDelegate.taskId : ""
                                Layout.fillWidth: true
                                text: taskDelegate.blocked
                                      ? qsTr("阻塞：%1").arg(taskDelegate.blockingReasonText)
                                      : taskDelegate.unlockCount > 0
                                        ? qsTr("推荐：%1 · 可解锁 %2 项")
                                            .arg(taskDelegate.orderReasonText).arg(taskDelegate.unlockCount)
                                        : qsTr("推荐：%1").arg(taskDelegate.orderReasonText)
                                color: taskDelegate.blocked ? root.theme.warning : root.theme.primary
                                font.pixelSize: root.theme.px(12)
                                elide: Text.ElideRight
                            }
                        }

                        Button {
                            objectName: "primaryTaskAction_" + taskDelegate.taskId
                            visible: taskDelegate.canStart || taskDelegate.canComplete
                                     || taskDelegate.canRedo || taskDelegate.canRestore
                            text: taskDelegate.canStart ? qsTr("开始")
                                  : taskDelegate.canComplete ? qsTr("完成")
                                  : taskDelegate.canRedo ? qsTr("重做") : qsTr("恢复")
                            onClicked: {
                                if (taskDelegate.canStart) root.appViewModel.taskList.startTask(taskDelegate.taskId)
                                else if (taskDelegate.canComplete) root.appViewModel.taskList.completeTask(taskDelegate.taskId)
                                else if (taskDelegate.canRedo) root.appViewModel.taskList.redoTask(taskDelegate.taskId)
                                else root.appViewModel.taskList.restoreTask(taskDelegate.taskId)
                            }
                        }
                        ToolButton {
                            objectName: "taskMenuButton_" + taskDelegate.taskId
                            text: "⋯"
                            onClicked: cardMenu.open()
                            Menu {
                                id: cardMenu
                                MenuItem { objectName: "viewTaskDetails_" + taskDelegate.taskId; text: qsTr("查看详情"); onTriggered: root.openDetails(taskDelegate.taskId) }
                                MenuItem { objectName: "editTaskButton_" + taskDelegate.taskId; enabled: taskDelegate.canEditTask; text: qsTr("编辑任务"); onTriggered: root.openEditor(taskDelegate.taskId) }
                                MenuItem { objectName: "editDependenciesButton_" + taskDelegate.taskId; enabled: taskDelegate.canEditDependencies; text: qsTr("编辑前置任务"); onTriggered: root.editDependenciesRequested(taskDelegate.taskId) }
                                MenuSeparator { }
                                MenuItem {
                                    objectName: "cancelTaskButton_" + taskDelegate.taskId
                                    enabled: taskDelegate.canCancel
                                    text: qsTr("取消任务")
                                    onTriggered: {
                                        cancelDialog.pendingTaskId = taskDelegate.taskId
                                        cancelDialog.pendingTitle = taskDelegate.title
                                        cancelDialog.open()
                                    }
                                }
                                MenuItem {
                                    objectName: "archiveTaskButton_" + taskDelegate.taskId
                                    enabled: taskDelegate.canArchive
                                    text: qsTr("归档")
                                    onTriggered: {
                                        archiveDialog.pendingTaskId = taskDelegate.taskId
                                        archiveDialog.pendingTitle = taskDelegate.title
                                        archiveDialog.open()
                                    }
                                }
                            }
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar { }
            }

            Label {
                objectName: "taskEmptyStateLabel"
                anchors.centerIn: parent
                visible: root.appViewModel.taskList.count === 0
                text: root.appViewModel.taskList.hasActiveFilters
                      ? qsTr("没有符合当前搜索和筛选条件的任务")
                      : root.appViewModel.taskList.showArchived
                        ? qsTr("还没有归档任务") : qsTr("还没有任务，从新建一项开始吧")
                color: root.theme.textMuted
                font.pixelSize: root.theme.px(15)
            }
        }
    }

    // 唯一拖动预览位于Overlay，不受ListView裁剪；DropArea从source直接读取TaskId。
    Rectangle {
        id: dragPreview
        objectName: "taskDragPreview"
        parent: Overlay.overlay
        property string taskId
        property string taskTitle
        property string taskStatus
        visible: root.dragActive
        z: 10000
        width: root.theme.px(280)
        height: root.theme.px(66)
        radius: 12
        color: root.theme.surfaceElevated
        border.width: 2
        border.color: root.theme.primary
        opacity: root.dragActive ? 0.96 : 0
        scale: root.dragActive ? 1.0 : 0.96

        Drag.active: root.dragActive
        Drag.source: dragPreview
        Drag.keys: ["smartmate-start-task"]
        Drag.supportedActions: Qt.MoveAction
        Drag.hotSpot.x: root.theme.px(24)
        Drag.hotSpot.y: height / 2
        Drag.onDragFinished: {
            dragPreview.taskId = ""
            dragPreview.taskTitle = ""
            dragPreview.taskStatus = ""
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10
            Label { text: "⠿"; color: root.theme.primary; font.pixelSize: root.theme.px(18) }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label {
                    Layout.fillWidth: true
                    text: dragPreview.taskTitle
                    color: root.theme.textPrimary
                    font.bold: true
                    elide: Text.ElideRight
                }
                Label {
                    text: qsTr("%1 · 拖到“现在做”开始").arg(dragPreview.taskStatus)
                    color: root.theme.textSecondary
                    font.pixelSize: root.theme.px(12)
                }
            }
        }

        Behavior on opacity { NumberAnimation { duration: 120 } }
        Behavior on scale { NumberAnimation { duration: 120 } }
    }

    TaskEditorDialog {
        id: editorDialog
        anchors.centerIn: Overlay.overlay
        editor: root.appViewModel.taskEditor
        theme: root.theme
    }
    TaskDetailsDialog {
        id: detailsDialog
        anchors.centerIn: Overlay.overlay
        taskList: root.appViewModel.taskList
        theme: root.theme
        onEditRequested: taskId => { close(); root.openEditor(taskId) }
        onEditDependenciesRequested: taskId => { close(); root.editDependenciesRequested(taskId) }
    }
    Dialog {
        id: cancelDialog
        objectName: "cancelTaskDialog"
        property string pendingTaskId
        property string pendingTitle
        anchors.centerIn: Overlay.overlay
        width: 430
        title: qsTr("确认取消任务")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label {
            width: 380; wrapMode: Text.Wrap
            text: qsTr("确定要取消“%1”吗？它作为前置任务的依赖将暂时失效，重做后会重新生效。")
                  .arg(cancelDialog.pendingTitle)
        }
        onAccepted: root.appViewModel.taskList.cancelTask(pendingTaskId)
    }
    Dialog {
        id: archiveDialog
        objectName: "archiveTaskDialog"
        property string pendingTaskId
        property string pendingTitle
        anchors.centerIn: Overlay.overlay
        width: 410
        title: qsTr("确认归档")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label { width: 360; wrapMode: Text.Wrap; text: qsTr("确定要归档“%1”吗？之后可在归档视图中恢复。").arg(archiveDialog.pendingTitle) }
        onAccepted: root.appViewModel.taskList.archiveTask(pendingTaskId)
    }
    Dialog {
        id: errorDialog
        property string message
        anchors.centerIn: Overlay.overlay
        width: 390
        title: qsTr("操作失败")
        modal: true
        standardButtons: Dialog.Ok
        Label { width: 340; wrapMode: Text.Wrap; text: errorDialog.message }
        onClosed: root.appViewModel.taskList.clearError()
    }
    Connections {
        target: root.appViewModel.taskList
        function onErrorOccurred(message) {
            errorDialog.message = message
            errorDialog.open()
        }
    }
}
