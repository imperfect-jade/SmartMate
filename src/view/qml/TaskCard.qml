pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: card

    required property var theme
    required property var dragPreview
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
    required property bool overdue
    required property bool canDeletePermanently
    required property bool bulkSelectionMode
    required property bool bulkSelected
    required property bool bulkSelectable

    signal detailsRequested(string taskId)
    signal editRequested(string taskId)
    signal editDependenciesRequested(string taskId)
    signal startRequested(string taskId)
    signal completeRequested(string taskId)
    signal redoRequested(string taskId)
    signal restoreRequested(string taskId)
    signal cancelRequested(string taskId, string title)
    signal archiveRequested(string taskId, string title)
    signal deletePermanentlyRequested(string taskId, string title)
    signal bulkSelectionToggled(string taskId)
    signal dragActiveRequested(bool active)

    height: Math.max(card.theme.px(138), cardContent.implicitHeight + card.theme.px(24))
    radius: 11
    color: cardHover.hovered ? card.theme.surfaceSubtle : card.theme.surface
    border.width: activeFocus ? 2 : 1
    border.color: activeFocus ? card.theme.primary : card.theme.borderSoft
    activeFocusOnTab: true

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 4
        radius: 2
        color: card.blocked ? card.theme.warning : card.theme.statusColor(card.status)
    }

    HoverHandler { id: cardHover }
    TapHandler {
        onTapped: eventPoint => {
            if (!card.bulkSelectionMode) {
                card.detailsRequested(card.taskId)
                return
            }
            if (!card.bulkSelectable)
                return
            const checkPoint = bulkCheckBox.mapFromItem(
                                 card, eventPoint.position.x,
                                 eventPoint.position.y)
            // CheckBox自身已转发点击，父级卡片不得再次切换同一TaskId。
            if (checkPoint.x >= 0 && checkPoint.x <= bulkCheckBox.width
                    && checkPoint.y >= 0 && checkPoint.y <= bulkCheckBox.height)
                return
            card.bulkSelectionToggled(card.taskId)
        }
    }
    Keys.onReturnPressed: {
        if (card.bulkSelectionMode) {
            if (card.bulkSelectable)
                card.bulkSelectionToggled(card.taskId)
        } else {
            card.detailsRequested(card.taskId)
        }
    }
    Keys.onSpacePressed: {
        if (card.bulkSelectionMode) {
            if (card.bulkSelectable)
                card.bulkSelectionToggled(card.taskId)
        } else {
            card.detailsRequested(card.taskId)
        }
    }

    RowLayout {
        id: cardContent
        anchors.fill: parent
        anchors.margins: card.theme.px(14)
        anchors.leftMargin: card.theme.px(18)
        spacing: card.theme.px(12)

        CheckBox {
            id: bulkCheckBox
            objectName: "bulkTaskCheckBox_" + card.taskId
            visible: card.bulkSelectionMode
            checked: card.bulkSelected
            enabled: card.bulkSelectable
            opacity: card.bulkSelectable ? 1.0 : 0.45
            Accessible.name: card.bulkSelected ? qsTr("取消选择 %1").arg(card.title)
                                               : qsTr("选择 %1").arg(card.title)
            onClicked: card.bulkSelectionToggled(card.taskId)
        }

        Rectangle {
            id: dragHandle
            objectName: "dragHandle_" + card.taskId
            visible: !card.bulkSelectionMode && card.canStart
            Layout.preferredWidth: card.theme.px(34)
            Layout.preferredHeight: card.theme.px(44)
            radius: 8
            color: dragArea.containsMouse ? card.theme.controlHover
                                          : card.theme.surfaceStrong
            border.color: card.theme.border

            Label {
                anchors.centerIn: parent
                text: "⠿"
                color: card.theme.textSecondary
                font.pixelSize: card.theme.px(18)
            }

            MouseArea {
                id: dragArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                drag.target: card.dragPreview
                drag.threshold: 5
                onPressed: mouse => {
                    const point = dragHandle.mapToItem(Overlay.overlay, mouse.x, mouse.y)
                    card.dragPreview.x = point.x - card.dragPreview.Drag.hotSpot.x
                    card.dragPreview.y = point.y - card.dragPreview.Drag.hotSpot.y
                    card.dragPreview.taskId = card.taskId
                    card.dragPreview.taskTitle = card.title
                    card.dragPreview.taskStatus = card.statusText
                    card.dragActiveRequested(true)
                }
                onReleased: {
                    card.dragPreview.Drag.drop()
                    card.dragActiveRequested(false)
                }
                onCanceled: {
                    card.dragPreview.Drag.cancel()
                    card.dragActiveRequested(false)
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
                    text: card.title
                    color: card.theme.textPrimary
                    font.pixelSize: card.theme.px(17)
                    font.bold: true
                    elide: Text.ElideRight
                }
                Label {
                    text: card.statusText
                    color: card.blocked ? card.theme.warning
                                        : card.theme.statusColor(card.status)
                    font.bold: true
                }
                Rectangle {
                    objectName: "overdueBadge_" + card.taskId
                    visible: card.overdue
                    implicitWidth: overdueBadgeLabel.implicitWidth + 16
                    implicitHeight: overdueBadgeLabel.implicitHeight + 8
                    radius: height / 2
                    color: "#fef3f2"
                    border.color: card.theme.danger
                    Label {
                        id: overdueBadgeLabel
                        anchors.centerIn: parent
                        text: qsTr("已逾期")
                        color: card.theme.danger
                        font.pixelSize: card.theme.px(12)
                        font.bold: true
                    }
                }
                Rectangle {
                    implicitWidth: priorityLabel.implicitWidth + 16
                    implicitHeight: priorityLabel.implicitHeight + 8
                    radius: height / 2
                    color: card.theme.surfaceSubtle
                    border.color: card.theme.priorityColor(card.priority)
                    Label {
                        id: priorityLabel
                        anchors.centerIn: parent
                        text: qsTr("%1优先级").arg(card.priorityText)
                        color: card.theme.priorityColor(card.priority)
                        font.pixelSize: card.theme.px(12)
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                visible: card.description.length > 0
                text: card.description
                color: card.theme.textSecondary
                maximumLineCount: 2
                wrapMode: Text.Wrap
                elide: Text.ElideRight
                font.pixelSize: card.theme.px(13)
            }
            Label {
                Layout.fillWidth: true
                text: {
                    let values = []
                    if (card.deadlineText.length > 0)
                        values.push(qsTr("截止 %1").arg(card.deadlineText))
                    if (card.estimatedMinutes > 0)
                        values.push(qsTr("预计 %1 分钟").arg(card.estimatedMinutes))
                    if (card.predecessorCount > 0)
                        values.push(qsTr("前置 %1 项").arg(card.predecessorCount))
                    return values.length > 0 ? values.join("  ·  ")
                                             : qsTr("未设置时间与前置任务")
                }
                color: card.overdue && card.deadlineText.length > 0
                       ? card.theme.danger : card.theme.textMuted
                font.pixelSize: card.theme.px(12)
                elide: Text.ElideRight
            }
            Label {
                objectName: "overdueReminder_" + card.taskId
                Layout.fillWidth: true
                visible: card.overdue
                text: qsTr("已超过截止时间，请尽快处理")
                color: card.theme.danger
                font.pixelSize: card.theme.px(12)
                font.bold: true
                elide: Text.ElideRight
            }
            Label {
                objectName: card.blocked ? "blockingReasonLabel_" + card.taskId
                                         : card.unlockCount > 0
                                           ? "unlockCountLabel_" + card.taskId : ""
                Layout.fillWidth: true
                text: card.blocked
                      ? qsTr("阻塞：%1").arg(card.blockingReasonText)
                      : card.unlockCount > 0
                        ? qsTr("推荐：%1 · 可解锁 %2 项")
                            .arg(card.orderReasonText).arg(card.unlockCount)
                        : qsTr("推荐：%1").arg(card.orderReasonText)
                color: card.blocked ? card.theme.warning : card.theme.primary
                font.pixelSize: card.theme.px(12)
                elide: Text.ElideRight
            }
        }

        Button {
            objectName: "primaryTaskAction_" + card.taskId
            visible: !card.bulkSelectionMode
                     && (card.canStart || card.canComplete
                         || card.canRedo || card.canRestore)
            text: card.canStart ? qsTr("开始")
                  : card.canComplete ? qsTr("完成")
                  : card.canRedo ? qsTr("重做") : qsTr("恢复")
            onClicked: {
                if (card.canStart) card.startRequested(card.taskId)
                else if (card.canComplete) card.completeRequested(card.taskId)
                else if (card.canRedo) card.redoRequested(card.taskId)
                else card.restoreRequested(card.taskId)
            }
        }
        ToolButton {
            objectName: "taskMenuButton_" + card.taskId
            visible: !card.bulkSelectionMode
            text: "⋯"
            onClicked: cardMenu.open()
            Menu {
                id: cardMenu
                MenuItem { objectName: "viewTaskDetails_" + card.taskId; text: qsTr("查看详情"); onTriggered: card.detailsRequested(card.taskId) }
                MenuItem { objectName: "editTaskButton_" + card.taskId; visible: card.canEditTask; text: qsTr("编辑任务"); onTriggered: card.editRequested(card.taskId) }
                MenuItem { objectName: "editDependenciesButton_" + card.taskId; enabled: card.canEditDependencies; text: qsTr("编辑前置任务"); onTriggered: card.editDependenciesRequested(card.taskId) }
                MenuSeparator { }
                MenuItem { objectName: "cancelTaskButton_" + card.taskId; enabled: card.canCancel; text: qsTr("取消任务"); onTriggered: card.cancelRequested(card.taskId, card.title) }
                MenuItem { objectName: "archiveTaskButton_" + card.taskId; enabled: card.canArchive; text: qsTr("归档"); onTriggered: card.archiveRequested(card.taskId, card.title) }
                MenuItem { objectName: "deleteTaskPermanentlyButton_" + card.taskId; visible: card.canDeletePermanently; text: qsTr("永久删除"); onTriggered: card.deletePermanentlyRequested(card.taskId, card.title) }
            }
        }
    }
}
