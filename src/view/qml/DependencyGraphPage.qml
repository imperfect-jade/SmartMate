pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import SmartMate.ViewModel 1.0

// 图页只绘制ViewModel提供的节点坐标和箭头几何；拓扑遍历与边状态判断不在QML中执行。
Page {
    id: root
    objectName: "dependencyGraphPage"

    required property AppViewModel appViewModel
    signal editDependenciesRequested(string taskId)

    readonly property TaskGraphViewModel graph: appViewModel.taskGraph
    property bool viewportInitialized: false

    function setZoom(value) {
        graphViewport.zoomFactor = Math.max(0.5, Math.min(2.0, value))
    }

    function fitContent() {
        if (root.graph.empty || root.graph.contentWidth <= 0
                || root.graph.contentHeight <= 0) {
            root.setZoom(1.0)
            return
        }

        const horizontalScale = Math.max(1, graphViewport.width - 32)
                                / root.graph.contentWidth
        const verticalScale = Math.max(1, graphViewport.height - 32)
                              / root.graph.contentHeight
        root.setZoom(Math.min(horizontalScale, verticalScale))
        graphViewport.contentX = 0
        graphViewport.contentY = 0
    }

    background: Rectangle { color: "#f5f7fb" }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: qsTr("Finish-to-Start 依赖图")
                color: "#172033"
                font.pixelSize: 20
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            Button {
                objectName: "reloadGraphButton"
                text: qsTr("刷新")
                onClicked: root.graph.reload()
            }

            Button {
                objectName: "fitGraphButton"
                text: qsTr("适应窗口")
                onClicked: root.fitContent()
            }

            Button {
                objectName: "resetGraphZoomButton"
                text: qsTr("100%")
                onClicked: root.setZoom(1.0)
            }

            Button {
                objectName: "zoomOutGraphButton"
                text: "−"
                enabled: graphViewport.zoomFactor > 0.5
                onClicked: root.setZoom(graphViewport.zoomFactor - 0.1)
            }

            Label {
                objectName: "graphZoomLabel"
                Layout.preferredWidth: 52
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("%1%").arg(Math.round(graphViewport.zoomFactor * 100))
                color: "#475467"
            }

            Button {
                objectName: "zoomInGraphButton"
                text: "+"
                enabled: graphViewport.zoomFactor < 2.0
                onClicked: root.setZoom(graphViewport.zoomFactor + 0.1)
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.graph.errorMessage.length > 0
            text: root.graph.errorMessage
            color: "#b42318"
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 480
                radius: 12
                color: "#ffffff"
                border.color: "#d0d5dd"
                clip: true

                Flickable {
                    id: graphViewport
                    objectName: "dependencyGraphViewport"

                    property real zoomFactor: 1.0

                    anchors.fill: parent
                    anchors.margins: 1
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    contentWidth: Math.max(width, root.graph.contentWidth * zoomFactor)
                    contentHeight: Math.max(height, root.graph.contentHeight * zoomFactor)

                    Item {
                        id: graphCanvas
                        objectName: "dependencyGraphCanvas"

                        x: Math.max(0, (graphViewport.width
                                       - width * graphViewport.zoomFactor) / 2)
                        y: Math.max(0, (graphViewport.height
                                       - height * graphViewport.zoomFactor) / 2)
                        width: root.graph.contentWidth
                        height: root.graph.contentHeight
                        scale: graphViewport.zoomFactor
                        transformOrigin: Item.TopLeft

                        // 每条边的曲线和箭头三点均由TaskGraphViewModel预先计算。
                        Repeater {
                            id: graphEdgeRepeater
                            objectName: "dependencyGraphEdgeRepeater"
                            model: root.graph.edges

                            delegate: Shape {
                                id: edgeShape

                                required property string predecessorId
                                required property string successorId
                                required property real startX
                                required property real startY
                                required property real control1X
                                required property real control1Y
                                required property real control2X
                                required property real control2Y
                                required property real endX
                                required property real endY
                                required property real arrowTipX
                                required property real arrowTipY
                                required property real arrowLeftX
                                required property real arrowLeftY
                                required property real arrowRightX
                                required property real arrowRightY
                                required property bool satisfied
                                required property bool highlighted

                                readonly property color edgeColor: satisfied
                                                                   ? "#12b76a"
                                                                   : "#f79009"

                                objectName: "graphEdge_" + predecessorId + "_" + successorId
                                width: graphCanvas.width
                                height: graphCanvas.height

                                ShapePath {
                                    strokeColor: edgeShape.edgeColor
                                    strokeWidth: edgeShape.highlighted ? 4 : 2.5
                                    fillColor: "transparent"
                                    capStyle: ShapePath.RoundCap
                                    startX: edgeShape.startX
                                    startY: edgeShape.startY

                                    PathCubic {
                                        control1X: edgeShape.control1X
                                        control1Y: edgeShape.control1Y
                                        control2X: edgeShape.control2X
                                        control2Y: edgeShape.control2Y
                                        x: edgeShape.endX
                                        y: edgeShape.endY
                                    }
                                }

                                ShapePath {
                                    strokeColor: edgeShape.edgeColor
                                    strokeWidth: 1
                                    fillColor: edgeShape.edgeColor
                                    startX: edgeShape.arrowTipX
                                    startY: edgeShape.arrowTipY

                                    PathLine {
                                        x: edgeShape.arrowLeftX
                                        y: edgeShape.arrowLeftY
                                    }
                                    PathLine {
                                        x: edgeShape.arrowRightX
                                        y: edgeShape.arrowRightY
                                    }
                                    PathLine {
                                        x: edgeShape.arrowTipX
                                        y: edgeShape.arrowTipY
                                    }
                                }
                            }
                        }

                        // 节点在边之后声明以保持可读性和点击区域不被连线遮挡。
                        Repeater {
                            id: graphNodeRepeater
                            objectName: "dependencyGraphNodeRepeater"
                            model: root.graph

                            delegate: Rectangle {
                                id: nodeDelegate

                                required property string taskId
                                required property string shortId
                                required property string title
                                required property string statusText
                                required property string priorityText
                                required property bool blocked
                                required property string blockingReasonText
                                required property bool archived
                                required property bool canEditDependencies
                                required property real nodeX
                                required property real nodeY
                                required property real nodeWidth
                                required property real nodeHeight
                                required property bool selected

                                objectName: "graphNode_" + taskId
                                x: nodeX
                                y: nodeY
                                width: nodeWidth
                                height: nodeHeight
                                radius: 9
                                color: archived ? "#f2f4f7" : "#ffffff"
                                border.width: selected ? 3 : 2
                                border.color: selected ? "#2e90fa"
                                                       : blocked ? "#f79009"
                                                                 : archived ? "#98a2b3"
                                                                            : "#84caff"

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 11
                                    spacing: 4

                                    Label {
                                        Layout.fillWidth: true
                                        text: nodeDelegate.title
                                        color: nodeDelegate.archived ? "#667085" : "#172033"
                                        font.bold: true
                                        font.pixelSize: 15
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("%1 · %2优先级")
                                              .arg(nodeDelegate.statusText)
                                              .arg(nodeDelegate.priorityText)
                                        color: "#475467"
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: nodeDelegate.blocked
                                              ? qsTr("已阻塞 · ID %1").arg(nodeDelegate.shortId)
                                              : qsTr("ID %1").arg(nodeDelegate.shortId)
                                        color: nodeDelegate.blocked ? "#b54708" : "#98a2b3"
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.graph.selectTask(nodeDelegate.taskId)
                                }

                                ToolTip.visible: nodeHover.hovered
                                                 && nodeDelegate.blockingReasonText.length > 0
                                ToolTip.text: nodeDelegate.blockingReasonText
                                ToolTip.delay: 450
                                HoverHandler { id: nodeHover }
                            }
                        }
                    }

                    ScrollBar.horizontal: ScrollBar { }
                    ScrollBar.vertical: ScrollBar { }
                }

                Label {
                    objectName: "dependencyGraphEmptyState"
                    anchors.centerIn: parent
                    visible: root.graph.empty && root.graph.errorMessage.length === 0
                    text: qsTr("还没有可显示的活动任务")
                    color: "#667085"
                    font.pixelSize: 16
                }
            }

            Rectangle {
                objectName: "dependencyGraphDetails"
                Layout.preferredWidth: 300
                Layout.fillHeight: true
                radius: 12
                color: "#ffffff"
                border.color: "#d0d5dd"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("任务详情")
                        color: "#172033"
                        font.pixelSize: 18
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: root.graph.selectedTaskId.length === 0
                        text: qsTr("选择一个节点查看直接前置、后续和阻塞信息。")
                        color: "#667085"
                        wrapMode: Text.Wrap
                    }

                    Label {
                        objectName: "selectedGraphTaskTitle"
                        Layout.fillWidth: true
                        visible: root.graph.selectedTaskId.length > 0
                        text: root.graph.selectedTaskTitle
                        color: "#172033"
                        font.pixelSize: 17
                        font.bold: true
                        wrapMode: Text.Wrap
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: root.graph.selectedTaskId.length > 0
                        text: qsTr("%1 · %2优先级")
                              .arg(root.graph.selectedStatusText)
                              .arg(root.graph.selectedPriorityText)
                        color: "#475467"
                    }

                    Label {
                        objectName: "selectedGraphTaskRelations"
                        Layout.fillWidth: true
                        visible: root.graph.selectedTaskId.length > 0
                        text: qsTr("直接前置 %1 项\n直接后续 %2 项")
                              .arg(root.graph.selectedPredecessorCount)
                              .arg(root.graph.selectedSuccessorCount)
                        color: "#475467"
                        lineHeight: 1.35
                    }

                    Label {
                        objectName: "selectedGraphTaskBlockingReason"
                        Layout.fillWidth: true
                        visible: root.graph.selectedBlockingReason.length > 0
                        text: qsTr("阻塞原因：%1").arg(root.graph.selectedBlockingReason)
                        color: "#b54708"
                        wrapMode: Text.Wrap
                    }

                    Item { Layout.fillHeight: true }

                    Button {
                        objectName: "editSelectedGraphDependenciesButton"
                        Layout.fillWidth: true
                        visible: root.graph.canEditSelectedDependencies
                        text: qsTr("编辑前置任务")
                        onClicked: root.editDependenciesRequested(root.graph.selectedTaskId)
                    }

                    Button {
                        Layout.fillWidth: true
                        visible: root.graph.selectedTaskId.length > 0
                        text: qsTr("清除选择")
                        onClicked: root.graph.clearSelection()
                    }
                }
            }
        }
    }

    onVisibleChanged: {
        // 只在首次进入图页时自动适配；之后切换标签不会覆盖用户选择的缩放和平移状态。
        if (visible && !viewportInitialized) {
            viewportInitialized = true
            Qt.callLater(root.fitContent)
        }
    }
}
