import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SmartMate.ViewModel 1.0

Dialog {
    id: root
    required property TaskListViewModel taskList
    required property AppearanceTheme theme
    signal editRequested(string taskId)
    signal editDependenciesRequested(string taskId)

    objectName: "taskDetailsDialog"
    width: Math.min(680, parent ? parent.width - 48 : 680)
    height: Math.min(620, parent ? parent.height - 48 : 620)
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    title: qsTr("任务详情")
    standardButtons: Dialog.Close

    background: Rectangle {
        radius: 14
        color: root.theme.surfaceElevated
        border.color: root.theme.border
    }

    contentItem: ScrollView {
        clip: true
        ColumnLayout {
            width: parent.width
            spacing: root.theme.px(14)
            Label {
                Layout.fillWidth: true
                text: root.taskList.selectedTitle
                color: root.theme.textPrimary
                font.pixelSize: root.theme.px(22)
                font.bold: true
                wrapMode: Text.Wrap
            }
            RowLayout {
                Label { text: root.taskList.selectedStatusText; color: root.theme.textSecondary; font.bold: true }
                Label { text: qsTr("%1优先级").arg(root.taskList.selectedPriorityText); color: root.theme.textSecondary }
                Item { Layout.fillWidth: true }
            }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: root.theme.borderSoft }
            Label {
                Layout.fillWidth: true
                text: root.taskList.selectedDescription.length > 0
                      ? root.taskList.selectedDescription : qsTr("暂无描述")
                color: root.taskList.selectedDescription.length > 0
                       ? root.theme.textBody : root.theme.textMuted
                wrapMode: Text.Wrap
                font.pixelSize: root.theme.px(14)
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 24
                rowSpacing: 10
                Label { text: qsTr("截止时间"); color: root.theme.textMuted }
                Label { text: root.taskList.selectedDeadlineText.length > 0 ? root.taskList.selectedDeadlineText : qsTr("未设置"); color: root.theme.textBody }
                Label { text: qsTr("预计用时"); color: root.theme.textMuted }
                Label { text: root.taskList.selectedEstimatedMinutes > 0 ? qsTr("%1 分钟").arg(root.taskList.selectedEstimatedMinutes) : qsTr("未设置"); color: root.theme.textBody }
                Label { text: qsTr("创建时间"); color: root.theme.textMuted }
                Label { text: root.taskList.selectedCreatedAtText; color: root.theme.textBody }
                Label { text: qsTr("更新时间"); color: root.theme.textMuted }
                Label { text: root.taskList.selectedUpdatedAtText; color: root.theme.textBody }
                Label { text: qsTr("前置任务"); color: root.theme.textMuted }
                Label { text: qsTr("%1 项").arg(root.taskList.selectedPredecessorCount); color: root.theme.textBody }
            }
            Rectangle {
                Layout.fillWidth: true
                visible: root.taskList.selectedBlockingReasonText.length > 0
                         || root.taskList.selectedReasonText.length > 0
                implicitHeight: insight.implicitHeight + 24
                radius: 8
                color: root.taskList.selectedBlockingReasonText.length > 0
                       ? "#fff7ed" : root.theme.primarySoft
                Label {
                    id: insight
                    anchors.fill: parent
                    anchors.margins: 12
                    text: root.taskList.selectedBlockingReasonText.length > 0
                          ? qsTr("阻塞：%1").arg(root.taskList.selectedBlockingReasonText)
                          : qsTr("推荐：%1").arg(root.taskList.selectedReasonText)
                    color: root.taskList.selectedBlockingReasonText.length > 0
                           ? root.theme.warning : root.theme.textBody
                    wrapMode: Text.Wrap
                }
            }
            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.fillWidth: true
                Button {
                    visible: root.taskList.selectedCanEditTask
                    text: qsTr("编辑任务")
                    onClicked: root.editRequested(root.taskList.selectedTaskId)
                }
                Button {
                    visible: root.taskList.selectedCanEditDependencies
                    text: qsTr("编辑前置任务")
                    onClicked: root.editDependenciesRequested(root.taskList.selectedTaskId)
                }
                Item { Layout.fillWidth: true }
            }
        }
    }

    onClosed: taskList.clearSelection()
}
