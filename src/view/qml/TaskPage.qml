pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SmartMate.ViewModel 1.0

// Bound 使列表 delegate 对外层对象的引用保持显式且可被 QML 工具检查。
Page {
    id: root

    required property AppViewModel appViewModel

    background: Rectangle {
        color: "#f5f7fb"
    }

    // 顶部区域只转发“开始新建”命令，草稿初始化由 TaskEditorViewModel 完成。
    header: ToolBar {
        implicitHeight: 72
        background: Rectangle {
            color: "#ffffff"
            border.color: "#e4e7ec"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 28
            anchors.rightMargin: 28

            Label {
                text: root.appViewModel.applicationName
                color: "#172033"
                font.pixelSize: 26
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("新建任务")
                onClicked: {
                    root.appViewModel.taskEditor.beginCreate()
                    editorDialog.open()
                }
            }
        }
    }

    // 主体绑定活动/归档投影以及 ViewModel 已格式化的任务角色。
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 18

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

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

            Item { Layout.fillWidth: true }

            Label {
                text: qsTr("共 %1 项").arg(root.appViewModel.taskList.count)
                color: "#667085"
            }
        }

        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.appViewModel.taskList.errorMessage
            color: "#b42318"
            wrapMode: Text.Wrap
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 12
            color: "#ffffff"
            border.color: "#e4e7ec"

            ListView {
                id: taskListView
                anchors.fill: parent
                anchors.margins: 12
                clip: true
                spacing: 10
                model: root.appViewModel.taskList

                // delegate 只展示角色，并用稳定 taskId 转发操作；行号不代表任务身份。
                delegate: Rectangle {
                    id: taskDelegate

                    required property string taskId
                    required property string title
                    required property string description
                    required property string statusText
                    required property string priorityText
                    required property string deadlineText
                    required property int estimatedMinutes

                    width: ListView.view.width
                    height: delegateContent.implicitHeight + 24
                    radius: 9
                    color: "#f9fafb"
                    border.color: "#eaecf0"

                    RowLayout {
                        id: delegateContent
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 16
                        anchors.rightMargin: 12
                        spacing: 14

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 5

                            Label {
                                Layout.fillWidth: true
                                text: taskDelegate.title
                                color: "#172033"
                                font.pixelSize: 17
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: taskDelegate.description.length > 0
                                text: taskDelegate.description
                                color: "#667085"
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: {
                                    let details = taskDelegate.statusText + "  ·  "
                                                  + qsTr("优先级：%1").arg(taskDelegate.priorityText)
                                    if (taskDelegate.deadlineText.length > 0)
                                        details += "  ·  " + qsTr("截止：%1").arg(taskDelegate.deadlineText)
                                    if (taskDelegate.estimatedMinutes > 0)
                                        details += "  ·  " + qsTr("预计 %1 分钟").arg(taskDelegate.estimatedMinutes)
                                    return details
                                }
                                color: "#475467"
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }
                        }

                        Button {
                            text: qsTr("编辑")
                            onClicked: {
                                if (root.appViewModel.taskEditor.beginEdit(taskDelegate.taskId))
                                    editorDialog.open()
                            }
                        }

                        Button {
                            text: qsTr("归档")
                            visible: !root.appViewModel.taskList.showArchived
                            onClicked: {
                                archiveDialog.pendingTaskId = taskDelegate.taskId
                                archiveDialog.pendingTitle = taskDelegate.title
                                archiveDialog.open()
                            }
                        }

                        Button {
                            text: qsTr("恢复")
                            visible: root.appViewModel.taskList.showArchived
                            onClicked: root.appViewModel.taskList.restoreTask(taskDelegate.taskId)
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar { }
            }

            Label {
                anchors.centerIn: parent
                visible: root.appViewModel.taskList.count === 0
                text: root.appViewModel.taskList.showArchived
                      ? qsTr("还没有归档任务")
                      : qsTr("还没有任务，从新建一项开始吧")
                color: "#667085"
                font.pixelSize: 16
            }
        }
    }

    // 编辑对话框绑定独立草稿，不直接修改列表或领域实体。
    TaskEditorDialog {
        id: editorDialog
        anchors.centerIn: Overlay.overlay
        editor: root.appViewModel.taskEditor
    }

    // 归档确认属于交互流程；真正的状态转换仍由 ViewModel 命令转交 Service。
    Dialog {
        id: archiveDialog

        property string pendingTaskId
        property string pendingTitle

        anchors.centerIn: Overlay.overlay
        title: qsTr("确认归档")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        Label {
            width: 360
            wrapMode: Text.Wrap
            text: qsTr("确定要归档“%1”吗？之后可在归档视图中恢复。")
                  .arg(archiveDialog.pendingTitle)
        }

        onAccepted: root.appViewModel.taskList.archiveTask(pendingTaskId)
    }

    // 错误既可在页面持续显示，也可通过事件信号立即提示。
    Dialog {
        id: errorDialog

        property string message

        anchors.centerIn: Overlay.overlay
        title: qsTr("操作失败")
        modal: true
        standardButtons: Dialog.Ok

        Label {
            width: 340
            wrapMode: Text.Wrap
            text: errorDialog.message
        }

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
