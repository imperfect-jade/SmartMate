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
        smartmate_viewmodelplugin
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
        smartmate_viewmodelplugin
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

get_target_property(qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(qt_runtime_directory "${qmake_executable}" DIRECTORY)
get_filename_component(compiler_runtime_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
set(runtime_path_modifications
    "PATH=path_list_prepend:${qt_runtime_directory}"
    "PATH=path_list_prepend:${compiler_runtime_directory}"
)

set_tests_properties(
    model.task_service
    model.task_state_machine
    model.task_dependency_cancellation
    model.task_state_transitions
    model.task_ordering
    persistence.sqlite_task_repository
    persistence.appearance_settings
    viewmodel.task_viewmodels
    viewmodel.appearance_settings
    integration.task_creation_flow
    view.qml_pickers
    view.qml_dependency_graph
    view.qml_bootstrap
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
