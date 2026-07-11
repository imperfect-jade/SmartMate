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
    required property AppearanceTheme theme

    width: Math.max(root.theme.px(430),
                    Math.min(root.theme.px(640), parent ? parent.width - root.theme.px(48) : root.theme.px(640)))
    height: Math.max(root.theme.px(450),
                     Math.min(root.theme.px(590), parent ? parent.height - root.theme.px(48) : root.theme.px(590)))
    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose
    title: qsTr("选择新任务的前置任务")

    background: Rectangle {
        radius: 14
        color: root.theme.surfaceElevated
        border.color: root.theme.border
    }

    contentItem: ColumnLayout {
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: qsTr("所选任务必须全部完成，新任务才会自动解锁。任务与依赖将在保存时一次写入。")
            color: root.theme.textSecondary
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
            color: root.theme.surfaceSubtle
            border.color: root.theme.border

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
                    height: root.theme.px(64)
                    radius: 6
                    color: candidateSelected ? root.theme.primarySoft : root.theme.surfaceElevated
                    border.color: candidateSelected ? root.theme.primary : root.theme.borderSoft

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
                                color: root.theme.textPrimary
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("ID %1").arg(candidateDelegate.candidateShortId)
                                color: root.theme.textMuted
                                font.pixelSize: 11
                            }
                        }

                        ColumnLayout {
                            Layout.maximumWidth: root.theme.px(150)
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("%1优先级").arg(candidateDelegate.candidatePriorityText)
                                color: root.theme.textSecondary
                                font.pixelSize: root.theme.px(12)
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: candidateDelegate.candidateStatusText
                                color: root.theme.textMuted
                                font.pixelSize: root.theme.px(11)
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar { }
            }

            Label {
                anchors.centerIn: parent
                visible: root.editor.predecessorCandidateCount === 0
                text: qsTr("还没有可作为前置的活动任务")
                color: root.theme.textMuted
            }
        }
    }

    footer: Rectangle {
        implicitHeight: 62
        color: root.theme.surfaceStrong

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
