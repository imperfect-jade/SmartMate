pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 日期时间候选值完全属于 View；取消弹窗不会向 ViewModel 发送任何命令。
Dialog {
    id: root

    property int pendingYear: 2000
    property int pendingMonth: 1
    property int pendingDay: 1
    property int pendingHour: 0
    property int pendingMinute: 0

    // 日历浏览月份与已选日期分离，跨月查看不会意外修改候选日期。
    property int visibleYear: pendingYear
    property int visibleMonth: pendingMonth

    signal selectionAccepted(int year, int month, int day, int hour, int minute)

    function openWithValues(year, month, day, hour, minute) {
        root.pendingYear = year
        root.pendingMonth = month
        root.pendingDay = day
        root.pendingHour = hour
        root.pendingMinute = minute
        root.visibleYear = year
        root.visibleMonth = month
        root.open()
    }

    function moveVisibleMonth(offset) {
        const zeroBasedMonth = root.visibleYear * 12 + root.visibleMonth - 1 + offset
        root.visibleYear = Math.floor(zeroBasedMonth / 12)
        root.visibleMonth = zeroBasedMonth - root.visibleYear * 12 + 1
    }

    function chooseToday() {
        const today = new Date()
        root.pendingYear = today.getFullYear()
        root.pendingMonth = today.getMonth() + 1
        root.pendingDay = today.getDate()
        root.visibleYear = root.pendingYear
        root.visibleMonth = root.pendingMonth
    }

    width: 500
    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose
    title: qsTr("选择截止时间")

    onAccepted: root.selectionAccepted(root.pendingYear,
                                       root.pendingMonth,
                                       root.pendingDay,
                                       root.pendingHour,
                                       root.pendingMinute)

    contentItem: ColumnLayout {
        spacing: 10

        RowLayout {
            Layout.fillWidth: true

            Button {
                text: qsTr("‹")
                Accessible.name: qsTr("上一个月")
                onClicked: root.moveVisibleMonth(-1)
            }

            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("%1年%2月").arg(root.visibleYear).arg(root.visibleMonth)
                font.bold: true
                font.pixelSize: 17
            }

            Button {
                text: qsTr("今天")
                onClicked: root.chooseToday()
            }

            Button {
                text: qsTr("›")
                Accessible.name: qsTr("下一个月")
                onClicked: root.moveVisibleMonth(1)
            }
        }

        DayOfWeekRow {
            Layout.fillWidth: true
            locale: Qt.locale()
        }

        MonthGrid {
            id: calendarGrid
            objectName: "calendarMonthGrid"

            Layout.alignment: Qt.AlignHCenter
            month: root.visibleMonth - 1
            year: root.visibleYear
            locale: Qt.locale()

            delegate: Rectangle {
                id: dayCell

                required property var model

                implicitWidth: 56
                implicitHeight: 38
                radius: 5
                opacity: dayCell.model.month === calendarGrid.month ? 1 : 0
                color: root.pendingYear === calendarGrid.year
                       && root.pendingMonth === calendarGrid.month + 1
                       && root.pendingDay === dayCell.model.day
                       ? "#dbeafe" : "transparent"

                Label {
                    anchors.centerIn: parent
                    text: dayCell.model.day
                    color: "#172033"
                }
            }

            onClicked: date => {
                // MonthGrid 还会产生相邻月份的网格日期，隐藏单元格不参与选择。
                if (date.getFullYear() !== root.visibleYear
                        || date.getMonth() + 1 !== root.visibleMonth)
                    return
                root.pendingYear = date.getFullYear()
                root.pendingMonth = date.getMonth() + 1
                root.pendingDay = date.getDate()
            }
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("时间（24小时制）")
            font.bold: true
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 12

            Tumbler {
                id: hourTumbler
                objectName: "deadlineHourTumbler"

                Layout.preferredWidth: 90
                Layout.preferredHeight: 140
                model: 24
                visibleItemCount: 5
                wrap: true
                currentIndex: root.pendingHour
                onCurrentIndexChanged: {
                    if (currentIndex >= 0 && root.pendingHour !== currentIndex)
                        root.pendingHour = currentIndex
                }

                delegate: Label {
                    required property int index
                    required property int modelData

                    text: modelData < 10 ? "0" + modelData : modelData
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    opacity: 1.0 - Math.abs(Tumbler.displacement)
                                         / (hourTumbler.visibleItemCount / 2)
                }
            }

            Label {
                text: qsTr("时")
                font.bold: true
            }

            Tumbler {
                id: minuteTumbler
                objectName: "deadlineMinuteTumbler"

                Layout.preferredWidth: 90
                Layout.preferredHeight: 140
                model: 60
                visibleItemCount: 5
                wrap: true
                currentIndex: root.pendingMinute
                onCurrentIndexChanged: {
                    if (currentIndex >= 0 && root.pendingMinute !== currentIndex)
                        root.pendingMinute = currentIndex
                }

                delegate: Label {
                    required property int index
                    required property int modelData

                    text: modelData < 10 ? "0" + modelData : modelData
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    opacity: 1.0 - Math.abs(Tumbler.displacement)
                                         / (minuteTumbler.visibleItemCount / 2)
                }
            }

            Label {
                text: qsTr("分")
                font.bold: true
            }
        }
    }

    footer: Rectangle {
        implicitHeight: 56
        color: "#ffffff"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("取消")
                onClicked: root.reject()
            }

            Button {
                objectName: "confirmDeadlineSelectionButton"
                text: qsTr("确定")
                onClicked: root.accept()
            }
        }
    }
}
