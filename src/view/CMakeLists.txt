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

set_source_files_properties(qml/TaskDependencyDialog.qml
    PROPERTIES
        QT_RESOURCE_ALIAS TaskDependencyDialog.qml
)

set_source_files_properties(qml/DependencyGraphPage.qml
    PROPERTIES
        QT_RESOURCE_ALIAS DependencyGraphPage.qml
)

set_source_files_properties(qml/TaskCreationPredecessorDialog.qml
    PROPERTIES
        QT_RESOURCE_ALIAS TaskCreationPredecessorDialog.qml
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
        qml/TaskDependencyDialog.qml
        qml/DependencyGraphPage.qml
        qml/TaskCreationPredecessorDialog.qml
    IMPORTS
        TARGET smartmate_viewmodel
    DEPENDENCIES
        QtQuick
        QtQuick.Controls
        QtQuick.Shapes
)

target_link_libraries(smartmate_ui
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
)
