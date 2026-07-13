pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SmartMate.ViewModel 1.0

// 图页只消费 ViewModel 给出的纵向节点和正交路径，并转发稳定 TaskId。
Page {
    id: root
    objectName: "dependencyGraphPage"
    required property AppViewModel appViewModel
    required property AppearanceTheme theme
    signal editDependenciesRequested(string taskId)

    readonly property TaskGraphViewModel graph: appViewModel.taskGraph
    readonly property real panelGap: 12
    readonly property real minimumCanvasWidth: 500
    readonly property real detailsWidth: Math.max(260, Math.min(
        340, contentArea.width * 0.36,
        Math.max(260, contentArea.width - minimumCanvasWidth - panelGap)))
    property bool detailsExpanded: false
    property bool detailsPinned: false
    property bool viewportInitialized: false

    function scheduleSelectedNodeCentering() {
        if (graph.selectedTaskId.length > 0)
            layoutSettleTimer.restart()
    }

    function setZoom(value) {
        graphViewport.zoomFactor = Math.max(0.5, Math.min(2.0, value))
    }

    function fitContent() {
        if (graph.empty || graph.contentWidth <= 0 || graph.contentHeight <= 0) {
            setZoom(1.0)
            return
        }
        const horizontalScale = Math.max(1, graphViewport.width - theme.px(32))
                                / graph.contentWidth
        const verticalScale = Math.max(1, graphViewport.height - theme.px(32))
                              / graph.contentHeight
        setZoom(Math.min(horizontalScale, verticalScale))
        graphViewport.contentX = 0
        graphViewport.contentY = 0
    }

    function centerSelectedNode() {
        if (graph.selectedTaskId.length === 0)
            return
        const targetX = graph.selectedNodeCenterX * graphViewport.zoomFactor
                        - graphViewport.width / 2
        const targetY = graph.selectedNodeCenterY * graphViewport.zoomFactor
                        - graphViewport.height / 2
        graphViewport.contentX = Math.max(0, Math.min(targetX,
            graphViewport.contentWidth - graphViewport.width))
        graphViewport.contentY = Math.max(0, Math.min(targetY,
            graphViewport.contentHeight - graphViewport.height))
    }

    function selectAndCenter(taskId) {
        if (graph.selectTask(taskId)) {
            detailsExpanded = true
            scheduleSelectedNodeCentering()
        }
    }

    function openFullDetails() {
        if (appViewModel.taskList.selectTask(graph.selectedTaskId))
            fullDetailsDialog.open()
    }

    background: Rectangle { color: root.theme.background }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.theme.px(18)
        spacing: root.theme.px(12)

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: toolbarLayout.implicitHeight + root.theme.px(24)
            radius: 12
            color: root.theme.surface
            border.color: root.theme.border

            GridLayout {
                id: toolbarLayout
                anchors.fill: parent
                anchors.margins: root.theme.px(12)
                columns: root.width < root.theme.px(1040) ? 1 : 2
                rowSpacing: root.theme.px(8)
                columnSpacing: root.theme.px(14)

                RowLayout {
                    Layout.fillWidth: true
                    spacing: root.theme.px(10)
                    Label {
                        text: qsTr("依赖图")
                        color: root.theme.textPrimary
                        font.pixelSize: root.theme.px(20)
                        font.bold: true
                    }
                    Rectangle {
                        implicitWidth: taskCountLabel.implicitWidth + root.theme.px(16)
                        implicitHeight: root.theme.px(28)
                        radius: 14; color: root.theme.surfaceStrong
                        Label { id: taskCountLabel; anchors.centerIn: parent; text: qsTr("%1 项任务").arg(root.graph.taskCount); color: root.theme.textSecondary }
                    }
                    Rectangle {
                        implicitWidth: blockedCountLabel.implicitWidth + root.theme.px(16)
                        implicitHeight: root.theme.px(28)
                        radius: 14; color: root.graph.blockedCount > 0 ? root.theme.controlHover : root.theme.surfaceStrong
                        Label { id: blockedCountLabel; anchors.centerIn: parent; text: qsTr("%1 项阻塞").arg(root.graph.blockedCount); color: root.graph.blockedCount > 0 ? root.theme.warning : root.theme.textMuted }
                    }
                    Item { Layout.fillWidth: true }
                    TextField {
                        id: graphSearch
                        objectName: "graphSearchField"
                        Layout.preferredWidth: root.theme.px(210)
                        Layout.minimumWidth: root.theme.px(150)
                        placeholderText: qsTr("搜索并定位任务")
                        text: root.graph.searchText
                        onTextEdited: root.graph.searchText = text
                        onAccepted: {
                            if (root.graph.locateFirstMatch()) {
                                root.detailsExpanded = true
                                root.scheduleSelectedNodeCentering()
                            }
                        }
                    }
                    ComboBox {
                        id: statusFilter
                        objectName: "graphStatusFilter"
                        Layout.preferredWidth: root.theme.px(116)
                        model: [qsTr("全部状态"), qsTr("待办"), qsTr("进行中"), qsTr("阻塞"), qsTr("已完成")]
                        currentIndex: root.graph.statusFilterIndex
                        onActivated: index => root.graph.statusFilterIndex = index
                    }
                    ComboBox {
                        id: graphCategoryFilter
                        objectName: "graphCategoryFilter"
                        Layout.preferredWidth: root.theme.px(158)
                        model: root.graph.categoryFilterOptions
                        textRole: "name"
                        valueRole: "categoryId"
                        currentIndex: root.graph.categoryFilterMode === 0 ? 0
                                      : root.graph.categoryFilterMode === 1 ? 1
                                      : graphCategoryFilter.indexOfValue(
                                            root.graph.categoryFilterCategoryId)
                        onActivated: index => {
                            const option = root.graph.categoryFilterOptions[index]
                            root.graph.setCategoryFilter(option.mode,
                                                         option.categoryId)
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignRight
                    spacing: root.theme.px(6)
                    Button {
                        objectName: "locateCurrentGraphTaskButton"
                        text: qsTr("定位当前")
                        enabled: root.graph.currentTaskId.length > 0
                        onClicked: {
                            if (root.graph.selectCurrentTask()) {
                                root.detailsExpanded = true
                                root.scheduleSelectedNodeCentering()
                            }
                        }
                    }
                    ToolButton { objectName: "zoomOutGraphButton"; text: "−"; enabled: graphViewport.zoomFactor > 0.5; onClicked: root.setZoom(graphViewport.zoomFactor - 0.1) }
                    Label { objectName: "graphZoomLabel"; Layout.preferredWidth: root.theme.px(52); horizontalAlignment: Text.AlignHCenter; text: qsTr("%1%").arg(Math.round(graphViewport.zoomFactor * 100)); color: root.theme.textSecondary }
                    ToolButton { objectName: "zoomInGraphButton"; text: "+"; enabled: graphViewport.zoomFactor < 2.0; onClicked: root.setZoom(graphViewport.zoomFactor + 0.1) }
                    Button { objectName: "resetGraphZoomButton"; text: qsTr("100%"); onClicked: root.setZoom(1.0) }
                    Button { objectName: "fitGraphButton"; text: qsTr("适应画布"); onClicked: root.fitContent() }
                    ToolButton { objectName: "reloadGraphButton"; text: "↻"; ToolTip.visible: hovered; ToolTip.text: qsTr("刷新"); onClicked: root.graph.reload() }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.graph.errorMessage.length > 0
            text: root.graph.errorMessage
            color: root.theme.danger
            wrapMode: Text.Wrap
        }

        Item {
            id: contentArea
            Layout.fillWidth: true
            Layout.fillHeight: true

            Rectangle {
                id: canvasFrame
                objectName: "dependencyGraphCanvasFrame"
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.rightMargin: root.detailsExpanded
                                     ? root.detailsWidth + root.panelGap : 0
                radius: 12
                color: root.theme.surface
                border.color: root.theme.border
                clip: true
                Behavior on anchors.rightMargin {
                    NumberAnimation {
                        duration: 180
                        easing.type: Easing.OutCubic
                        onFinished: root.scheduleSelectedNodeCentering()
                    }
                }

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
                        x: Math.max(0, (graphViewport.width - width * graphViewport.zoomFactor) / 2)
                        y: Math.max(0, (graphViewport.height - height * graphViewport.zoomFactor) / 2)
                        width: root.graph.contentWidth
                        height: root.graph.contentHeight
                        scale: graphViewport.zoomFactor
                        transformOrigin: Item.TopLeft

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                root.graph.clearSelection()
                                if (!root.detailsPinned) root.detailsExpanded = false
                            }
                        }

                        Repeater {
                            id: graphEdgeRepeater
                            objectName: "dependencyGraphEdgeRepeater"
                            model: root.graph.edges
                            delegate: DependencyGraphEdge {
                                width: graphCanvas.width
                                height: graphCanvas.height
                                theme: root.theme
                            }
                        }

                        Repeater {
                            id: graphNodeRepeater
                            objectName: "dependencyGraphNodeRepeater"
                            model: root.graph
                            delegate: DependencyGraphNode {
                                theme: root.theme
                                onSelectedRequested: taskId =>
                                    root.selectAndCenter(taskId)
                                onHoverRequested: (taskId, hovered) => {
                                    if (hovered)
                                        root.graph.setHoveredTask(taskId)
                                    else
                                        root.graph.clearHoveredTask()
                                }
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
                    text: root.graph.categoryFilterMode === 0
                          ? qsTr("还没有可显示的活动任务")
                          : qsTr("当前类别没有可显示的任务")
                    color: root.theme.textMuted
                    font.pixelSize: root.theme.px(16)
                }

                Button {
                    objectName: "openGraphDetailsButton"
                    visible: !root.detailsExpanded && root.graph.selectedTaskId.length > 0
                    anchors.right: parent.right; anchors.top: parent.top; anchors.margins: root.theme.px(12)
                    text: qsTr("任务详情 ‹")
                    onClicked: root.detailsExpanded = true
                }
            }

            Rectangle {
                id: detailsPanel
                objectName: "dependencyGraphDetails"
                visible: root.detailsExpanded
                width: root.detailsWidth
                anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.right: parent.right
                z: 1
                radius: 12
                color: root.theme.surfaceElevated
                border.color: root.theme.border
                Behavior on opacity { NumberAnimation { duration: 160 } }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: root.theme.px(16)
                    spacing: root.theme.px(10)
                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: qsTr("任务详情"); color: root.theme.textPrimary; font.pixelSize: root.theme.px(18); font.bold: true }
                        ToolButton { objectName: "pinGraphDetailsButton"; checkable: true; checked: root.detailsPinned; text: checked ? "●" : "○"; ToolTip.visible: hovered; ToolTip.text: qsTr("固定面板"); onToggled: root.detailsPinned = checked }
                        ToolButton { objectName: "collapseGraphDetailsButton"; text: "›"; onClicked: root.detailsExpanded = false }
                    }
                    ScrollView {
                        id: detailsScroll
                        objectName: "graphDetailsScrollView"
                        Layout.fillWidth: true; Layout.fillHeight: true
                        contentWidth: availableWidth
                        clip: true; ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        ColumnLayout {
                            width: detailsScroll.availableWidth
                            spacing: root.theme.px(10)
                            Label { objectName: "selectedGraphTaskTitle"; Layout.fillWidth: true; Layout.minimumWidth: 0; text: root.graph.selectedTaskTitle; color: root.theme.textPrimary; font.pixelSize: root.theme.px(17); font.bold: true; wrapMode: Text.Wrap }
                            RowLayout {
                                Layout.fillWidth: true
                                visible: root.graph.selectedHasCategory
                                         || !root.graph.selectedCoreNode
                                Rectangle {
                                    visible: root.graph.selectedHasCategory
                                    Layout.maximumWidth: root.theme.px(150)
                                    implicitWidth: Math.min(
                                        root.theme.px(150),
                                        selectedGraphCategoryLabel.implicitWidth + 16)
                                    implicitHeight: selectedGraphCategoryLabel.implicitHeight + 7
                                    radius: height / 2
                                    color: Qt.alpha(root.graph.selectedCategoryAccent, 0.12)
                                    border.color: root.graph.selectedCategoryAccent
                                    Label {
                                        id: selectedGraphCategoryLabel
                                        anchors.fill: parent
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 8
                                        verticalAlignment: Text.AlignVCenter
                                        text: root.graph.selectedCategoryName
                                        color: root.graph.selectedCategoryAccent
                                        font.pixelSize: root.theme.px(11)
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                }
                                Label {
                                    visible: !root.graph.selectedCoreNode
                                    text: qsTr("跨类别上下文")
                                    color: root.theme.textMuted
                                    font.pixelSize: root.theme.px(11)
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                            }
                            Label { Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTr("%1 · %2优先级").arg(root.graph.selectedStatusText).arg(root.graph.selectedPriorityText); color: root.theme.textSecondary; wrapMode: Text.Wrap }
                            Label { Layout.fillWidth: true; Layout.minimumWidth: 0; text: root.graph.selectedDescription.length > 0 ? root.graph.selectedDescription : qsTr("暂无描述"); color: root.graph.selectedDescription.length > 0 ? root.theme.textBody : root.theme.textMuted; wrapMode: Text.Wrap; maximumLineCount: 4; elide: Text.ElideRight }
                            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: root.theme.borderSoft }
                            Label { Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTr("截止时间  %1").arg(root.graph.selectedDeadlineText); color: root.theme.textSecondary; wrapMode: Text.Wrap }
                            Label { Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTr("预计用时  %1").arg(root.graph.selectedEstimatedDurationText); color: root.theme.textSecondary; wrapMode: Text.Wrap }
                            Label { objectName: "selectedGraphTaskRelations"; Layout.fillWidth: true; Layout.minimumWidth: 0; text: qsTr("直接前置 %1 项 · 直接后继 %2 项 · 可解锁 %3 项").arg(root.graph.selectedPredecessorCount).arg(root.graph.selectedSuccessorCount).arg(root.graph.selectedUnlockCount); color: root.theme.textSecondary; wrapMode: Text.Wrap }
                            Rectangle {
                                Layout.fillWidth: true
                                visible: root.graph.selectedBlockingReason.length > 0
                                implicitHeight: blockingLabel.implicitHeight + root.theme.px(20)
                                radius: 8; color: root.theme.controlHover
                                Label { id: blockingLabel; objectName: "selectedGraphTaskBlockingReason"; anchors.fill: parent; anchors.margins: root.theme.px(10); text: qsTr("阻塞：%1").arg(root.graph.selectedBlockingReason); color: root.theme.warning; wrapMode: Text.Wrap }
                            }

                            component RelationList: ColumnLayout {
                                required property string heading
                                required property var relationModel
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 5
                                Label { text: parent.heading; color: root.theme.textPrimary; font.bold: true }
                                Repeater {
                                    model: parent.relationModel
                                    delegate: Button {
                                        id: relationButton
                                        required property string taskId
                                        required property string title
                                        required property string statusText
                                        required property string relationText
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        implicitHeight: root.theme.px(48)
                                        contentItem: ColumnLayout {
                                            spacing: 1
                                            Label { Layout.fillWidth: true; Layout.minimumWidth: 0; text: relationButton.title; color: root.theme.textPrimary; elide: Text.ElideRight }
                                            Label { Layout.fillWidth: true; Layout.minimumWidth: 0; text: relationButton.statusText + " · " + relationButton.relationText; color: root.theme.textMuted; font.pixelSize: root.theme.px(11); elide: Text.ElideRight }
                                        }
                                        onClicked: root.selectAndCenter(relationButton.taskId)
                                    }
                                }
                            }
                            RelationList { heading: qsTr("直接前置"); relationModel: root.graph.selectedPredecessors; visible: root.graph.selectedPredecessorCount > 0 }
                            RelationList { heading: qsTr("直接后继"); relationModel: root.graph.selectedSuccessors; visible: root.graph.selectedSuccessorCount > 0 }
                        }
                    }
                    Button { Layout.fillWidth: true; text: qsTr("在画布中居中"); onClicked: root.centerSelectedNode() }
                    Button { objectName: "openSelectedGraphTaskDetailsButton"; Layout.fillWidth: true; text: qsTr("查看完整详情"); onClicked: root.openFullDetails() }
                    Button { objectName: "editSelectedGraphDependenciesButton"; Layout.fillWidth: true; visible: root.graph.canEditSelectedDependencies; text: qsTr("编辑前置任务"); onClicked: root.editDependenciesRequested(root.graph.selectedTaskId) }
                }
            }
        }
    }

    TaskDetailsDialog {
        id: fullDetailsDialog
        anchors.centerIn: Overlay.overlay
        taskList: root.appViewModel.taskList
        theme: root.theme
        actionsVisible: false
    }

    Timer {
        id: layoutSettleTimer
        interval: 210
        repeat: false
        onTriggered: root.centerSelectedNode()
    }

    Connections {
        target: root.graph
        function onSelectionChanged() {
            if (root.graph.selectedTaskId.length > 0)
                root.detailsExpanded = true
            else if (!root.detailsPinned)
                root.detailsExpanded = false
        }
        function onCategoryFilterChanged() {
            Qt.callLater(root.fitContent)
        }
    }

    onVisibleChanged: {
        if (visible && !viewportInitialized) {
            viewportInitialized = true
            Qt.callLater(fitContent)
        }
    }
    onWidthChanged: {
        if (detailsExpanded)
            scheduleSelectedNodeCentering()
    }
    onDetailsExpandedChanged: scheduleSelectedNodeCentering()
}
