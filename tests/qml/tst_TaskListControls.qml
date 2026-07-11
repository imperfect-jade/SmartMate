import QtQuick
import QtQuick.Controls
import QtTest
import SmartMate.View

TestCase {
    id: testCase

    name: "TaskListControls"
    when: windowShown
    property var subject: null

    Component {
        id: taskPageComponent

        ApplicationWindow {
            width: 1000
            height: 760
            visible: true

            TaskPage {
                anchors.fill: parent
                appViewModel: testAppViewModel
            }
        }
    }

    function createTask(title, description, priorityIndex) {
        const editor = testAppViewModel.taskEditor
        editor.beginCreate()
        editor.title = title
        editor.description = description
        editor.priorityIndex = priorityIndex
        verify(editor.save(), editor.errorMessage)
    }

    function initTestCase() {
        verify(testAppViewModel !== null)
        createTask("Alpha 架构设计", "Model planner 说明", 3)
        createTask("Beta 文档", "整理使用手册", 0)
        createTask("Gamma 测试", "覆盖筛选条件", 2)

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
        testAppViewModel.taskList.showArchived = false
        testAppViewModel.taskList.clearFilters()
        compare(testAppViewModel.taskList.count, 3)
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
}
