add_library(smartmate_viewmodel_contracts STATIC
    AppearanceSettingsContract.h
    TaskCategoryContract.h
    TaskDependencyContract.h
    TaskEditorContract.h
    TaskFocusContract.h
    TaskDetailsContract.h
    TaskGraphContract.h
    TaskListContract.h
    ViewModelContracts.cpp
)

target_include_directories(smartmate_viewmodel_contracts
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(smartmate_viewmodel_contracts
    PUBLIC
        smartmate_common
        Qt6::Core
)
