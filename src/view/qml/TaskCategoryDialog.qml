pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SmartMate.ViewModel 1.0

// 共享类别管理弹窗只维护ViewModel草稿并转发稳定CategoryId；校验和原子删除均由Model完成。
Dialog {
    id: root
    required property TaskCategoryViewModel categories
    required property AppearanceTheme theme

    property string pendingDeleteCategoryId
    property string pendingDeleteCategoryName
    property int pendingDeleteTaskCount: 0

    objectName: "taskCategoryDialog"
    width: Math.max(root.theme.px(620),
                    Math.min(root.theme.px(780),
                             parent ? parent.width - root.theme.px(48)
                                    : root.theme.px(780)))
    height: Math.max(root.theme.px(480),
                     Math.min(root.theme.px(610),
                              parent ? parent.height - root.theme.px(48)
                                     : root.theme.px(610)))
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    title: qsTr("管理任务类别")
    standardButtons: Dialog.Close

    onOpened: root.categories.reload()
    onClosed: root.categories.cancel()

    background: Rectangle {
        radius: 14
        color: root.theme.surfaceElevated
        border.color: root.theme.border
    }

    contentItem: RowLayout {
        spacing: root.theme.px(14)

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: root.theme.px(310)
            radius: 10
            color: root.theme.surface
            border.color: root.theme.borderSoft

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: root.theme.px(12)
                spacing: root.theme.px(10)

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("已有类别")
                        color: root.theme.textPrimary
                        font.pixelSize: root.theme.px(16)
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: qsTr("%1 个").arg(root.categories.count)
                        color: root.theme.textMuted
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: categoryList
                        objectName: "taskCategoryList"
                        anchors.fill: parent
                        clip: true
                        spacing: root.theme.px(7)
                        model: root.categories

                        delegate: Rectangle {
                            id: categoryRow
                            required property string categoryId
                            required property string name
                            required property string accent
                            required property int taskCount

                            width: ListView.view.width
                            height: root.theme.px(58)
                            radius: 8
                            color: root.theme.surfaceSubtle
                            border.color: root.theme.borderSoft

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: root.theme.px(9)
                                spacing: root.theme.px(8)

                                Rectangle {
                                    Layout.preferredWidth: root.theme.px(13)
                                    Layout.preferredHeight: root.theme.px(13)
                                    radius: width / 2
                                    color: categoryRow.accent
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    spacing: 1
                                    Label {
                                        Layout.fillWidth: true
                                        text: categoryRow.name
                                        color: root.theme.textPrimary
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        text: qsTr("关联 %1 项任务").arg(categoryRow.taskCount)
                                        color: root.theme.textMuted
                                        font.pixelSize: root.theme.px(11)
                                    }
                                }
                                ToolButton {
                                    objectName: "editCategory_" + categoryRow.categoryId
                                    text: qsTr("编辑")
                                    onClicked: root.categories.beginEdit(categoryRow.categoryId)
                                }
                                ToolButton {
                                    objectName: "deleteCategory_" + categoryRow.categoryId
                                    text: qsTr("删除")
                                    onClicked: {
                                        root.pendingDeleteCategoryId = categoryRow.categoryId
                                        root.pendingDeleteCategoryName = categoryRow.name
                                        root.pendingDeleteTaskCount = categoryRow.taskCount
                                        deleteCategoryDialog.open()
                                    }
                                }
                            }
                        }

                        ScrollBar.vertical: ScrollBar { }
                    }

                    ColumnLayout {
                        anchors.centerIn: parent
                        visible: root.categories.empty
                        spacing: root.theme.px(6)
                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("还没有类别")
                            color: root.theme.textSecondary
                            font.bold: true
                        }
                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("可以创建“学习”“工作”“旅游”等类别")
                            color: root.theme.textMuted
                            font.pixelSize: root.theme.px(12)
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: root.theme.px(285)
            Layout.fillHeight: true
            radius: 10
            color: root.theme.surface
            border.color: root.theme.borderSoft

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: root.theme.px(15)
                spacing: root.theme.px(11)

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: root.categories.editMode
                              ? qsTr("编辑类别") : qsTr("创建类别")
                        color: root.theme.textPrimary
                        font.pixelSize: root.theme.px(16)
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Button {
                        visible: root.categories.editMode
                        text: qsTr("新建")
                        onClicked: root.categories.beginCreate()
                    }
                }

                Label { text: qsTr("名称"); color: root.theme.textBody; font.bold: true }
                TextField {
                    id: categoryNameField
                    objectName: "categoryNameField"
                    Layout.fillWidth: true
                    text: root.categories.draftName
                    placeholderText: qsTr("例如：学习")
                    selectByMouse: true
                    onTextEdited: root.categories.draftName = text
                }

                Label { text: qsTr("颜色"); color: root.theme.textBody; font.bold: true }
                Flow {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.theme.px(108)
                    spacing: root.theme.px(8)

                    Repeater {
                        model: root.categories.colorOptions.length
                        delegate: ToolButton {
                            id: colorButton
                            required property int index
                            width: root.theme.px(52)
                            height: root.theme.px(48)
                            checkable: true
                            checked: root.categories.draftColorIndex === colorButton.index
                            Accessible.name: root.categories.colorOptions[colorButton.index]
                            onClicked: root.categories.draftColorIndex = colorButton.index
                            background: Rectangle {
                                radius: 8
                                color: colorButton.checked
                                       ? root.theme.surfaceStrong : "transparent"
                                border.width: colorButton.checked ? 2 : 1
                                border.color: colorButton.checked
                                              ? root.categories.colorAccents[colorButton.index]
                                              : root.theme.borderSoft
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: root.theme.px(22)
                                    height: width
                                    radius: width / 2
                                    color: root.categories.colorAccents[colorButton.index]
                                }
                            }
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.categories.errorMessage.length > 0
                    text: root.categories.errorMessage
                    color: root.theme.danger
                    wrapMode: Text.Wrap
                }

                Item { Layout.fillHeight: true }

                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        text: qsTr("重置")
                        enabled: root.categories.dirty
                        onClicked: root.categories.cancel()
                    }
                    Item { Layout.fillWidth: true }
                    Button {
                        objectName: "saveCategoryButton"
                        text: root.categories.editMode ? qsTr("保存修改") : qsTr("创建")
                        enabled: root.categories.canSave
                        onClicked: root.categories.save()
                    }
                }
            }
        }
    }

    Dialog {
        id: deleteCategoryDialog
        objectName: "deleteCategoryConfirmationDialog"
        anchors.centerIn: Overlay.overlay
        width: root.theme.px(460)
        modal: true
        title: qsTr("确认删除类别")
        standardButtons: Dialog.Ok | Dialog.Cancel

        Label {
            width: root.theme.px(410)
            wrapMode: Text.Wrap
            text: root.pendingDeleteTaskCount > 0
                  ? qsTr("删除“%1”后，关联的 %2 项任务将变为未分类。任务状态和全部依赖关系保持不变。")
                      .arg(root.pendingDeleteCategoryName)
                      .arg(root.pendingDeleteTaskCount)
                  : qsTr("确定删除空类别“%1”吗？")
                      .arg(root.pendingDeleteCategoryName)
        }

        onAccepted: root.categories.deleteCategory(root.pendingDeleteCategoryId)
    }
}
