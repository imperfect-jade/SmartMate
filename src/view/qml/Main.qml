import QtQuick
import QtQuick.Controls
import SmartMate.ViewModel 1.0

// 根 View 只组合页面。AppViewModel 由 C++ 组合根注入，QML 不创建也不拥有它。
ApplicationWindow {
    id: root

    required property AppViewModel appViewModel

    width: 960
    height: 680
    minimumWidth: 760
    minimumHeight: 540
    visible: true
    title: appViewModel.applicationName
    color: "#f5f7fb"

    TaskPage {
        anchors.fill: parent
        appViewModel: root.appViewModel
    }
}
