import QtQuick
import QtTest
import SmartMate.View

TestCase {
    id: testCase

    name: "PickerDialogs"
    when: windowShown

    property var subject: null

    AppearanceTheme {
        id: testTheme
        settings: testAppViewModel.appearanceSettings
    }

    SignalSpy {
        id: selectionSpy
        signalName: "selectionAccepted"
    }

    Component {
        id: dateTimePickerComponent

        DateTimePickerDialog { theme: testTheme }
    }

    Component {
        id: durationPickerComponent

        DurationPickerDialog { theme: testTheme }
    }

    function init() {
        subject = null
        selectionSpy.target = null
        selectionSpy.clear()
    }

    function cleanup() {
        selectionSpy.target = null
        if (subject !== null) {
            subject.destroy()
            subject = null
        }
    }

    function createDateTimePicker() {
        subject = dateTimePickerComponent.createObject(testCase)
        verify(subject !== null)
        selectionSpy.target = subject
    }

    function createDurationPicker() {
        subject = durationPickerComponent.createObject(testCase)
        verify(subject !== null)
        selectionSpy.target = subject
    }

    function test_dateTimeAcceptCommitsPendingSelectionOnce() {
        createDateTimePicker()
        subject.openWithValues(2028, 2, 29, 23, 59)

        compare(subject.pendingYear, 2028)
        compare(subject.pendingMonth, 2)
        compare(subject.pendingDay, 29)
        compare(subject.pendingHour, 23)
        compare(subject.pendingMinute, 59)

        subject.accept()

        compare(selectionSpy.count, 1)
        compare(selectionSpy.signalArguments[0][0], 2028)
        compare(selectionSpy.signalArguments[0][1], 2)
        compare(selectionSpy.signalArguments[0][2], 29)
        compare(selectionSpy.signalArguments[0][3], 23)
        compare(selectionSpy.signalArguments[0][4], 59)
    }

    function test_dateTimeRejectDoesNotCommitPendingSelection() {
        createDateTimePicker()
        subject.openWithValues(2030, 12, 31, 20, 15)

        subject.pendingDay = 30
        subject.pendingHour = 8
        subject.reject()

        compare(selectionSpy.count, 0)
    }

    function test_dateTimeUsesCalendarAndFixedClockControls() {
        createDateTimePicker()

        const calendar = findChild(subject, "calendarMonthGrid")
        const hourTumbler = findChild(subject, "deadlineHourTumbler")
        const minuteTumbler = findChild(subject, "deadlineMinuteTumbler")

        verify(calendar !== null)
        verify(hourTumbler !== null)
        verify(minuteTumbler !== null)
        compare(hourTumbler.model, 24)
        compare(minuteTumbler.model, 60)
    }

    function test_durationAcceptCommitsPendingSelectionOnce() {
        createDurationPicker()
        subject.openWithValues(2, 3, 4)

        compare(subject.pendingDays, 2)
        compare(subject.pendingHours, 3)
        compare(subject.pendingMinutes, 4)
        subject.accept()

        compare(selectionSpy.count, 1)
        compare(selectionSpy.signalArguments[0][0], 2)
        compare(selectionSpy.signalArguments[0][1], 3)
        compare(selectionSpy.signalArguments[0][2], 4)
    }

    function test_durationRejectDoesNotCommitPendingSelection() {
        createDurationPicker()
        subject.openWithValues(1, 2, 3)

        subject.pendingMinutes = 45
        subject.reject()

        compare(selectionSpy.count, 0)
    }

    function test_durationUsesNonEditableBoundedControls() {
        createDurationPicker()

        const daysSpinBox = findChild(subject, "durationDaysSpinBox")
        const hoursSpinBox = findChild(subject, "durationHoursSpinBox")
        const minutesSpinBox = findChild(subject, "durationMinutesSpinBox")

        verify(daysSpinBox !== null)
        verify(hoursSpinBox !== null)
        verify(minutesSpinBox !== null)
        compare(daysSpinBox.from, 0)
        compare(daysSpinBox.to, 365)
        compare(daysSpinBox.editable, false)
        compare(hoursSpinBox.editable, false)
        compare(minutesSpinBox.editable, false)
        compare(minutesSpinBox.from, 1)
    }

    function test_durationZeroSelectionClampsToMinimumMinute() {
        createDurationPicker()
        subject.openWithValues(0, 0, 0)

        const minutesSpinBox = findChild(subject, "durationMinutesSpinBox")
        compare(minutesSpinBox.from, 1)
        compare(subject.pendingMinutes, 1)

        subject.accept()
        compare(selectionSpy.count, 1)
        compare(selectionSpy.signalArguments[0][0], 0)
        compare(selectionSpy.signalArguments[0][1], 0)
        compare(selectionSpy.signalArguments[0][2], 1)
    }

    function test_durationMaximumDayRestrictsSmallerUnits() {
        createDurationPicker()
        subject.openWithValues(364, 23, 59)

        const hoursSpinBox = findChild(subject, "durationHoursSpinBox")
        const minutesSpinBox = findChild(subject, "durationMinutesSpinBox")
        compare(hoursSpinBox.to, 23)
        compare(minutesSpinBox.to, 59)

        subject.pendingDays = 365
        tryCompare(hoursSpinBox, "to", 0)
        tryCompare(minutesSpinBox, "to", 0)
        compare(subject.pendingHours, 0)
        compare(subject.pendingMinutes, 0)

        subject.accept()
        compare(selectionSpy.count, 1)
        compare(selectionSpy.signalArguments[0][0], 365)
        compare(selectionSpy.signalArguments[0][1], 0)
        compare(selectionSpy.signalArguments[0][2], 0)
    }
}
