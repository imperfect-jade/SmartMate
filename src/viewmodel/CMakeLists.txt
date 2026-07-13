qt_add_qml_module(smartmate_viewmodel
    STATIC
    URI SmartMate.ViewModel
    VERSION 1.0
    SOURCES
        AppViewModel.h
    AppViewModel.cpp
    AppearanceSettingsViewModel.h
    AppearanceSettingsViewModel.cpp
        TaskEditorViewModel.h
        TaskEditorViewModel.cpp
        TaskDependencyViewModel.h
        TaskDependencyViewModel.cpp
        TaskErrorMapper.h
        TaskErrorMapper.cpp
        TaskPresentationFormatter.h
        TaskPresentationFormatter.cpp
        TaskCategoryPresentation.h
        TaskCategoryPresentation.cpp
        TaskCategoryViewModel.h
        TaskCategoryViewModel.cpp
        TaskGraphViewModel.h
        TaskGraphViewModel.cpp
        TaskGraphProjectionModels.h
        TaskGraphProjectionModels.cpp
        TaskGraphLayout.h
        TaskGraphLayout.cpp
        TaskListViewModel.h
        TaskListViewModel.cpp
)

target_include_directories(smartmate_viewmodel
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(smartmate_viewmodel
    PUBLIC
        smartmate_model
        Qt6::Core
        Qt6::QmlIntegration
)
