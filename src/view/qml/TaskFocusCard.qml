pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SmartMate.ViewModel 1.0

Rectangle {
    id: focus
    required property TaskListViewModel taskList
    required property var theme
    required property var dragPreview
    signal detailsRequested(string taskId)
    signal startRequested(string taskId)
    signal completeRequested(string taskId)
    signal dependencyGraphRequested()
    signal createRequested()

    objectName: "focusTaskSlot"
    implicitHeight: Math.max(focus.theme.px(148), focusContent.implicitHeight
                             + focus.theme.px(36))
    radius: 14
    color: focusDrop.containsDrag ? focus.theme.primarySoft
          : focus.taskList.focusState === TaskListViewModel.InProgress
            ? focus.theme.primarySoft : focus.theme.surface
    border.width: focusDrop.containsDrag ? 2 : 1
    border.color: focusDrop.containsDrag ? focus.theme.primary : focus.theme.borderSoft

    Behavior on color { ColorAnimation { duration: 180 } }

    DropArea {
        id: focusDrop
        objectName: "focusTaskDropArea"
        anchors.fill: parent
        keys: ["smartmate-start-task"]
        enabled: focus.taskList.focusState !== TaskListViewModel.InProgress
        onDropped: drop => {
            if (drop.source !== focus.dragPreview
                    || focus.dragPreview.taskId.length === 0)
                return
            const taskId = focus.dragPreview.taskId
            drop.acceptProposedAction()
            Qt.callLater(() => focus.startRequested(taskId))
        }
    }

    RowLayout {
        id: focusContent
        anchors.fill: parent
        anchors.margins: focus.theme.px(18)
        spacing: focus.theme.px(18)

        Rectangle {
            Layout.preferredWidth: focus.theme.px(48)
            Layout.preferredHeight: focus.theme.px(48)
            radius: 14
            color: focus.theme.primary
            Label {
                anchors.centerIn: parent
                text: focus.taskList.focusState === TaskListViewModel.InProgress
                      ? "▶" : focus.taskList.focusState === TaskListViewModel.AllBlocked
                        ? "⌁" : "+"
                color: "white"
                font.pixelSize: focus.theme.px(20)
                font.bold: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 5
            RowLayout {
                Label {
                    text: focus.taskList.focusState === TaskListViewModel.InProgress
                          ? qsTr("现在做 · 正在进行") : qsTr("现在做")
                    color: focus.theme.primary
                    font.pixelSize: focus.theme.px(13)
                    font.bold: true
                }
                Rectangle {
                    objectName: "focusCategoryBadge"
                    visible: focus.taskList.focusHasCategory
                    Layout.maximumWidth: focus.theme.px(120)
                    implicitWidth: Math.min(focus.theme.px(120),
                                            focusCategoryLabel.implicitWidth + 16)
                    implicitHeight: focusCategoryLabel.implicitHeight + 6
                    radius: height / 2
                    color: Qt.alpha(focus.taskList.focusCategoryAccent, 0.12)
                    border.color: focus.taskList.focusCategoryAccent
                    Label {
                        id: focusCategoryLabel
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        verticalAlignment: Text.AlignVCenter
                        text: focus.taskList.focusCategoryName
                        color: focus.taskList.focusCategoryAccent
                        font.pixelSize: focus.theme.px(11)
                        font.bold: true
                        elide: Text.ElideRight
                    }
                }
                Rectangle {
                    objectName: "focusOverdueBadge"
                    visible: focus.taskList.focusOverdue
                    implicitWidth: focusOverdueLabel.implicitWidth + 16
                    implicitHeight: focusOverdueLabel.implicitHeight + 6
                    radius: height / 2
                    color: "#fef3f2"
                    border.color: focus.theme.danger
                    Label {
                        id: focusOverdueLabel
                        anchors.centerIn: parent
                        text: qsTr("已逾期")
                        color: focus.theme.danger
                        font.pixelSize: focus.theme.px(11)
                        font.bold: true
                    }
                }
                Item { Layout.fillWidth: true }
            }
            Label {
                Layout.fillWidth: true
                text: focusDrop.containsDrag ? qsTr("释放以开始任务")
                      : focus.taskList.focusState === TaskListViewModel.AllBlocked
                        ? qsTr("当前没有可执行任务")
                        : focus.taskList.focusState === TaskListViewModel.NoTasks
                          ? qsTr("还没有待办任务") : focus.taskList.focusTitle
                color: focus.theme.textPrimary
                font.pixelSize: focus.theme.px(20)
                font.bold: true
                elide: Text.ElideRight
            }
            Label {
                Layout.fillWidth: true
                text: focus.taskList.focusState === TaskListViewModel.AllBlocked
                      ? qsTr("任务正在等待前置任务完成，可前往依赖图查看。")
                      : focus.taskList.focusState === TaskListViewModel.NoTasks
                        ? qsTr("创建第一项任务，开始安排你的计划。")
                        : focus.taskList.focusState === TaskListViewModel.Suggested
                          ? qsTr("淡化推荐 · %1 · 可拖入任意可执行卡片")
                              .arg(focus.taskList.focusReasonText)
                          : focus.taskList.focusDescription
                color: focus.theme.textSecondary
                font.pixelSize: focus.theme.px(13)
                elide: Text.ElideRight
            }
            Label {
                Layout.fillWidth: true
                visible: focus.taskList.focusTaskId.length > 0
                text: {
                    let values = [focus.taskList.focusStatusText,
                                  focus.taskList.focusPriorityText + qsTr("优先级")]
                    if (focus.taskList.focusDeadlineText.length > 0)
                        values.push(qsTr("截止 %1").arg(focus.taskList.focusDeadlineText))
                    return values.join("  ·  ")
                }
                color: focus.taskList.focusOverdue
                       ? focus.theme.danger : focus.theme.textMuted
                font.pixelSize: focus.theme.px(12)
                elide: Text.ElideRight
            }
            Label {
                objectName: "focusOverdueReminder"
                Layout.fillWidth: true
                visible: focus.taskList.focusOverdue
                text: qsTr("已超过截止时间，请尽快处理")
                color: focus.theme.danger
                font.pixelSize: focus.theme.px(12)
                font.bold: true
                elide: Text.ElideRight
            }
        }

        ColumnLayout {
            spacing: 8
            Button {
                visible: focus.taskList.focusTaskId.length > 0
                text: qsTr("查看详情")
                onClicked: focus.detailsRequested(focus.taskList.focusTaskId)
            }
            Button {
                objectName: "focusPrimaryActionButton"
                text: focus.taskList.focusState === TaskListViewModel.InProgress
                      ? qsTr("完成任务")
                      : focus.taskList.focusState === TaskListViewModel.Suggested
                        ? qsTr("开始推荐任务")
                        : focus.taskList.focusState === TaskListViewModel.AllBlocked
                          ? qsTr("查看依赖图") : qsTr("新建任务")
                onClicked: {
                    if (focus.taskList.focusState === TaskListViewModel.InProgress)
                        focus.completeRequested(focus.taskList.focusTaskId)
                    else if (focus.taskList.focusState === TaskListViewModel.Suggested)
                        focus.startRequested(focus.taskList.focusTaskId)
                    else if (focus.taskList.focusState === TaskListViewModel.AllBlocked)
                        focus.dependencyGraphRequested()
                    else
                        focus.createRequested()
                }
            }
        }
    }
}
