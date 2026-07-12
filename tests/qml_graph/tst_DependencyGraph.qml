import QtQuick
import QtQuick.Controls
import QtTest
import SmartMate.View

TestCase {
    id: testCase

    name: "DependencyGraph"
    when: windowShown

    property var subject: null

    Component {
        id: mainComponent

        Main {
            width: 1180
            height: 760
            visible: true
            appViewModel: graphTestAppViewModel
        }
    }

    function showGraphPage() {
        const tabs = findChild(subject, "mainNavigationTabs")
        verify(tabs !== null)
        tabs.currentIndex = 1
        const page = findChild(subject, "dependencyGraphPage")
        verify(page !== null)
        tryCompare(page, "visible", true)
        return page
    }

    function creationCandidateDelegate(dialog, taskId) {
        const listView = findChild(dialog, "creationPredecessorCandidateList")
        if (listView === null)
            return null
        for (let row = 0; row < listView.count; ++row) {
            listView.positionViewAtIndex(row, ListView.Contain)
            const delegate = listView.itemAtIndex(row)
            if (delegate !== null && delegate.candidateTaskId === taskId)
                return delegate
        }
        return null
    }

    function initTestCase() {
        verify(graphTestAppViewModel !== null)
        subject = mainComponent.createObject(testCase)
        verify(subject !== null)
        tryCompare(subject, "visible", true)
        graphTestAppViewModel.taskGraph.reload()
    }

    function cleanupTestCase() {
        if (subject !== null) {
            subject.close()
            subject.destroy()
            subject = null
        }
    }

    function cleanup() {
        const dialog = findChild(subject, "taskDependencyDialog")
        if (dialog !== null && dialog.visible) {
            graphTestAppViewModel.taskDependencies.cancel()
            dialog.close()
        }
        if (!graphTestAppViewModel.taskEditor.editMode)
            graphTestAppViewModel.taskEditor.cancel()
        const taskEditorDialog = findChild(subject, "taskEditorDialog")
        if (taskEditorDialog !== null && taskEditorDialog.visible)
            taskEditorDialog.close()
        graphTestAppViewModel.taskGraph.clearSelection()
    }

    function test_tabExposesDirectedGraphPage() {
        const tabs = findChild(subject, "mainNavigationTabs")
        verify(tabs !== null)
        compare(tabs.count, 3)

        const page = showGraphPage()
        verify(page.visible)
    }

    function test_nodesAndArrowUseViewModelGeometry() {
        showGraphPage()
        const nodeRepeater = findChild(subject, "dependencyGraphNodeRepeater")
        const edgeRepeater = findChild(subject, "dependencyGraphEdgeRepeater")
        verify(nodeRepeater !== null)
        verify(edgeRepeater !== null)
        tryCompare(nodeRepeater, "count", 2)
        tryCompare(edgeRepeater, "count", 1)

        const edge = edgeRepeater.itemAt(0)
        verify(edge !== null)
        compare(edge.predecessorId, graphPredecessorId)
        compare(edge.successorId, graphSuccessorId)
        verify(edge.routePoints.length >= 2)
        const start = edge.routePoints[0]
        const end = edge.routePoints[edge.routePoints.length - 1]
        verify(end.y > start.y,
               "Finish-to-Start 箭头必须从前置节点向下指向后继节点")
        compare(edge.arrowTipX, end.x)
        compare(edge.arrowTipY, end.y)
    }

    function test_cancelledEdgeUsesGreyProjectionAndRedoReactivatesIt() {
        showGraphPage()
        const edgeRepeater = findChild(subject, "dependencyGraphEdgeRepeater")
        verify(edgeRepeater !== null)
        tryCompare(edgeRepeater, "count", 1)
        let edge = edgeRepeater.itemAt(0)
        verify(edge !== null)
        compare(edge.cancelled, false)
        compare(edge.satisfied, false)

        verify(graphTestAppViewModel.taskList.cancelTask(graphPredecessorId),
               graphTestAppViewModel.taskList.errorMessage)
        tryVerify(function() {
            const current = edgeRepeater.itemAt(0)
            return current !== null && current.cancelled
        })
        edge = edgeRepeater.itemAt(0)
        compare(edge.edgeColor, "#98a2b3")

        // 重做只改变任务状态，SQLite中的边没有被删除，图投影重新回到待满足。
        verify(graphTestAppViewModel.taskList.redoTask(graphPredecessorId),
               graphTestAppViewModel.taskList.errorMessage)
        tryVerify(function() {
            const current = edgeRepeater.itemAt(0)
            return current !== null && !current.cancelled && !current.satisfied
        })
    }

    function test_zoomIsClampedAndCanResetToOneHundredPercent() {
        const page = showGraphPage()
        const viewport = findChild(subject, "dependencyGraphViewport")
        const fitButton = findChild(subject, "fitGraphButton")
        const resetButton = findChild(subject, "resetGraphZoomButton")
        verify(viewport !== null)
        verify(fitButton !== null)
        verify(resetButton !== null)

        page.setZoom(0.1)
        compare(viewport.zoomFactor, 0.5)
        page.setZoom(3.0)
        compare(viewport.zoomFactor, 2.0)
        fitButton.clicked()
        verify(viewport.zoomFactor >= 0.5)
        verify(viewport.zoomFactor <= 2.0)
        resetButton.clicked()
        compare(viewport.zoomFactor, 1.0)
    }

    function test_zoomStateSurvivesTabSwitch() {
        const page = showGraphPage()
        const tabs = findChild(subject, "mainNavigationTabs")
        const viewport = findChild(subject, "dependencyGraphViewport")
        verify(tabs !== null)
        verify(viewport !== null)

        page.setZoom(1.4)
        tabs.currentIndex = 0
        tabs.currentIndex = 1
        tryCompare(page, "visible", true)
        compare(viewport.zoomFactor, 1.4)
    }

    function test_creationPredecessorPickerCancelAndAcceptUseStableId() {
        const tabs = findChild(subject, "mainNavigationTabs")
        verify(tabs !== null)
        tabs.currentIndex = 0

        const newButton = findChild(subject, "newTaskButton")
        verify(newButton !== null)
        newButton.clicked()

        const openButton = findChild(subject, "openCreationPredecessorButton")
        verify(openButton !== null)
        tryCompare(openButton, "visible", true)
        openButton.clicked()

        const picker = findChild(subject, "taskCreationPredecessorDialog")
        verify(picker !== null)
        tryCompare(picker, "visible", true)
        tryVerify(function() {
            return creationCandidateDelegate(picker, graphPredecessorId) !== null
        })

        let candidate = creationCandidateDelegate(picker, graphPredecessorId)
        let checkBox = findChild(candidate,
                                 "creationPredecessorCheckBox_"
                                 + graphPredecessorId)
        const cancelButton = findChild(picker,
                                       "cancelCreationPredecessorsButton")
        verify(checkBox !== null)
        verify(cancelButton !== null)
        mouseClick(checkBox)
        tryCompare(graphTestAppViewModel.taskEditor,
                   "selectedPredecessorCount", 1)
        mouseClick(cancelButton)
        tryCompare(graphTestAppViewModel.taskEditor,
                   "selectedPredecessorCount", 0)

        openButton.clicked()
        tryCompare(picker, "visible", true)
        candidate = creationCandidateDelegate(picker, graphPredecessorId)
        verify(candidate !== null)
        checkBox = findChild(candidate,
                             "creationPredecessorCheckBox_"
                             + graphPredecessorId)
        const acceptButton = findChild(picker,
                                       "acceptCreationPredecessorsButton")
        verify(checkBox !== null)
        verify(acceptButton !== null)
        mouseClick(checkBox)
        mouseClick(acceptButton)
        tryCompare(graphTestAppViewModel.taskEditor,
                   "selectedPredecessorCount", 1)
    }

    function test_nodeDetailsOpenSharedDependencyEditorByStableId() {
        const page = showGraphPage()
        page.selectAndCenter(graphSuccessorId)

        const title = findChild(subject, "selectedGraphTaskTitle")
        const relations = findChild(subject, "selectedGraphTaskRelations")
        const blockingReason = findChild(subject,
                                         "selectedGraphTaskBlockingReason")
        const editButton = findChild(subject,
                                     "editSelectedGraphDependenciesButton")
        verify(title !== null)
        verify(relations !== null)
        verify(blockingReason !== null)
        verify(editButton !== null)
        tryCompare(page, "detailsExpanded", true)
        tryCompare(title, "text", "实现任务模块")
        verify(relations.text.indexOf("1") >= 0)
        verify(blockingReason.text.indexOf("需求分析") >= 0)
        tryCompare(editButton, "visible", true)

        editButton.clicked()
        const dialog = findChild(subject, "taskDependencyDialog")
        verify(dialog !== null)
        tryCompare(dialog, "visible", true)
        compare(graphTestAppViewModel.taskDependencies.taskId,
                graphSuccessorId)
    }

    function test_toolbarSearchFilterAndResponsiveDetailsPanel() {
        const page = showGraphPage()
        const search = findChild(subject, "graphSearchField")
        const filter = findChild(subject, "graphStatusFilter")
        const details = findChild(subject, "dependencyGraphDetails")
        const collapse = findChild(subject, "collapseGraphDetailsButton")
        verify(search !== null)
        verify(filter !== null)
        verify(details !== null)
        verify(collapse !== null)

        search.text = "实现任务"
        search.textEdited()
        search.accepted()
        tryCompare(graphTestAppViewModel.taskGraph, "selectedTaskId", graphSuccessorId)
        tryCompare(details, "visible", true)
        compare(page.narrowDetails, false)

        filter.activated(3)
        tryCompare(graphTestAppViewModel.taskGraph, "statusFilterIndex", 3)
        collapse.clicked()
        tryCompare(details, "visible", false)

        subject.width = 900
        tryCompare(page, "narrowDetails", true)
        page.selectAndCenter(graphSuccessorId)
        tryCompare(details, "visible", true)
        verify(details.width <= page.width * 0.8 + 1)
        subject.width = 1180
        graphTestAppViewModel.taskGraph.searchText = ""
        graphTestAppViewModel.taskGraph.statusFilterIndex = 0
    }
}
