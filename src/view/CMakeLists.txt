set_source_files_properties(qml/Main.qml
    PROPERTIES
        QT_RESOURCE_ALIAS Main.qml
)

set_source_files_properties(qml/TaskPage.qml
    PROPERTIES
        QT_RESOURCE_ALIAS TaskPage.qml
)

set_source_files_properties(qml/TaskEditorDialog.qml
    PROPERTIES
        QT_RESOURCE_ALIAS TaskEditorDialog.qml
)

set_source_files_properties(qml/DateTimePickerDialog.qml
    PROPERTIES
        QT_RESOURCE_ALIAS DateTimePickerDialog.qml
)

set_source_files_properties(qml/DurationPickerDialog.qml
    PROPERTIES
        QT_RESOURCE_ALIAS DurationPickerDialog.qml
)

qt_add_qml_module(smartmate_ui
    STATIC
    URI SmartMate.View
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/TaskPage.qml
        qml/TaskEditorDialog.qml
        qml/DateTimePickerDialog.qml
        qml/DurationPickerDialog.qml
    IMPORTS
        TARGET smartmate_viewmodel
    DEPENDENCIES
        QtQuick
        QtQuick.Controls
)

target_link_libraries(smartmate_ui
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
)
