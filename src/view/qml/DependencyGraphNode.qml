pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SmartMate.ViewModel 1.0

Rectangle {
    id: node
    required property var theme
    required property string taskId
    required property string title
    required property string statusText
    required property int stateColorKey
    required property string priorityText
    required property string deadlineText
    required property int unlockCount
    required property bool blocked
    required property string blockingReasonText
    required property bool archived
    required property real nodeX
    required property real nodeY
    required property real nodeWidth
    required property real nodeHeight
    required property bool selected
    required property int emphasisLevel
    required property bool filterMatched
    required property string categoryName
    required property string categoryAccent
    required property bool hasCategory
    required property bool coreNode
    signal selectedRequested(string taskId)
    signal hoverRequested(string taskId, bool hovered)

    objectName: "graphNode_" + node.taskId
    x: node.nodeX
    y: node.nodeY
    width: node.nodeWidth
    height: node.nodeHeight
    radius: 11
    color: node.selected ? node.theme.primarySoft
          : !node.coreNode ? node.theme.surfaceStrong
          : node.archived ? node.theme.surfaceStrong : node.theme.surfaceElevated
    border.width: node.selected ? 3 : node.stateColorKey === 1 ? 2.5 : 1.5
    border.color: node.selected || node.stateColorKey === 1 ? node.theme.primary
        : !node.coreNode ? node.theme.archived
        : node.blocked ? node.theme.warning
        : node.archived ? node.theme.archived : node.theme.borderStrong
    opacity: !node.filterMatched
             || node.emphasisLevel === TaskGraphViewModel.UnrelatedEmphasis ? 0.32
             : !node.coreNode && !node.selected ? 0.68 : 1.0
    scale: node.selected ? 1.025 : 1.0
    Behavior on opacity { NumberAnimation { duration: 140 } }
    Behavior on scale { NumberAnimation { duration: 140 } }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 5
        radius: 3
        color: node.blocked ? node.theme.warning
                            : node.theme.statusColor(node.stateColorKey)
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: node.theme.px(15)
        anchors.rightMargin: node.theme.px(11)
        anchors.topMargin: node.theme.px(9)
        anchors.bottomMargin: node.theme.px(9)
        spacing: 3
        RowLayout {
            Layout.fillWidth: true
            Rectangle { Layout.preferredWidth: 9; Layout.preferredHeight: 9; radius: 5; color: node.blocked ? node.theme.warning : node.theme.statusColor(node.stateColorKey) }
            Label { Layout.fillWidth: true; Layout.minimumWidth: 0; text: node.title; color: node.theme.textPrimary; font.bold: true; font.pixelSize: node.theme.px(14); elide: Text.ElideRight }
            Rectangle {
                visible: node.hasCategory
                Layout.maximumWidth: node.theme.px(72)
                implicitWidth: Math.min(node.theme.px(72), nodeCategoryLabel.implicitWidth + 12)
                implicitHeight: nodeCategoryLabel.implicitHeight + 5
                radius: height / 2
                color: Qt.alpha(node.categoryAccent, 0.12)
                border.color: node.categoryAccent
                Label {
                    id: nodeCategoryLabel
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    verticalAlignment: Text.AlignVCenter
                    text: node.categoryName
                    color: node.categoryAccent
                    font.pixelSize: node.theme.px(10)
                    elide: Text.ElideRight
                }
            }
            Label { text: node.blocked ? "🔒" : ""; visible: node.blocked; font.pixelSize: node.theme.px(12) }
        }
        Label { Layout.fillWidth: true; text: qsTr("%1 · %2优先级").arg(node.statusText).arg(node.priorityText); color: node.theme.textSecondary; font.pixelSize: node.theme.px(12); elide: Text.ElideRight }
        Label {
            Layout.fillWidth: true
            text: !node.coreNode ? qsTr("跨类别上下文")
                : node.blocked ? qsTr("等待前置任务")
                : node.unlockCount > 0 ? qsTr("完成后解锁 %1 项").arg(node.unlockCount)
                : node.deadlineText
            color: node.blocked ? node.theme.warning : node.theme.textMuted
            font.pixelSize: node.theme.px(11)
            elide: Text.ElideRight
        }
    }
    Rectangle { width: 10; height: 10; radius: 5; anchors.horizontalCenter: parent.horizontalCenter; anchors.top: parent.top; anchors.topMargin: -5; color: node.theme.surfaceElevated; border.color: node.theme.borderStrong }
    Rectangle { width: 10; height: 10; radius: 5; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: -5; color: node.theme.surfaceElevated; border.color: node.theme.borderStrong }
    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: node.selectedRequested(node.taskId) }
    HoverHandler { onHoveredChanged: node.hoverRequested(node.taskId, hovered) }
    ToolTip.visible: nodeHover.hovered && node.blockingReasonText.length > 0
    ToolTip.text: node.blockingReasonText
    ToolTip.delay: 450
    HoverHandler { id: nodeHover }
}
