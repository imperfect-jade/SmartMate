qt_add_executable(tst_task_service
    model/tst_TaskService.cpp
    fakes/FakeTaskRepository.h
)

target_include_directories(tst_task_service
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(tst_task_service
    PRIVATE
        smartmate_model
        Qt6::Test
)

add_test(NAME model.task_service COMMAND tst_task_service)

qt_add_executable(tst_task_category_service
    model/tst_TaskCategoryService.cpp
    fakes/FakeTaskCategoryRepository.h
)
target_include_directories(tst_task_category_service
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(tst_task_category_service
    PRIVATE smartmate_model Qt6::Test
)
add_test(NAME model.task_category_service COMMAND tst_task_category_service)

qt_add_executable(tst_task_category_task_service
    model/tst_TaskCategoryTaskService.cpp
    fakes/FakeTaskCategoryRepository.h
)
target_include_directories(tst_task_category_task_service
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(tst_task_category_task_service
    PRIVATE smartmate_model Qt6::Test
)
add_test(NAME model.task_category_task_service
         COMMAND tst_task_category_task_service)

qt_add_executable(tst_task_state_machine
    model/tst_TaskStateMachine.cpp
)

target_link_libraries(tst_task_state_machine
    PRIVATE
        smartmate_model
        Qt6::Test
)

add_test(NAME model.task_state_machine COMMAND tst_task_state_machine)

qt_add_executable(tst_task_dependency_cancellation
    model/tst_TaskDependencyCancellation.cpp
)

target_link_libraries(tst_task_dependency_cancellation
    PRIVATE
        smartmate_model
        Qt6::Test
)

add_test(NAME model.task_dependency_cancellation
         COMMAND tst_task_dependency_cancellation)

qt_add_executable(tst_task_state_transitions
    model/tst_TaskStateTransitions.cpp
    fakes/FakeTaskRepository.h
    fakes/FakeTaskDependencyRepository.h
    fakes/FakeTaskCreationRepository.h
)

target_include_directories(tst_task_state_transitions
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(tst_task_state_transitions
    PRIVATE
        smartmate_model
        Qt6::Test
)

add_test(NAME model.task_state_transitions COMMAND tst_task_state_transitions)

qt_add_executable(tst_task_ordering
    model/tst_TaskOrderingPolicy.cpp
    fakes/FakeTaskRepository.h
)

target_include_directories(tst_task_ordering
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(tst_task_ordering
    PRIVATE
        smartmate_model
        Qt6::Test
)

add_test(NAME model.task_ordering COMMAND tst_task_ordering)

qt_add_executable(tst_focus_service
    model/tst_FocusService.cpp
    fakes/FakeFocusSessionRepository.h
    fakes/FakeTaskActivityRepository.h
    fakes/FakeTaskCategoryRepository.h
    fakes/FakeTaskRepository.h
)
target_include_directories(tst_focus_service PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tst_focus_service PRIVATE smartmate_model Qt6::Test)
add_test(NAME model.focus_service COMMAND tst_focus_service)

qt_add_executable(tst_statistics_service
    model/tst_StatisticsService.cpp
    fakes/FakeTaskActivityRepository.h
    fakes/FakeTaskRepository.h
    fakes/FakeTaskDependencyRepository.h
)
target_include_directories(tst_statistics_service PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tst_statistics_service PRIVATE smartmate_model Qt6::Test)
add_test(NAME model.statistics_service COMMAND tst_statistics_service)

qt_add_executable(tst_desktop_pet_settings_service
    model/tst_DesktopPetSettingsService.cpp
    fakes/FakeDesktopPetSettingsRepository.h
)
target_include_directories(tst_desktop_pet_settings_service PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tst_desktop_pet_settings_service PRIVATE smartmate_model Qt6::Test)
add_test(NAME model.desktop_pet_settings COMMAND tst_desktop_pet_settings_service)

qt_add_executable(tst_sqlite_task_repository
    persistence/tst_SqliteTaskRepository.cpp
)

target_link_libraries(tst_sqlite_task_repository
    PRIVATE
        smartmate_persistence
        Qt6::Sql
        Qt6::Test
)

add_test(
    NAME persistence.sqlite_task_repository
    COMMAND tst_sqlite_task_repository
)

qt_add_executable(tst_sqlite_focus_session_repository
    persistence/tst_SqliteFocusSessionRepository.cpp
)
target_link_libraries(tst_sqlite_focus_session_repository
    PRIVATE
        smartmate_persistence
        Qt6::Sql
        Qt6::Test
)
add_test(
    NAME persistence.sqlite_focus_session_repository
    COMMAND tst_sqlite_focus_session_repository
)

qt_add_executable(tst_qsettings_appearance_repository
    persistence/tst_QSettingsAppearanceRepository.cpp
)
target_link_libraries(tst_qsettings_appearance_repository
    PRIVATE smartmate_persistence Qt6::Test
)
add_test(NAME persistence.appearance_settings
         COMMAND tst_qsettings_appearance_repository)

qt_add_executable(tst_qsettings_desktop_pet_repository
    persistence/tst_QSettingsDesktopPetRepository.cpp
)
target_link_libraries(tst_qsettings_desktop_pet_repository
    PRIVATE smartmate_persistence Qt6::Test
)
add_test(NAME persistence.desktop_pet_settings
         COMMAND tst_qsettings_desktop_pet_repository)

qt_add_executable(tst_task_viewmodels
    viewmodel/tst_TaskViewModels.cpp
    fakes/FakeTaskRepository.h
)

target_include_directories(tst_task_viewmodels
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(tst_task_viewmodels
    PRIVATE
        smartmate_viewmodel
        Qt6::Test
)

add_test(NAME viewmodel.task_viewmodels COMMAND tst_task_viewmodels)

qt_add_executable(tst_task_presentation
    viewmodel/tst_TaskPresentation.cpp
)
target_link_libraries(tst_task_presentation
    PRIVATE smartmate_viewmodel Qt6::Test
)
add_test(NAME viewmodel.task_presentation COMMAND tst_task_presentation)

qt_add_executable(tst_task_projection_sources
    viewmodel/tst_TaskProjectionSources.cpp
    fakes/FakeTaskRepository.h
    fakes/FakeTaskDependencyRepository.h
    fakes/FakeTaskCreationRepository.h
    fakes/FakeTaskBatchTransitionRepository.h
    fakes/FakeTaskDeletionRepository.h
    fakes/FakeTaskCategoryRepository.h
)
target_include_directories(tst_task_projection_sources
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(tst_task_projection_sources
    PRIVATE smartmate_viewmodel Qt6::Test
)
add_test(NAME viewmodel.task_projection_sources COMMAND tst_task_projection_sources)

qt_add_executable(tst_appearance_settings
    viewmodel/tst_AppearanceSettings.cpp
    fakes/FakeAppearanceSettingsRepository.h
)
target_include_directories(tst_appearance_settings PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tst_appearance_settings PRIVATE smartmate_viewmodel Qt6::Test)
add_test(NAME viewmodel.appearance_settings COMMAND tst_appearance_settings)

qt_add_executable(tst_desktop_pet_settings_viewmodel
    viewmodel/tst_DesktopPetSettingsViewModel.cpp
    fakes/FakeDesktopPetSettingsRepository.h
)
target_include_directories(tst_desktop_pet_settings_viewmodel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tst_desktop_pet_settings_viewmodel
    PRIVATE smartmate_viewmodel Qt6::Test
)
add_test(NAME viewmodel.desktop_pet_settings COMMAND tst_desktop_pet_settings_viewmodel)

qt_add_executable(tst_viewmodel_contracts
    viewmodel/tst_ViewModelContracts.cpp
    fakes/FakeAppearanceSettingsRepository.h
    fakes/FakeTaskRepository.h
    fakes/FakeTaskDependencyRepository.h
    fakes/FakeTaskCreationRepository.h
    fakes/FakeTaskBatchTransitionRepository.h
    fakes/FakeTaskDeletionRepository.h
    fakes/FakeTaskCategoryRepository.h
)
target_include_directories(tst_viewmodel_contracts PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tst_viewmodel_contracts
    PRIVATE smartmate_viewmodel Qt6::Test
)
add_test(NAME viewmodel.contracts COMMAND tst_viewmodel_contracts)

qt_add_executable(tst_statistics_viewmodel
    viewmodel/tst_StatisticsViewModel.cpp
    fakes/FakeTaskActivityRepository.h
    fakes/FakeTaskRepository.h
    fakes/FakeTaskDependencyRepository.h
    fakes/FakeTaskCreationRepository.h
    fakes/FakeTaskBatchTransitionRepository.h
    fakes/FakeTaskDeletionRepository.h
    fakes/FakeTaskCategoryRepository.h
)
target_include_directories(tst_statistics_viewmodel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tst_statistics_viewmodel
    PRIVATE smartmate_viewmodel Qt6::Test
)
add_test(NAME viewmodel.statistics COMMAND tst_statistics_viewmodel)

qt_add_executable(tst_focus_viewmodel
    viewmodel/tst_FocusViewModel.cpp
    fakes/FakeFocusSessionRepository.h
    fakes/FakeTaskActivityRepository.h
    fakes/FakeTaskRepository.h
    fakes/FakeTaskDependencyRepository.h
    fakes/FakeTaskCreationRepository.h
    fakes/FakeTaskBatchTransitionRepository.h
    fakes/FakeTaskDeletionRepository.h
    fakes/FakeTaskCategoryRepository.h
)
target_include_directories(tst_focus_viewmodel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tst_focus_viewmodel
    PRIVATE smartmate_viewmodel Qt6::Test
)
add_test(NAME viewmodel.focus COMMAND tst_focus_viewmodel)

qt_add_executable(tst_settings_widgets
    widgets/tst_SettingsWidgets.cpp
)
target_link_libraries(tst_settings_widgets
    PRIVATE smartmate_widgets Qt6::Test
)
add_test(NAME view.widgets_settings COMMAND tst_settings_widgets -platform offscreen)
set_tests_properties(view.widgets_settings PROPERTIES TIMEOUT 15)

qt_add_executable(tst_desktop_pet_widgets
    widgets/tst_DesktopPetWidgets.cpp
)
target_link_libraries(tst_desktop_pet_widgets
    PRIVATE smartmate_widgets Qt6::Test
)
add_test(NAME view.widgets_desktop_pet
    COMMAND tst_desktop_pet_widgets -platform offscreen)
set_tests_properties(view.widgets_desktop_pet PROPERTIES TIMEOUT 15)

qt_add_executable(tst_task_widgets
    widgets/tst_TaskWidgets.cpp
)
target_link_libraries(tst_task_widgets
    PRIVATE smartmate_widgets Qt6::Test
)
add_test(NAME view.widgets_tasks COMMAND tst_task_widgets -platform offscreen)
set_tests_properties(view.widgets_tasks PROPERTIES TIMEOUT 20)

qt_add_executable(tst_graph_widgets
    widgets/tst_GraphWidgets.cpp
)
target_link_libraries(tst_graph_widgets PRIVATE smartmate_widgets Qt6::Test)
add_test(NAME view.widgets_graph COMMAND tst_graph_widgets -platform offscreen)
set_tests_properties(view.widgets_graph PROPERTIES TIMEOUT 20)

qt_add_executable(tst_statistics_widgets
    widgets/tst_StatisticsWidgets.cpp
)
target_link_libraries(tst_statistics_widgets
    PRIVATE smartmate_widgets Qt6::Charts Qt6::Test
)
add_test(NAME view.widgets_statistics
    COMMAND tst_statistics_widgets -platform offscreen)
set_tests_properties(view.widgets_statistics PROPERTIES TIMEOUT 20)

qt_add_executable(tst_focus_widgets
    widgets/tst_FocusWidgets.cpp
)
target_link_libraries(tst_focus_widgets
    PRIVATE smartmate_widgets Qt6::Test
)
add_test(NAME view.widgets_focus
    COMMAND tst_focus_widgets -platform offscreen)
set_tests_properties(view.widgets_focus PROPERTIES TIMEOUT 20)

qt_add_executable(tst_widgets_appearance_flow
    integration/tst_WidgetsAppearanceFlow.cpp
    fakes/FakeAppearanceSettingsRepository.h
)
target_include_directories(tst_widgets_appearance_flow PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tst_widgets_appearance_flow
    PRIVATE smartmate_widgets smartmate_viewmodel Qt6::Test
)
add_test(NAME integration.widgets_appearance COMMAND tst_widgets_appearance_flow -platform offscreen)
set_tests_properties(integration.widgets_appearance PROPERTIES TIMEOUT 15)

qt_add_executable(tst_widgets_task_flow
    integration/tst_WidgetsTaskFlow.cpp
)
target_link_libraries(tst_widgets_task_flow
    PRIVATE smartmate_widgets smartmate_persistence smartmate_viewmodel Qt6::Test
)
add_test(NAME integration.widgets_tasks COMMAND tst_widgets_task_flow -platform offscreen)
set_tests_properties(integration.widgets_tasks PROPERTIES TIMEOUT 25)

qt_add_executable(tst_widgets_category_dependency_flow
    integration/tst_WidgetsCategoryDependencyFlow.cpp
)
target_link_libraries(tst_widgets_category_dependency_flow
    PRIVATE smartmate_widgets smartmate_persistence smartmate_viewmodel Qt6::Test
)
add_test(NAME integration.widgets_category_dependencies
    COMMAND tst_widgets_category_dependency_flow -platform offscreen)
set_tests_properties(integration.widgets_category_dependencies PROPERTIES TIMEOUT 30)

qt_add_executable(tst_widgets_graph_flow
    integration/tst_WidgetsGraphFlow.cpp
)
target_link_libraries(tst_widgets_graph_flow
    PRIVATE smartmate_widgets smartmate_persistence smartmate_viewmodel Qt6::Test
)
add_test(NAME integration.widgets_graph COMMAND tst_widgets_graph_flow -platform offscreen)
set_tests_properties(integration.widgets_graph PROPERTIES TIMEOUT 30)

qt_add_executable(tst_widgets_statistics_flow
    integration/tst_WidgetsStatisticsFlow.cpp
)
target_link_libraries(tst_widgets_statistics_flow
    PRIVATE
        smartmate_app_core
        smartmate_widgets
        smartmate_persistence
        smartmate_viewmodel
        Qt6::Test
)
add_test(NAME integration.widgets_statistics
    COMMAND tst_widgets_statistics_flow -platform offscreen)
set_tests_properties(integration.widgets_statistics PROPERTIES TIMEOUT 30)

qt_add_executable(tst_widgets_focus_flow
    integration/tst_WidgetsFocusFlow.cpp
)
target_link_libraries(tst_widgets_focus_flow
    PRIVATE
        smartmate_widgets
        smartmate_persistence
        smartmate_viewmodel
        Qt6::Test
)
add_test(NAME integration.widgets_focus
    COMMAND tst_widgets_focus_flow -platform offscreen)
set_tests_properties(integration.widgets_focus PROPERTIES TIMEOUT 30)

qt_add_executable(tst_widgets_desktop_pet_flow
    integration/tst_WidgetsDesktopPetFlow.cpp
    fakes/FakeAppearanceSettingsRepository.h
    fakes/FakeDesktopPetSettingsRepository.h
)
target_include_directories(tst_widgets_desktop_pet_flow PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tst_widgets_desktop_pet_flow
    PRIVATE smartmate_widgets smartmate_persistence smartmate_viewmodel Qt6::Test
)
add_test(NAME integration.widgets_desktop_pet
    COMMAND tst_widgets_desktop_pet_flow -platform offscreen)
set_tests_properties(integration.widgets_desktop_pet PROPERTIES TIMEOUT 30)

qt_add_executable(tst_task_creation_flow
    integration/tst_TaskCreationFlow.cpp
)

target_link_libraries(tst_task_creation_flow
    PRIVATE
        smartmate_persistence
        smartmate_viewmodel
        Qt6::Sql
        Qt6::Test
)

add_test(NAME integration.task_creation_flow COMMAND tst_task_creation_flow)

qt_add_executable(tst_focus_app_lifecycle
    integration/tst_FocusAppLifecycle.cpp
)
target_link_libraries(tst_focus_app_lifecycle
    PRIVATE smartmate_app_core smartmate_persistence Qt6::Test
)
add_test(NAME integration.focus_app_lifecycle COMMAND tst_focus_app_lifecycle)

add_test(
    NAME view.application_bootstrap
    COMMAND SmartMate --smoke-test -platform offscreen
)
set_tests_properties(view.application_bootstrap PROPERTIES TIMEOUT 10)

get_target_property(qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(qt_runtime_directory "${qmake_executable}" DIRECTORY)
get_filename_component(compiler_runtime_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
set(runtime_path_modifications
    "PATH=path_list_prepend:${compiler_runtime_directory}"
    "PATH=path_list_prepend:${qt_runtime_directory}"
)

set(runtime_tests
    model.task_service
    model.task_category_service
    model.task_category_task_service
    model.task_state_machine
    model.task_dependency_cancellation
    model.task_state_transitions
    model.task_ordering
    model.focus_service
    model.statistics_service
    model.desktop_pet_settings
    persistence.sqlite_task_repository
    persistence.sqlite_focus_session_repository
    persistence.appearance_settings
    persistence.desktop_pet_settings
    viewmodel.task_viewmodels
    viewmodel.task_presentation
    viewmodel.task_projection_sources
    viewmodel.appearance_settings
    viewmodel.desktop_pet_settings
    viewmodel.contracts
    viewmodel.statistics
    viewmodel.focus
    view.widgets_settings
    view.widgets_desktop_pet
    view.widgets_tasks
    view.widgets_graph
    view.widgets_statistics
    view.widgets_focus
    integration.widgets_appearance
    integration.widgets_tasks
    integration.widgets_category_dependencies
    integration.widgets_graph
    integration.widgets_statistics
    integration.widgets_focus
    integration.widgets_desktop_pet
    integration.task_creation_flow
    integration.focus_app_lifecycle
    view.application_bootstrap
)
set_tests_properties(
    ${runtime_tests}
    PROPERTIES
        ENVIRONMENT_MODIFICATION
            "${runtime_path_modifications}"
)

add_test(
    NAME architecture.mvvm_boundaries
    COMMAND
        ${CMAKE_COMMAND}
        -DROOT_DIR=${PROJECT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/architecture/check_mvvm_boundaries.cmake
)
