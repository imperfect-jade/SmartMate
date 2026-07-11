import QtQuick
import SmartMate.ViewModel 1.0

// View 层集中解释外观偏好；业务状态色不随强调色切换。
QtObject {
    objectName: "appearanceTheme"
    required property AppearanceSettingsViewModel settings

    readonly property bool blueAccent: settings.accentThemeIndex === 1
    readonly property real scale: settings.fontScale
    readonly property string fontFamily: settings.fontFamilyName

    // 青绿主题取自欧碧、春辰、碧山、京绿、秧色、苔荷色阶，正文使用更深色保证可读性。
    readonly property color primary: blueAccent ? "#2563eb" : "#507936"
    readonly property color primaryHover: blueAccent ? "#1d4ed8" : "#3f6330"
    readonly property color primarySoft: blueAccent ? "#ddeaff" : "#e7f0d6"
    readonly property color focusRing: blueAccent ? "#78a9ed" : "#719847"
    readonly property color background: blueAccent ? "#eaf2fc" : "#eef5e5"
    readonly property color navigation: blueAccent ? "#dce9fa" : "#dde9c5"
    readonly property color surface: blueAccent ? "#f8fbff" : "#fafdf5"
    readonly property color surfaceElevated: "#ffffff"
    readonly property color surfaceSubtle: blueAccent ? "#ecf3fc" : "#f0f6e7"
    readonly property color surfaceStrong: blueAccent ? "#e0ecfa" : "#e4eed3"
    readonly property color inputBackground: blueAccent ? "#fbfdff" : "#fcfff8"
    readonly property color controlHover: blueAccent ? "#e4effc" : "#eaf2dd"
    readonly property color controlPressed: blueAccent ? "#d5e5f8" : "#dce8c9"
    readonly property color border: blueAccent ? "#abc4e4" : "#a9be7b"
    readonly property color borderSoft: blueAccent ? "#ceddf0" : "#cdddb0"
    readonly property color borderStrong: blueAccent ? "#769ac8" : "#719847"
    readonly property color textPrimary: blueAccent ? "#142c4a" : "#24351f"
    readonly property color textBody: blueAccent ? "#294765" : "#354b2e"
    readonly property color textSecondary: blueAccent ? "#496783" : "#506449"
    readonly property color textMuted: blueAccent ? "#61778e" : "#677761"
    readonly property color textDisabled: "#98a2b3"
    readonly property color todo: "#175cd3"
    readonly property color inProgress: "#387a4a"
    readonly property color done: "#067647"
    readonly property color cancelled: "#667085"
    readonly property color archived: "#475467"
    readonly property color warning: "#b54708"
    readonly property color danger: "#b42318"

    function px(value) { return Math.round(value * scale) }
    function statusColor(status) {
        if (status === 0) return todo
        if (status === 1) return inProgress
        if (status === 2) return done
        if (status === 3) return cancelled
        return archived
    }
    function priorityColor(priority) {
        if (priority >= 3) return danger
        if (priority === 2) return warning
        if (priority === 1) return todo
        return textSecondary
    }
}
