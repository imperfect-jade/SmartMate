pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SmartMate.ViewModel 1.0

// 新建依赖选择是TaskEditorViewModel草稿的一部分；取消只恢复本次弹窗的选择快照。
Dialog {
    id: root
    objectName: "taskCreationPredecessorDialog"

    required property TaskEditorViewModel editor

    width: 600
    height: Math.min(560, parent ? parent.height - 40 : 560)
    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose
    title: qsTr("选择新任务的前置任务")

    contentItem: ColumnLayout {
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: qsTr("所选任务必须全部完成，新任务才会自动解锁。任务与依赖将在保存时一次写入。")
            color: "#475467"
            wrapMode: Text.Wrap
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("已选择 %1 项").arg(root.editor.selectedPredecessorCount)
            font.bold: true
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 8
            color: "#f9fafb"
            border.color: "#d0d5dd"

            ListView {
                id: creationCandidateList
                objectName: "creationPredecessorCandidateList"

                anchors.fill: parent
                anchors.margins: 8
                clip: true
                spacing: 6
                model: root.editor

                delegate: Rectangle {
                    id: candidateDelegate

                    required property string candidateTaskId
                    required property string candidateShortId
                    required property string candidateTitle
                    required property string candidateStatusText
                    required property string candidatePriorityText
                    required property bool candidateSelected

                    width: ListView.view.width
                    height: 52
                    radius: 6
                    color: candidateSelected ? "#eff8ff" : "#ffffff"
                    border.color: candidateSelected ? "#84caff" : "#eaecf0"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 10

                        CheckBox {
                            objectName: "creationPredecessorCheckBox_"
                                        + candidateDelegate.candidateTaskId
                            checked: candidateDelegate.candidateSelected
                            Accessible.name: candidateDelegate.candidateTitle
                            onToggled: root.editor.setCreationPredecessorSelected(
                                           candidateDelegate.candidateTaskId, checked)
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Label {
                                Layout.fillWidth: true
                                text: candidateDelegate.candidateTitle
                                color: "#172033"
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("ID %1").arg(candidateDelegate.candidateShortId)
                                color: "#98a2b3"
                                font.pixelSize: 11
                            }
                        }

                        Label {
                            text: qsTr("%1优先级 · %2")
                                  .arg(candidateDelegate.candidatePriorityText)
                                  .arg(candidateDelegate.candidateStatusText)
                            color: "#475467"
                            font.pixelSize: 13
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar { }
            }

            Label {
                anchors.centerIn: parent
                visible: root.editor.predecessorCandidateCount === 0
                text: qsTr("还没有可作为前置的活动任务")
                color: "#667085"
            }
        }
    }

    footer: Rectangle {
        implicitHeight: 62
        color: "#ffffff"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20

            Button {
                text: qsTr("清空")
                enabled: root.editor.selectedPredecessorCount > 0
                onClicked: root.editor.clearCreationPredecessors()
            }

            Item { Layout.fillWidth: true }

            Button {
                objectName: "cancelCreationPredecessorsButton"
                text: qsTr("取消")
                onClicked: {
                    root.editor.cancelPredecessorSelection()
                    root.close()
                }
            }

            Button {
                objectName: "acceptCreationPredecessorsButton"
                text: qsTr("确定")
                onClicked: {
                    root.editor.acceptPredecessorSelection()
                    root.close()
                }
            }
        }
    }
}
