add_library(smartmate_widgets STATIC
    MainWindowDependencies.h
    MainWindow.h
    MainWindow.cpp
    statistics/StatisticsPage.h
    statistics/StatisticsPage.cpp
    binding/WidgetBinding.h
    settings/SettingsPage.h
    settings/SettingsPage.cpp
    pet/DesktopPetGeometry.h
    pet/DesktopPetGeometry.cpp
    pet/DesktopPetSpriteWidget.h
    pet/DesktopPetSpriteWidget.cpp
    pet/DesktopPetTaskPopup.h
    pet/DesktopPetTaskPopup.cpp
    pet/AttachedDesktopPetWindow.h
    pet/AttachedDesktopPetWindow.cpp
    pet/FloatingDesktopPetWindow.h
    pet/FloatingDesktopPetWindow.cpp
    task/TaskPage.h
    task/TaskPage.cpp
    task/TaskDragMime.h
    task/TaskListView.h
    task/TaskListView.cpp
    task/TaskFocusPanel.h
    task/TaskFocusPanel.cpp
    task/TaskItemDelegate.h
    task/TaskItemDelegate.cpp
    task/TaskEditorDialog.h
    task/TaskEditorDialog.cpp
    task/TaskDetailsDialog.h
    task/TaskDetailsDialog.cpp
    task/TaskCategoryDialog.h
    task/TaskCategoryDialog.cpp
    task/TaskCreationPredecessorDialog.h
    task/TaskCreationPredecessorDialog.cpp
    task/TaskDependencyDialog.h
    task/TaskDependencyDialog.cpp
    task/DeadlinePickerDialog.h
    task/DeadlinePickerDialog.cpp
    task/DurationPickerDialog.h
    task/DurationPickerDialog.cpp
    graph/DependencyGraphPage.h
    graph/DependencyGraphPage.cpp
    graph/DependencyGraphToolbar.h
    graph/DependencyGraphToolbar.cpp
    graph/TaskGraphDetailsPanel.h
    graph/TaskGraphDetailsPanel.cpp
    graph/DependencyGraphView.h
    graph/DependencyGraphView.cpp
    graph/TaskGraphItems.h
    graph/TaskGraphItems.cpp
    theme/WidgetTheme.h
    theme/WidgetTheme.cpp
)

qt_add_resources(smartmate_widgets desktop_pet_assets
    PREFIX "/smartmate/pets"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/pet/assets"
    FILES
        pet/assets/calico-cat-spritesheet.png
)

target_include_directories(smartmate_widgets
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(smartmate_widgets
    PUBLIC
        smartmate_common
        smartmate_viewmodel_contracts
        Qt6::Widgets
    PRIVATE
        Qt6::Charts
)
