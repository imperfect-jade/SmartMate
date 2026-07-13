pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SmartMate.ViewModel 1.0

Rectangle {
    id: bar
    required property TaskListViewModel taskList
    required property var theme
    signal newTaskRequested()
    signal manageCategoriesRequested()
    signal bulkArchiveRequested()
    signal bulkRestoreRequested()
    signal bulkDeleteRequested()

    implicitHeight: toolbarLayout.implicitHeight + bar.theme.px(20)
    radius: 11
    color: bar.theme.surface
    border.color: bar.theme.borderSoft

    // 筛选控件始终可用；筛选变化由ViewModel清空选择并保留批量模式。
    ColumnLayout {
        id: toolbarLayout
        anchors.fill: parent
        anchors.margins: bar.theme.px(10)
        spacing: bar.theme.px(8)

        RowLayout {
            Layout.fillWidth: true
            spacing: bar.theme.px(8)
            Button {
                text: qsTr("活动任务")
                checkable: true
                checked: !bar.taskList.showArchived
                onClicked: bar.taskList.showArchived = false
            }
            Button {
                text: qsTr("归档")
                checkable: true
                checked: bar.taskList.showArchived
                onClicked: bar.taskList.showArchived = true
            }
            TextField {
                objectName: "taskSearchField"
                Layout.fillWidth: true
                Layout.minimumWidth: 180
                Layout.maximumWidth: 420
                text: bar.taskList.searchText
                placeholderText: qsTr("搜索任务标题或描述")
                selectByMouse: true
                onTextEdited: bar.taskList.searchText = text
            }
            Item { Layout.fillWidth: true }
            Label {
                text: qsTr("%1 项").arg(bar.taskList.count)
                color: bar.theme.textMuted
            }
            Button {
                objectName: "bulkManagementButton"
                visible: !bar.taskList.bulkSelectionMode
                text: qsTr("批量管理")
                enabled: bar.taskList.bulkSelectableVisibleCount > 0
                onClicked: bar.taskList.beginBulkSelection()
            }
            Button {
                objectName: "newTaskButton"
                visible: !bar.taskList.bulkSelectionMode
                text: qsTr("＋ 新建任务")
                onClicked: bar.newTaskRequested()
            }
        }

        // 类别选项由ViewModel提供稳定ID；下拉行号只用于取得当前选项，不作为类别身份。
        RowLayout {
            Layout.fillWidth: true
            spacing: bar.theme.px(8)
            Label { text: qsTr("筛选"); color: bar.theme.textMuted }
            ComboBox {
                objectName: "priorityFilterComboBox"
                Layout.preferredWidth: bar.theme.px(145)
                model: bar.taskList.priorityFilterOptions
                currentIndex: bar.taskList.priorityFilterIndex
                onActivated: index => bar.taskList.priorityFilterIndex = index
            }
            ComboBox {
                id: categoryFilter
                objectName: "categoryFilterComboBox"
                Layout.preferredWidth: bar.theme.px(180)
                model: bar.taskList.categoryFilterOptions
                textRole: "name"
                valueRole: "categoryId"
                currentIndex: bar.taskList.categoryFilterMode === 0 ? 0
                              : bar.taskList.categoryFilterMode === 1 ? 1
                              : categoryFilter.indexOfValue(
                                    bar.taskList.categoryFilterCategoryId)
                onActivated: index => {
                    const option = bar.taskList.categoryFilterOptions[index]
                    bar.taskList.setCategoryFilter(option.mode, option.categoryId)
                }
            }
            Button {
                objectName: "manageCategoriesButton"
                text: qsTr("管理类别")
                onClicked: bar.manageCategoriesRequested()
            }
            Button {
                objectName: "clearFiltersButton"
                text: qsTr("清除条件")
                visible: bar.taskList.hasActiveFilters
                onClicked: bar.taskList.clearFilters()
            }
            Item { Layout.fillWidth: true }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: bar.taskList.bulkSelectionMode
            spacing: bar.theme.px(8)
            Label {
                objectName: "bulkSelectedCountLabel"
                text: qsTr("已选择 %1 项").arg(bar.taskList.bulkSelectedCount)
                color: bar.theme.textPrimary
                font.bold: true
            }
            Button {
                objectName: "selectAllVisibleButton"
                text: bar.taskList.allVisibleSelected
                      ? qsTr("取消全选") : qsTr("全选当前结果")
                enabled: bar.taskList.bulkSelectableVisibleCount > 0
                onClicked: bar.taskList.toggleSelectAllVisible()
            }
            Button {
                objectName: "clearBulkSelectionButton"
                text: qsTr("清空")
                enabled: bar.taskList.bulkSelectedCount > 0
                onClicked: bar.taskList.clearBulkSelection()
            }
            Item { Layout.fillWidth: true }
            Button {
                objectName: "bulkArchiveButton"
                visible: !bar.taskList.showArchived
                enabled: bar.taskList.canBulkArchive
                text: qsTr("归档所选")
                onClicked: bar.bulkArchiveRequested()
            }
            Button {
                objectName: "bulkRestoreButton"
                visible: bar.taskList.showArchived
                enabled: bar.taskList.canBulkRestore
                text: qsTr("恢复所选")
                onClicked: bar.bulkRestoreRequested()
            }
            Button {
                objectName: "bulkDeleteButton"
                visible: bar.taskList.showArchived
                enabled: bar.taskList.canBulkDelete
                text: qsTr("永久删除所选")
                onClicked: bar.bulkDeleteRequested()
            }
            Button {
                objectName: "cancelBulkSelectionButton"
                text: qsTr("退出")
                onClicked: bar.taskList.cancelBulkSelection()
            }
        }
    }
}
