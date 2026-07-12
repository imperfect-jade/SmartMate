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

        // 任务槽只消费 ViewModel 投影，并通过稳定 TaskId 转发命令。
        TaskFocusCard {
            Layout.fillWidth: true
            visible: !root.appViewModel.taskList.bulkSelectionMode
            taskList: root.appViewModel.taskList
            theme: root.theme
            dragPreview: dragPreview
            onDetailsRequested: taskId => root.openDetails(taskId)
            onStartRequested: taskId => root.appViewModel.taskList.startTask(taskId)
            onCompleteRequested: taskId =>
                root.appViewModel.taskList.completeTask(taskId)
            onDependencyGraphRequested: root.showDependencyGraphRequested()
            onCreateRequested: root.beginCreate()
        }

        TaskFilterBar {
            Layout.fillWidth: true
            taskList: root.appViewModel.taskList
            theme: root.theme
            onNewTaskRequested: root.beginCreate()
            onBulkArchiveRequested: {
                bulkArchiveDialog.pendingCount =
                    root.appViewModel.taskList.bulkSelectedCount
                bulkArchiveDialog.open()
            }
            onBulkRestoreRequested:
                root.appViewModel.taskList.restoreSelectedTasks()
            onBulkDeleteRequested: {
                bulkDeleteDialog.pendingCount =
                    root.appViewModel.taskList.bulkSelectedCount
                bulkDeleteDialog.open()
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

                delegate: TaskCard {
                    width: ListView.view.width
                    theme: root.theme
                    dragPreview: dragPreview
                    bulkSelectionMode: root.appViewModel.taskList.bulkSelectionMode

                    onDetailsRequested: taskId => root.openDetails(taskId)
                    onEditRequested: taskId => root.openEditor(taskId)
                    onEditDependenciesRequested: taskId =>
                        root.editDependenciesRequested(taskId)
                    onStartRequested: taskId =>
                        root.appViewModel.taskList.startTask(taskId)
                    onCompleteRequested: taskId =>
                        root.appViewModel.taskList.completeTask(taskId)
                    onRedoRequested: taskId =>
                        root.appViewModel.taskList.redoTask(taskId)
                    onRestoreRequested: taskId =>
                        root.appViewModel.taskList.restoreTask(taskId)
                    onCancelRequested: (taskId, title) => {
                        cancelDialog.pendingTaskId = taskId
                        cancelDialog.pendingTitle = title
                        cancelDialog.open()
                    }
                    onArchiveRequested: (taskId, title) => {
                        archiveDialog.pendingTaskId = taskId
                        archiveDialog.pendingTitle = title
                        archiveDialog.open()
                    }
                    onDeletePermanentlyRequested: (taskId, title) => {
                        deleteDialog.pendingTaskId = taskId
                        deleteDialog.pendingTitle = title
                        deleteDialog.open()
                    }
                    onBulkSelectionToggled: taskId =>
                        root.appViewModel.taskList.toggleBulkSelection(taskId)
                    onDragActiveRequested: active => root.dragActive = active
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
        id: deleteDialog
        objectName: "deleteArchivedTaskDialog"
        property string pendingTaskId
        property string pendingTitle
        anchors.centerIn: Overlay.overlay
        width: 450
        title: qsTr("确认永久删除")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label {
            objectName: "deleteArchivedTaskWarning"
            width: 400
            wrapMode: Text.Wrap
            text: qsTr("确定要永久删除“%1”吗？此操作不可撤销，任务关联的全部前置和后继依赖也会永久删除。")
                  .arg(deleteDialog.pendingTitle)
        }
        onAccepted:
            root.appViewModel.taskList.deleteArchivedTask(pendingTaskId)
    }
    Dialog {
        id: bulkArchiveDialog
        objectName: "bulkArchiveTaskDialog"
        property int pendingCount: 0
        anchors.centerIn: Overlay.overlay
        width: 430
        title: qsTr("确认批量归档")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label {
            objectName: "bulkArchiveTaskWarning"
            width: 380
            wrapMode: Text.Wrap
            text: qsTr("确定要归档选中的 %1 项任务吗？之后可在归档视图中恢复。")
                  .arg(bulkArchiveDialog.pendingCount)
        }
        onAccepted: root.appViewModel.taskList.archiveSelectedTasks()
    }
    Dialog {
        id: bulkDeleteDialog
        objectName: "bulkDeleteArchivedTaskDialog"
        property int pendingCount: 0
        anchors.centerIn: Overlay.overlay
        width: 470
        title: qsTr("确认批量永久删除")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label {
            objectName: "bulkDeleteArchivedTaskWarning"
            width: 420
            wrapMode: Text.Wrap
            text: qsTr("确定要永久删除选中的 %1 项归档任务吗？此操作不可撤销，关联的全部前置和后继依赖也会永久删除。")
                  .arg(bulkDeleteDialog.pendingCount)
        }
        onAccepted: root.appViewModel.taskList.deleteSelectedArchivedTasks()
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
