add_library(smartmate_viewmodel STATIC
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
    TaskPlanProjection.h
    TaskPlanProjection.cpp
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
    TaskFocusViewModel.h
    TaskFocusViewModel.cpp
    TaskDetailsViewModel.h
    TaskDetailsViewModel.cpp
)

target_include_directories(smartmate_viewmodel
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(smartmate_viewmodel
    PUBLIC
        smartmate_viewmodel_contracts
        smartmate_model
        Qt6::Core
)

if(SMARTMATE_BUILD_QML_BASELINE)
    # 删除门禁前的 QML adapter 通过 foreign types 消费具体 ViewModel 元对象。
    qt_extract_metatypes(smartmate_viewmodel)
endif()
