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

qt_add_executable(tst_qsettings_appearance_repository
    persistence/tst_QSettingsAppearanceRepository.cpp
)
target_link_libraries(tst_qsettings_appearance_repository
    PRIVATE smartmate_persistence Qt6::Test
)
add_test(NAME persistence.appearance_settings
         COMMAND tst_qsettings_appearance_repository)

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

qt_add_executable(tst_appearance_settings
    viewmodel/tst_AppearanceSettings.cpp
    fakes/FakeAppearanceSettingsRepository.h
)
target_include_directories(tst_appearance_settings PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tst_appearance_settings PRIVATE smartmate_viewmodel Qt6::Test)
add_test(NAME viewmodel.appearance_settings COMMAND tst_appearance_settings)

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

qt_add_executable(tst_settings_widgets
    widgets/tst_SettingsWidgets.cpp
)
target_link_libraries(tst_settings_widgets
    PRIVATE smartmate_widgets Qt6::Test
)
add_test(NAME view.widgets_settings COMMAND tst_settings_widgets -platform offscreen)
set_tests_properties(view.widgets_settings PROPERTIES TIMEOUT 15)

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

qt_add_executable(tst_qml_pickers
    qml/quicktest_main.cpp
)

target_compile_definitions(tst_qml_pickers
    PRIVATE
        QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
)

target_link_libraries(tst_qml_pickers
    PRIVATE
        smartmate_persistence
        smartmate_viewmodel
        smartmate_viewmodel_qml
        smartmate_viewmodel_qmlplugin
        smartmate_ui
        smartmate_uiplugin
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickTest
        Qt6::Sql
)

# QuickTest需要把静态QML模块的插件和资源导入测试进程，才能按URI创建选择器。
qt_import_qml_plugins(tst_qml_pickers)

add_test(
    NAME view.qml_pickers
    COMMAND tst_qml_pickers -platform offscreen
)
set_tests_properties(
    view.qml_pickers
    PROPERTIES
        ENVIRONMENT "QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic"
        TIMEOUT 15
)

# 图页使用独立进程与内存数据库，避免节点夹具污染列表/选择器测试。
qt_add_executable(tst_qml_dependency_graph
    qml_graph/quicktest_main.cpp
)

target_compile_definitions(tst_qml_dependency_graph
    PRIVATE
        QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml_graph"
)

target_link_libraries(tst_qml_dependency_graph
    PRIVATE
        smartmate_persistence
        smartmate_viewmodel
        smartmate_viewmodel_qml
        smartmate_viewmodel_qmlplugin
        smartmate_ui
        smartmate_uiplugin
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickTest
        Qt6::Sql
)

qt_import_qml_plugins(tst_qml_dependency_graph)

add_test(
    NAME view.qml_dependency_graph
    COMMAND tst_qml_dependency_graph -platform offscreen
)
set_tests_properties(
    view.qml_dependency_graph
    PROPERTIES
        ENVIRONMENT "QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic"
        TIMEOUT 15
)

add_test(
    NAME view.qml_bootstrap
    COMMAND SmartMate --smoke-test -platform offscreen
)
set_tests_properties(view.qml_bootstrap PROPERTIES TIMEOUT 10)

add_test(
    NAME view.widgets_bootstrap
    COMMAND SmartMateWidgets --smoke-test -platform offscreen
)
set_tests_properties(view.widgets_bootstrap PROPERTIES TIMEOUT 10)

get_target_property(qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(qt_runtime_directory "${qmake_executable}" DIRECTORY)
get_filename_component(compiler_runtime_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
set(runtime_path_modifications
    "PATH=path_list_prepend:${compiler_runtime_directory}"
    "PATH=path_list_prepend:${qt_runtime_directory}"
)

set_tests_properties(
    model.task_service
    model.task_category_service
    model.task_category_task_service
    model.task_state_machine
    model.task_dependency_cancellation
    model.task_state_transitions
    model.task_ordering
    persistence.sqlite_task_repository
    persistence.appearance_settings
    viewmodel.task_viewmodels
    viewmodel.appearance_settings
    viewmodel.contracts
    view.widgets_settings
    view.widgets_tasks
    view.widgets_graph
    integration.widgets_appearance
    integration.widgets_tasks
    integration.widgets_category_dependencies
    integration.widgets_graph
    integration.task_creation_flow
    view.qml_pickers
    view.qml_dependency_graph
    view.qml_bootstrap
    view.widgets_bootstrap
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
