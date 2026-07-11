import QtQuick
import QtQuick.Controls
import QtTest
import SmartMate.View

TestCase {
    id: testCase

    name: "TaskListControls"
    when: windowShown
    property var subject: null
    property string alphaId
    property string betaId
    property string gammaId

    Component {
        id: taskPageComponent

        Main {
            width: 1000
            height: 760
            visible: true
            appViewModel: testAppViewModel
        }
    }

    function createTask(title, description, priorityIndex) {
        const editor = testAppViewModel.taskEditor
        editor.beginCreate()
        editor.title = title
        editor.description = description
        editor.priorityIndex = priorityIndex
        verify(editor.save(), editor.errorMessage)
        return editor.taskId
    }

    function clearDependencies(taskId) {
        const editor = testAppViewModel.taskDependencies
        if (!editor.beginEdit(taskId))
            return
        const taskIds = [alphaId, betaId, gammaId]
        for (let index = 0; index < taskIds.length; ++index) {
            if (taskIds[index] !== taskId)
                verify(editor.setPredecessorSelected(taskIds[index], false))
        }
        if (editor.dirty)
            verify(editor.save(), editor.errorMessage)
    }

    function saveSinglePredecessor(taskId, predecessorId) {
        const editor = testAppViewModel.taskDependencies
        verify(editor.beginEdit(taskId), editor.errorMessage)
        verify(editor.setPredecessorSelected(predecessorId, true))
        verify(editor.save(), editor.errorMessage)
    }

    function taskDelegate(taskId) {
        const listView = findChild(subject, "taskListView")
        if (listView === null)
            return null
        for (let row = 0; row < listView.count; ++row) {
            listView.positionViewAtIndex(row, ListView.Contain)
            const delegate = listView.itemAtIndex(row)
            if (delegate !== null && delegate.taskId === taskId)
                return delegate
        }
        return null
    }

    function dependencyCandidateDelegate(dialog, taskId) {
        const listView = findChild(dialog, "dependencyCandidateList")
        if (listView === null)
            return null
        for (let row = 0; row < listView.count; ++row) {
            listView.positionViewAtIndex(row, ListView.Contain)
            const delegate = listView.itemAtIndex(row)
            if (delegate !== null && delegate.taskId === taskId)
                return delegate
        }
        return null
    }

    function initTestCase() {
        verify(testAppViewModel !== null)
        alphaId = createTask("Alpha 架构设计", "Model planner 说明", 3)
        betaId = createTask("Beta 文档", "整理使用手册", 0)
        gammaId = createTask("Gamma 测试", "覆盖筛选条件", 2)

        subject = taskPageComponent.createObject(testCase)
        verify(subject !== null)
        tryCompare(subject, "visible", true)
    }

    function cleanupTestCase() {
        if (subject !== null) {
            subject.close()
            subject.destroy()
            subject = null
        }
    }

    function init() {
        testAppViewModel.appearanceSettings.resetDefaults()
        testAppViewModel.taskList.showArchived = false
        testAppViewModel.taskList.clearFilters()
        compare(testAppViewModel.taskList.count, 3)
    }

    function cleanup() {
        // 每个依赖用例都恢复三项任务的空关系，避免共享内存数据库污染其他测试。
        clearDependencies(alphaId)
        clearDependencies(betaId)
        clearDependencies(gammaId)
    }

    function test_searchFieldForwardsEditedText() {
        const searchField = findChild(subject, "taskSearchField")
        verify(searchField !== null)
        tryCompare(searchField, "text", "")

        // 直接触发控件的用户编辑信号，验证 QML 只把当前文本转发给 ViewModel。
        searchField.text = "PLANNER"
        searchField.textEdited()

        tryCompare(testAppViewModel.taskList, "searchText", "PLANNER")
        compare(testAppViewModel.taskList.count, 1)
    }

    function test_priorityComboForwardsSelection() {
        const priorityCombo = findChild(subject, "priorityFilterComboBox")
        verify(priorityCombo !== null)

        priorityCombo.activated(4)

        tryCompare(testAppViewModel.taskList, "priorityFilterIndex", 4)
        compare(testAppViewModel.taskList.count, 1)
    }

    function test_clearButtonClearsBothConditions() {
        const clearButton = findChild(subject, "clearFiltersButton")
        verify(clearButton !== null)
        testAppViewModel.taskList.searchText = "架构"
        testAppViewModel.taskList.priorityFilterIndex = 4
        tryCompare(clearButton, "visible", true)

        clearButton.clicked()

        compare(testAppViewModel.taskList.searchText, "")
        compare(testAppViewModel.taskList.priorityFilterIndex, 0)
        compare(testAppViewModel.taskList.count, 3)
        tryCompare(clearButton, "visible", false)
    }

    function test_emptyStateDistinguishesFilteredResult() {
        const emptyState = findChild(subject, "taskEmptyStateLabel")
        verify(emptyState !== null)

        testAppViewModel.taskList.searchText = "不存在的任务"

        compare(testAppViewModel.taskList.count, 0)
        tryCompare(emptyState, "visible", true)
        verify(emptyState.text.indexOf("没有符合") >= 0)
    }

    function test_navigationAndAppearanceSettings() {
        const theme = findChild(subject, "appearanceTheme")
        const tabs = findChild(subject, "mainNavigationTabs")
        const focusNav = findChild(subject, "focusNavigationButton")
        const statsNav = findChild(subject, "statisticsNavigationButton")
        const settingsNav = findChild(subject, "settingsNavigationButton")
        verify(theme !== null)
        verify(tabs !== null)
        verify(focusNav !== null)
        verify(statsNav !== null)
        verify(settingsNav !== null)
        compare(focusNav.enabled, false)
        compare(statsNav.enabled, false)

        const greenBackground = theme.background.toString()
        const greenNavigation = theme.navigation.toString()
        const greenSurface = theme.surface.toString()
        const greenBorder = theme.border.toString()
        const greenInput = theme.inputBackground.toString()

        mouseClick(settingsNav)
        tryCompare(tabs, "currentIndex", 2)
        const blueButton = findChild(subject, "accentThemeButton_1")
        const largeButton = findChild(subject, "fontScaleButton_2")
        verify(blueButton !== null)
        verify(largeButton !== null)
        mouseClick(blueButton)
        mouseClick(largeButton)
        tryCompare(testAppViewModel.appearanceSettings, "accentThemeIndex", 1)
        tryCompare(testAppViewModel.appearanceSettings, "fontScaleIndex", 2)
        verify(theme.background.toString() !== greenBackground)
        verify(theme.navigation.toString() !== greenNavigation)
        verify(theme.surface.toString() !== greenSurface)
        verify(theme.border.toString() !== greenBorder)
        verify(theme.inputBackground.toString() !== greenInput)
        testAppViewModel.appearanceSettings.resetDefaults()
        tabs.currentIndex = 0
    }

    function test_cardClickOpensReadOnlyDetails() {
        tryVerify(function() { return taskDelegate(alphaId) !== null })
        mouseClick(taskDelegate(alphaId), 80, 30)
        const dialog = findChild(subject, "taskDetailsDialog")
        verify(dialog !== null)
        tryCompare(dialog, "opened", true)
        compare(testAppViewModel.taskList.selectedTaskId, alphaId)
        dialog.close()
        tryCompare(testAppViewModel.taskList, "selectedTaskId", "")
    }

    function test_focusSlotAcceptsRealPointerDrag() {
        const slot = findChild(subject, "focusTaskSlot")
        const dropArea = findChild(subject, "focusTaskDropArea")
        const preview = findChild(subject, "taskDragPreview")
        verify(slot !== null)
        verify(dropArea !== null)
        verify(preview !== null)
        compare(testAppViewModel.taskList.focusState, 1)
        const focusId = testAppViewModel.taskList.focusTaskId
        const handle = findChild(taskDelegate(focusId), "dragHandle_" + focusId)
        verify(handle !== null)
        tryCompare(handle, "visible", true)

        const target = handle.mapFromItem(slot, slot.width / 2, slot.height / 2)
        mousePress(handle, handle.width / 2, handle.height / 2, Qt.LeftButton)
        mouseMove(handle, handle.width / 2 + 12, handle.height / 2, 20)
        tryCompare(preview, "visible", true)
        mouseMove(handle, target.x, target.y, 80)
        tryCompare(dropArea, "containsDrag", true)
        mouseRelease(handle, target.x, target.y, Qt.LeftButton)

        tryCompare(testAppViewModel.taskList, "focusState", 2)
        compare(testAppViewModel.taskList.focusTaskId, focusId)
        verify(testAppViewModel.taskList.completeTask(focusId))
        verify(testAppViewModel.taskList.redoTask(focusId))
        tryCompare(testAppViewModel.taskList, "focusState", 1)
    }

    function test_blockedTaskDoesNotExposeDragHandle() {
        saveSinglePredecessor(betaId, alphaId)
        tryVerify(function() { return taskDelegate(betaId) !== null })
        const handle = findChild(taskDelegate(betaId), "dragHandle_" + betaId)
        verify(handle !== null)
        tryCompare(handle, "visible", false)
    }

    function test_editorFitsMinimumWindowWithLargeFontAndLongText() {
        const oldWidth = subject.width
        const oldHeight = subject.height
        subject.width = 900
        subject.height = 620
        testAppViewModel.appearanceSettings.fontScaleIndex = 2

        const newTaskButton = findChild(subject, "newTaskButton")
        mouseClick(newTaskButton)
        const dialog = findChild(subject, "taskEditorDialog")
        const scroll = findChild(subject, "taskEditorScrollView")
        const content = findChild(subject, "taskEditorContent")
        const titleField = findChild(subject, "taskTitleField")
        const descriptionArea = findChild(subject, "taskDescriptionArea")
        verify(dialog !== null)
        verify(scroll !== null)
        verify(content !== null)
        verify(titleField !== null)
        verify(descriptionArea !== null)
        tryCompare(dialog, "opened", true)

        testAppViewModel.taskEditor.title = "这是一个用于验证小窗口布局的很长任务标题，所有控件都不应越过弹窗边界"
        testAppViewModel.taskEditor.description = "长描述用于验证文字自动换行。".repeat(20)
        wait(50)
        verify(dialog.width <= subject.width - 40)
        verify(dialog.height <= subject.height - 40)
        verify(content.width <= scroll.availableWidth + 1)
        verify(titleField.width <= scroll.availableWidth + 1)
        verify(descriptionArea.width <= scroll.availableWidth + 1)

        dialog.openDeadlinePicker()
        const deadline = findChild(subject, "deadlinePickerDialog")
        tryCompare(deadline, "opened", true)
        verify(deadline.width <= subject.width - 24)
        verify(deadline.height <= subject.height - 24)
        deadline.close()

        dialog.openDurationPicker()
        const duration = findChild(subject, "durationPickerDialog")
        tryCompare(duration, "opened", true)
        verify(duration.width <= subject.width - 24)
        duration.close()

        const predecessorButton = findChild(subject, "openCreationPredecessorButton")
        predecessorButton.clicked()
        const predecessor = findChild(subject, "taskCreationPredecessorDialog")
        tryCompare(predecessor, "opened", true)
        verify(predecessor.width <= subject.width - 24)
        verify(predecessor.height <= subject.height - 24)
        predecessor.close()

        testAppViewModel.taskEditor.cancel()
        dialog.close()
        subject.width = oldWidth
        subject.height = oldHeight
        testAppViewModel.appearanceSettings.resetDefaults()
    }

    function test_archivedTaskHidesEditEntryUntilRestored() {
        tryVerify(function() { return taskDelegate(alphaId) !== null })
        let editButton = findChild(taskDelegate(alphaId),
                                   "editTaskButton_" + alphaId)
        verify(editButton !== null)
        tryCompare(editButton, "enabled", true)

        // Todo 不能直接归档；先严格经过 Todo → InProgress → Done。
        verify(testAppViewModel.taskList.startTask(alphaId),
               testAppViewModel.taskList.errorMessage)
        verify(testAppViewModel.taskList.completeTask(alphaId),
               testAppViewModel.taskList.errorMessage)
        verify(testAppViewModel.taskList.archiveTask(alphaId),
               testAppViewModel.taskList.errorMessage)
        testAppViewModel.taskList.showArchived = true
        tryCompare(testAppViewModel.taskList, "count", 1)
        tryVerify(function() { return taskDelegate(alphaId) !== null })
        editButton = findChild(taskDelegate(alphaId),
                               "editTaskButton_" + alphaId)
        const restoreButton = findChild(taskDelegate(alphaId),
                                        "primaryTaskAction_" + alphaId)
        verify(editButton !== null)
        verify(restoreButton !== null)
        tryCompare(editButton, "enabled", false)
        tryCompare(restoreButton, "visible", true)
        compare(restoreButton.text, "恢复")

        verify(testAppViewModel.taskList.restoreTask(alphaId),
               testAppViewModel.taskList.errorMessage)
        testAppViewModel.taskList.showArchived = false
        tryCompare(testAppViewModel.taskList, "count", 3)
        tryVerify(function() { return taskDelegate(alphaId) !== null })
        editButton = findChild(taskDelegate(alphaId),
                               "editTaskButton_" + alphaId)
        verify(editButton !== null)
        tryCompare(editButton, "enabled", true)

        // 恢复后的正常状态是 Done；重做回 Todo，避免污染后续依赖编辑用例。
        verify(testAppViewModel.taskList.redoTask(alphaId),
               testAppViewModel.taskList.errorMessage)
    }

    function test_editorShowsReadOnlyInitialStatusInsteadOfStatusSelector() {
        const newTaskButton = findChild(subject, "newTaskButton")
        verify(newTaskButton !== null)
        mouseClick(newTaskButton)

        const editorDialog = findChild(subject, "taskEditorDialog")
        verify(editorDialog !== null)
        tryCompare(editorDialog, "opened", true)
        const statusLabel = findChild(editorDialog, "taskCurrentStatusLabel")
        verify(statusLabel !== null)
        verify(statusLabel.text.indexOf("初始状态：待办") >= 0)
        compare(findChild(editorDialog, "taskStatusComboBox"), null)

        testAppViewModel.taskEditor.cancel()
        editorDialog.close()
    }

    function test_stateButtonsFollowProjectionAndCancellationRequiresConfirmation() {
        tryVerify(function() { return taskDelegate(gammaId) !== null })
        let delegate = taskDelegate(gammaId)
        let startButton = findChild(delegate, "primaryTaskAction_" + gammaId)
        let cancelButton = findChild(delegate, "cancelTaskButton_" + gammaId)
        let archiveButton = findChild(delegate, "archiveTaskButton_" + gammaId)
        verify(startButton !== null)
        verify(cancelButton !== null)
        verify(archiveButton !== null)
        compare(delegate.canStart, true)
        compare(startButton.text, "开始")
        tryCompare(cancelButton, "enabled", true)
        tryCompare(archiveButton, "enabled", false)

        startButton.clicked()
        tryVerify(function() {
            return taskDelegate(gammaId) !== null
                   && taskDelegate(gammaId).statusText === "进行中"
        })
        delegate = taskDelegate(gammaId)
        let completeButton = findChild(delegate, "primaryTaskAction_" + gammaId)
        cancelButton = findChild(delegate, "cancelTaskButton_" + gammaId)
        tryCompare(completeButton, "visible", true)
        compare(completeButton.text, "完成")
        tryCompare(cancelButton, "enabled", true)

        completeButton.clicked()
        tryVerify(function() {
            return taskDelegate(gammaId) !== null
                   && taskDelegate(gammaId).statusText === "已完成"
        })
        delegate = taskDelegate(gammaId)
        let redoButton = findChild(delegate, "primaryTaskAction_" + gammaId)
        archiveButton = findChild(delegate, "archiveTaskButton_" + gammaId)
        tryCompare(redoButton, "visible", true)
        compare(redoButton.text, "重做")
        tryCompare(archiveButton, "enabled", true)
        redoButton.clicked()
        tryVerify(function() {
            return taskDelegate(gammaId) !== null
                   && taskDelegate(gammaId).statusText === "待办"
        })

        delegate = taskDelegate(gammaId)
        cancelButton = findChild(delegate, "cancelTaskButton_" + gammaId)
        cancelButton.triggered()
        const cancelDialog = findChild(subject, "cancelTaskDialog")
        verify(cancelDialog !== null)
        tryCompare(cancelDialog, "opened", true)
        cancelDialog.reject()
        tryCompare(cancelDialog, "opened", false)
        compare(taskDelegate(gammaId).statusText, "待办")

        cancelButton = findChild(taskDelegate(gammaId),
                                 "cancelTaskButton_" + gammaId)
        cancelButton.triggered()
        tryCompare(cancelDialog, "opened", true)
        cancelDialog.accept()
        tryVerify(function() {
            return taskDelegate(gammaId) !== null
                   && taskDelegate(gammaId).statusText === "已取消"
        })
        delegate = taskDelegate(gammaId)
        redoButton = findChild(delegate, "primaryTaskAction_" + gammaId)
        archiveButton = findChild(delegate, "archiveTaskButton_" + gammaId)
        verify(redoButton !== null)
        compare(redoButton.text, "重做")
        tryCompare(archiveButton, "enabled", true)

        // 返回 Todo，保证共享内存数据库中的后续用例仍可编辑依赖。
        redoButton.clicked()
        tryVerify(function() {
            return taskDelegate(gammaId) !== null
                   && taskDelegate(gammaId).statusText === "待办"
        })
    }

    function test_dependencyDialogShowsBlockerAndUnlockProjection() {
        const listView = findChild(subject, "taskListView")
        verify(listView !== null)
        compare(listView.count, 3)
        verify(listView.height > 0, "taskListView height=" + listView.height)
        listView.positionViewAtBeginning()
        wait(0)
        tryVerify(function() { return taskDelegate(alphaId) !== null })
        const alphaDelegate = taskDelegate(alphaId)
        const editButton = findChild(alphaDelegate,
                                     "editDependenciesButton_" + alphaId)
        verify(editButton !== null)
        editButton.triggered()
        wait(0)

        const dialog = findChild(subject, "taskDependencyDialog")
        verify(dialog !== null)
        tryVerify(function() {
            return dependencyCandidateDelegate(dialog, betaId) !== null
        })
        const betaCheckBox = findChild(
                    dependencyCandidateDelegate(dialog, betaId),
                    "dependencyCandidateCheckBox_" + betaId)
        const saveButton = findChild(dialog, "saveDependenciesButton")
        verify(betaCheckBox !== null)
        verify(saveButton !== null)
        mouseClick(betaCheckBox)
        tryCompare(testAppViewModel.taskDependencies, "selectedCount", 1)
        mouseClick(saveButton)

        tryVerify(function() {
            return taskDelegate(alphaId) !== null && taskDelegate(betaId) !== null
        })
        const blockingLabel = findChild(taskDelegate(alphaId),
                                        "blockingReasonLabel_" + alphaId)
        const unlockLabel = findChild(taskDelegate(betaId),
                                      "unlockCountLabel_" + betaId)
        verify(blockingLabel !== null)
        verify(unlockLabel !== null)
        tryCompare(blockingLabel, "visible", true)
        verify(blockingLabel.text.indexOf("Beta 文档") >= 0)
        tryCompare(unlockLabel, "visible", true)
        verify(unlockLabel.text.indexOf("1") >= 0)
    }

    function test_dependencyDialogShowsCompleteCyclePathAndCancelsDraft() {
        // Beta → Alpha、Gamma → Beta 已存在时，再添加 Alpha → Gamma 会形成完整三节点环。
        saveSinglePredecessor(alphaId, betaId)
        saveSinglePredecessor(betaId, gammaId)

        tryVerify(function() { return taskDelegate(gammaId) !== null })
        const editButton = findChild(taskDelegate(gammaId),
                                     "editDependenciesButton_" + gammaId)
        verify(editButton !== null)
        editButton.triggered()
        wait(0)

        const dialog = findChild(subject, "taskDependencyDialog")
        verify(dialog !== null)
        tryVerify(function() {
            return dependencyCandidateDelegate(dialog, alphaId) !== null
        })
        const alphaCheckBox = findChild(
                    dependencyCandidateDelegate(dialog, alphaId),
                    "dependencyCandidateCheckBox_" + alphaId)
        const saveButton = findChild(dialog, "saveDependenciesButton")
        const cancelButton = findChild(dialog, "cancelDependenciesButton")
        const errorLabel = findChild(dialog, "dependencyErrorLabel")
        verify(alphaCheckBox !== null)
        verify(saveButton !== null)
        verify(cancelButton !== null)
        verify(errorLabel !== null)

        mouseClick(alphaCheckBox)
        mouseClick(saveButton)
        tryVerify(function() { return errorLabel.text.length > 0 })
        verify(errorLabel.text.indexOf("Alpha 架构设计") >= 0)
        verify(errorLabel.text.indexOf("Beta 文档") >= 0)
        verify(errorLabel.text.indexOf("Gamma 测试") >= 0)
        verify(errorLabel.text.indexOf("→") >= 0)

        mouseClick(cancelButton)
        verify(!testAppViewModel.taskDependencies.dirty)
    }
}
