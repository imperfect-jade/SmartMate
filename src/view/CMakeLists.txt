set_source_files_properties(qml/Main.qml
    PROPERTIES
        QT_RESOURCE_ALIAS Main.qml
)

set_source_files_properties(qml/TaskPage.qml
    PROPERTIES
        QT_RESOURCE_ALIAS TaskPage.qml
)

set_source_files_properties(qml/TaskCard.qml
    PROPERTIES QT_RESOURCE_ALIAS TaskCard.qml)
set_source_files_properties(qml/TaskFilterBar.qml
    PROPERTIES QT_RESOURCE_ALIAS TaskFilterBar.qml)
set_source_files_properties(qml/TaskFocusCard.qml
    PROPERTIES QT_RESOURCE_ALIAS TaskFocusCard.qml)
set_source_files_properties(qml/DependencyGraphEdge.qml
    PROPERTIES QT_RESOURCE_ALIAS DependencyGraphEdge.qml)
set_source_files_properties(qml/DependencyGraphNode.qml
    PROPERTIES QT_RESOURCE_ALIAS DependencyGraphNode.qml)

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

set_source_files_properties(qml/AppearanceTheme.qml
    PROPERTIES QT_RESOURCE_ALIAS AppearanceTheme.qml)
set_source_files_properties(qml/SettingsPage.qml
    PROPERTIES QT_RESOURCE_ALIAS SettingsPage.qml)
set_source_files_properties(qml/TaskDetailsDialog.qml
    PROPERTIES QT_RESOURCE_ALIAS TaskDetailsDialog.qml)
set_source_files_properties(qml/TaskCategoryDialog.qml
    PROPERTIES QT_RESOURCE_ALIAS TaskCategoryDialog.qml)

qt_add_qml_module(smartmate_ui
    STATIC
    URI SmartMate.View
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/TaskPage.qml
        qml/TaskCard.qml
        qml/TaskFilterBar.qml
        qml/TaskFocusCard.qml
        qml/DependencyGraphEdge.qml
        qml/DependencyGraphNode.qml
        qml/TaskEditorDialog.qml
        qml/DateTimePickerDialog.qml
        qml/DurationPickerDialog.qml
        qml/TaskDependencyDialog.qml
        qml/DependencyGraphPage.qml
        qml/TaskCreationPredecessorDialog.qml
        qml/AppearanceTheme.qml
        qml/SettingsPage.qml
        qml/TaskDetailsDialog.qml
        qml/TaskCategoryDialog.qml
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
