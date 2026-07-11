import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SmartMate.ViewModel 1.0

// 根 View 只组合页面。AppViewModel 由 C++ 组合根注入，QML 不创建也不拥有它。
ApplicationWindow {
    id: root

    required property AppViewModel appViewModel

    width: 1180
    height: 760
    minimumWidth: 900
    minimumHeight: 620
    visible: true
    title: appViewModel.applicationName
    color: "#f5f7fb"

    function openDependencyEditor(taskId) {
        if (root.appViewModel.taskDependencies.beginEdit(taskId)) {
            dependencyDialog.open()
        } else {
            dependencyErrorDialog.message = root.appViewModel.taskDependencies.errorMessage
            dependencyErrorDialog.open()
        }
    }

    header: ToolBar {
        implicitHeight: 72
        background: Rectangle {
            color: "#ffffff"
            border.color: "#e4e7ec"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 28
            anchors.rightMargin: 28
            spacing: 28

            Label {
                text: root.appViewModel.applicationName
                color: "#172033"
                font.pixelSize: 26
                font.bold: true
            }

            TabBar {
                id: navigationTabs
                objectName: "mainNavigationTabs"
                Layout.fillWidth: true
                Layout.maximumWidth: 360

                TabButton { text: qsTr("任务列表") }
                TabButton { text: qsTr("依赖图") }
            }

            Item { Layout.fillWidth: true }
        }
    }

    // 两个页面只共享根ViewModel和依赖编辑入口，各自保留列表条件、画布缩放与选中状态。
    StackLayout {
        anchors.fill: parent
        currentIndex: navigationTabs.currentIndex

        TaskPage {
            appViewModel: root.appViewModel
            onEditDependenciesRequested: function(taskId) {
                root.openDependencyEditor(taskId)
            }
        }

        DependencyGraphPage {
            appViewModel: root.appViewModel
            onEditDependenciesRequested: function(taskId) {
                root.openDependencyEditor(taskId)
            }
        }
    }

    // 两个页面复用同一依赖草稿，避免创建两个编辑器实例或让子ViewModel相互调用。
    TaskDependencyDialog {
        id: dependencyDialog
        objectName: "taskDependencyDialog"
        anchors.centerIn: Overlay.overlay
        editor: root.appViewModel.taskDependencies
    }

    Dialog {
        id: dependencyErrorDialog

        property string message

        anchors.centerIn: Overlay.overlay
        title: qsTr("无法编辑依赖")
        modal: true
        standardButtons: Dialog.Ok

        Label {
            width: 360
            wrapMode: Text.Wrap
            text: dependencyErrorDialog.message
        }

        onClosed: root.appViewModel.taskDependencies.clearError()
    }
}
