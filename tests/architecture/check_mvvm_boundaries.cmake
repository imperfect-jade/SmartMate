cmake_minimum_required(VERSION 3.24)

# 将文档中的 MVVM 依赖约束变成可执行守卫，防止后续功能通过跨层捷径实现。
if(NOT DEFINED ROOT_DIR)
    message(FATAL_ERROR "ROOT_DIR must point to the SmartMate source tree")
endif()

file(TO_CMAKE_PATH "${ROOT_DIR}" ROOT_DIR)
set_property(GLOBAL PROPERTY MVVM_VIOLATIONS "")

function(record_violation source_file rule)
    file(RELATIVE_PATH relative_path "${ROOT_DIR}" "${source_file}")
    set_property(GLOBAL APPEND PROPERTY MVVM_VIOLATIONS
        "${relative_path}: ${rule}")
endfunction()

function(scan_includes source_root layer_name)
    # 按层扫描 include，发现反向依赖或把 UI/SQL API 泄漏到错误层时统一报告。
    set(forbidden_patterns ${ARGN})
    file(GLOB_RECURSE source_files LIST_DIRECTORIES FALSE
        "${source_root}/*.h"
        "${source_root}/*.hpp"
        "${source_root}/*.cpp"
        "${source_root}/*.cc"
        "${source_root}/*.cxx")

    foreach(source_file IN LISTS source_files)
        file(READ "${source_file}" contents)
        string(REGEX MATCHALL "#[ \t]*include[ \t]*[<\"][^>\"\r\n]*[>\"]"
            include_lines "${contents}")

        foreach(include_line IN LISTS include_lines)
            string(TOLOWER "${include_line}" include_lower)
            foreach(pattern IN LISTS forbidden_patterns)
                if(include_lower MATCHES "${pattern}")
                    record_violation("${source_file}"
                        "${layer_name} contains forbidden include: ${include_line}")
                endif()
            endforeach()
        endforeach()
    endforeach()
endfunction()

scan_includes("${ROOT_DIR}/src/model/domain" "Model domain"
    "qobject" "qtquick" "qquick" "qtqml" "qqml" "qtsql" "qsql"
    "viewmodel" "view/")

scan_includes("${ROOT_DIR}/src/model/dependencies" "Model dependency graph"
    "qobject" "qtquick" "qquick" "qtqml" "qqml" "qtsql" "qsql"
    "viewmodel" "view/")

scan_includes("${ROOT_DIR}/src/model/planner" "Model planner"
    "qobject" "qtquick" "qquick" "qtqml" "qqml" "qtsql" "qsql"
    "viewmodel" "view/")

scan_includes("${ROOT_DIR}/src/model/services" "Model service"
    "qtquick" "qquick" "qtqml" "qqml" "qtsql" "qsql"
    "viewmodel" "view/")

scan_includes("${ROOT_DIR}/src/model/repositories" "Repository interface"
    "qtquick" "qquick" "qtqml" "qqml" "qtsql" "qsql"
    "viewmodel" "view/")

scan_includes("${ROOT_DIR}/src/model/persistence" "Model persistence"
    "qtquick" "qquick" "qtqml" "qqml"
    "viewmodel" "view/")

scan_includes("${ROOT_DIR}/src/common" "Common"
    "model/" "domain/" "services/" "repositories/" "persistence/"
    "viewmodel" "contracts/" "view/"
    "qtquick" "qquick" "qtqml" "qqml" "qtsql" "qsql"
    "qsettings" "qtwidgets" "qwidget" "qdialog" "qgraphics")

scan_includes("${ROOT_DIR}/src/viewmodel/contracts" "ViewModel Contracts"
    "domain/" "services/" "repositories/" "persistence/"
    "appearance.*viewmodel" "task.*viewmodel" "appviewmodel"
    "qtquick" "qquick" "qtqml" "qqml" "qtsql" "qsql"
    "qtwidgets" "qwidget" "qdialog" "qgraphics")

scan_includes("${ROOT_DIR}/src/viewmodel" "ViewModel"
    "qtquick" "qquick" "qqmlengine" "qqmlcontext" "qtsql" "qsql"
    "model/persistence" "view/")

scan_includes("${ROOT_DIR}/src/view/widgets" "Qt Widgets View"
    "domain/" "services/" "repositories/" "persistence/"
    "appviewmodel" "appearance.*viewmodel\\.h" "task.*viewmodel\\.h"
    "qtquick" "qquick" "qtqml" "qqml" "qtsql" "qsql" "qsettings")

# include 检查之外，再验证 CMake 链接关系，避免通过传递依赖绕过边界。
set(model_cmake "${ROOT_DIR}/src/model/CMakeLists.txt")
if(EXISTS "${model_cmake}")
    file(READ "${model_cmake}" model_cmake_contents)
    string(TOLOWER "${model_cmake_contents}" model_cmake_lower)
    if(model_cmake_lower MATCHES "qt6::(quick|qml|sql)")
        record_violation("${model_cmake}"
            "smartmate_model may link only Qt Core, not Quick/Qml/Sql")
    endif()
endif()

set(common_cmake "${ROOT_DIR}/src/common/CMakeLists.txt")
if(EXISTS "${common_cmake}")
    file(READ "${common_cmake}" common_cmake_contents)
    string(TOLOWER "${common_cmake_contents}" common_cmake_lower)
    if(NOT common_cmake_lower MATCHES "qt6::core")
        record_violation("${common_cmake}"
            "smartmate_common must link Qt Core")
    endif()
    if(common_cmake_lower MATCHES "smartmate_(model|viewmodel|persistence|ui)|qt6::(qml|quick|sql|widgets)")
        record_violation("${common_cmake}"
            "smartmate_common may depend only on Qt Core")
    endif()
endif()

set(contracts_cmake "${ROOT_DIR}/src/viewmodel/contracts/CMakeLists.txt")
if(EXISTS "${contracts_cmake}")
    file(READ "${contracts_cmake}" contracts_cmake_contents)
    string(TOLOWER "${contracts_cmake_contents}" contracts_cmake_lower)
    if(NOT contracts_cmake_lower MATCHES "smartmate_common"
       OR NOT contracts_cmake_lower MATCHES "qt6::core")
        record_violation("${contracts_cmake}"
            "smartmate_viewmodel_contracts must link smartmate_common and Qt Core")
    endif()
    if(contracts_cmake_lower MATCHES "smartmate_model|smartmate_persistence|smartmate_viewmodel[ \t\r\n]|qt6::(qml|quick|sql|widgets)")
        record_violation("${contracts_cmake}"
            "Contracts may not link Model, concrete ViewModel, persistence, QML, Quick, SQL, or Widgets")
    endif()
    if(NOT contracts_cmake_lower MATCHES "taskfocuscontract"
       OR NOT contracts_cmake_lower MATCHES "taskdetailscontract")
        record_violation("${contracts_cmake}"
            "Task focus and details must remain independent Contracts")
    endif()
endif()

set(persistence_cmake "${ROOT_DIR}/src/model/persistence/CMakeLists.txt")
if(EXISTS "${persistence_cmake}")
    file(READ "${persistence_cmake}" persistence_cmake_contents)
    string(TOLOWER "${persistence_cmake_contents}" persistence_cmake_lower)
    if(NOT persistence_cmake_lower MATCHES "smartmate_model")
        record_violation("${persistence_cmake}"
            "smartmate_persistence must depend on smartmate_model")
    endif()
    if(NOT persistence_cmake_lower MATCHES "qt6::sql")
        record_violation("${persistence_cmake}"
            "smartmate_persistence must own the Qt SQL dependency")
    endif()
    if(persistence_cmake_lower MATCHES "smartmate_viewmodel|qt6::(quick|qml)")
        record_violation("${persistence_cmake}"
            "smartmate_persistence may not depend on ViewModel, Qt Quick, or QML")
    endif()
endif()

set(task_list_header "${ROOT_DIR}/src/viewmodel/TaskListViewModel.h")
if(EXISTS "${task_list_header}")
    file(READ "${task_list_header}" task_list_contents)
    if(task_list_contents MATCHES "focusTaskId|selectedTaskId|selectTask")
        record_violation("${task_list_header}"
            "TaskListViewModel may not absorb focus or details session state")
    endif()
endif()

foreach(split_viewmodel IN ITEMS TaskFocusViewModel TaskDetailsViewModel)
    set(split_source "${ROOT_DIR}/src/viewmodel/${split_viewmodel}.cpp")
    if(NOT EXISTS "${split_source}")
        record_violation("${ROOT_DIR}/src/viewmodel/CMakeLists.txt"
            "${split_viewmodel} is required by the task-main-flow boundary")
    else()
        file(READ "${split_source}" split_contents)
        if(split_contents MATCHES "TaskListViewModel")
            record_violation("${split_source}"
                "Split child ViewModels may not call or include TaskListViewModel")
        endif()
    endif()
endforeach()

# 计划与类别目录必须只有一个 Service 查询入口；共享源由 AppViewModel 显式拥有，
# 不能退化为全局缓存、Service Locator 或各消费者再次独立查询。
set(projection_source "${ROOT_DIR}/src/viewmodel/TaskProjectionSources.cpp")
if(NOT EXISTS "${projection_source}")
    record_violation("${ROOT_DIR}/src/viewmodel/CMakeLists.txt"
        "Shared task projection sources are required")
endif()

file(GLOB task_viewmodel_sources LIST_DIRECTORIES FALSE
    "${ROOT_DIR}/src/viewmodel/Task*ViewModel.cpp")
foreach(source_file IN LISTS task_viewmodel_sources)
    file(READ "${source_file}" source_contents)
    if(source_contents MATCHES "listRecommendedTasks" OR source_contents MATCHES "listCategories")
        record_violation("${source_file}"
            "Concrete task ViewModels must consume shared projection sources instead of duplicating Service queries")
    endif()
endforeach()

foreach(timer_owner IN ITEMS TaskListViewModel TaskFocusViewModel)
    set(timer_header "${ROOT_DIR}/src/viewmodel/${timer_owner}.h")
    if(EXISTS "${timer_header}")
        file(READ "${timer_header}" timer_contents)
        if(timer_contents MATCHES "QTimer")
            record_violation("${timer_header}"
                "The minute refresh timer must be owned only by TaskPlanProjectionSource")
        endif()
    endif()
endforeach()

set(app_viewmodel_header "${ROOT_DIR}/src/viewmodel/AppViewModel.h")
if(EXISTS "${app_viewmodel_header}")
    file(READ "${app_viewmodel_header}" app_viewmodel_contents)
    string(FIND "${app_viewmodel_contents}" "TaskPlanProjectionSource m_taskPlanSource" plan_source_position)
    string(FIND "${app_viewmodel_contents}" "TaskCategoryProjectionSource m_taskCategorySource" category_source_position)
    string(FIND "${app_viewmodel_contents}" "TaskListViewModel m_taskList" first_consumer_position)
    if(plan_source_position LESS 0 OR category_source_position LESS 0
       OR first_consumer_position LESS 0
       OR plan_source_position GREATER first_consumer_position
       OR category_source_position GREATER first_consumer_position)
        record_violation("${app_viewmodel_header}"
            "AppViewModel must own both projection sources before their consumers")
    endif()
endif()

set(viewmodel_cmake "${ROOT_DIR}/src/viewmodel/CMakeLists.txt")
if(EXISTS "${viewmodel_cmake}")
    file(READ "${viewmodel_cmake}" viewmodel_cmake_contents)
    string(TOLOWER "${viewmodel_cmake_contents}" viewmodel_cmake_lower)
    if(viewmodel_cmake_lower MATCHES "qt6::(quick|sql)")
        record_violation("${viewmodel_cmake}"
            "smartmate_viewmodel may not link Qt Quick or Qt SQL")
    endif()
    if(viewmodel_cmake_lower MATCHES "smartmate_persistence")
        record_violation("${viewmodel_cmake}"
            "smartmate_viewmodel may not link concrete persistence")
    endif()
    if(NOT viewmodel_cmake_lower MATCHES "smartmate_viewmodel_contracts")
        record_violation("${viewmodel_cmake}"
            "smartmate_viewmodel must implement and link the Contracts target")
    endif()
endif()

set(widgets_cmake "${ROOT_DIR}/src/view/widgets/CMakeLists.txt")
if(EXISTS "${widgets_cmake}")
    file(READ "${widgets_cmake}" widgets_cmake_contents)
    string(TOLOWER "${widgets_cmake_contents}" widgets_cmake_lower)
    if(NOT widgets_cmake_lower MATCHES "smartmate_viewmodel_contracts"
       OR NOT widgets_cmake_lower MATCHES "smartmate_common"
       OR NOT widgets_cmake_lower MATCHES "qt6::widgets")
        record_violation("${widgets_cmake}"
            "smartmate_widgets must link Contracts, Common, and Qt Widgets")
    endif()
    if(widgets_cmake_lower MATCHES "smartmate_viewmodel[ \t\r\n]|smartmate_model|smartmate_persistence|smartmate_ui|qt6::(qml|quick|sql)")
        record_violation("${widgets_cmake}"
            "smartmate_widgets may not link concrete ViewModel, Model, persistence, QML, Quick, or SQL")
    endif()
endif()

set(app_cmake "${ROOT_DIR}/src/app/CMakeLists.txt")
if(EXISTS "${app_cmake}")
    file(READ "${app_cmake}" app_cmake_contents)
    string(TOLOWER "${app_cmake_contents}" app_cmake_lower)
    string(FIND "${app_cmake_lower}" "qt_add_executable(smartmate win32" official_target_start)
    if(official_target_start LESS 0)
        record_violation("${app_cmake}"
            "The official SmartMate Widgets target is required")
    else()
        if(NOT app_cmake_lower MATCHES "smartmate_widgets"
           OR NOT app_cmake_lower MATCHES "qt6::widgets")
            record_violation("${app_cmake}"
                "The official SmartMate target must link smartmate_widgets and Qt Widgets")
        endif()
    endif()
    if(app_cmake_lower MATCHES "smartmatewidgets|smartmateqmlbaseline|smartmate_(ui|viewmodel_qml)|qt6::(qml|quick)")
        record_violation("${app_cmake}"
            "Removed migration frontend targets and Qt QML/Quick links may not remain")
    endif()
endif()

# 语义资格不能靠禁止 TaskStatus 的脆弱正则判断；这里只锁定数据入口，
# 具体候选和命令资格由 Model/ViewModel 契约测试验证。
set(dependency_viewmodel "${ROOT_DIR}/src/viewmodel/TaskDependencyViewModel.cpp")
if(EXISTS "${dependency_viewmodel}")
    file(READ "${dependency_viewmodel}" dependency_viewmodel_contents)
    if(NOT dependency_viewmodel_contents MATCHES "taskDependencyEditContext")
        record_violation("${dependency_viewmodel}"
            "Dependency editor must consume the Model edit-context contract")
    endif()
    if(dependency_viewmodel_contents MATCHES "m_taskService\\.(listTasks|listDependencies)")
        record_violation("${dependency_viewmodel}"
            "Dependency editor may not reconstruct its business context from raw lists")
    endif()
endif()

set(graph_viewmodel "${ROOT_DIR}/src/viewmodel/TaskGraphViewModel.cpp")
if(EXISTS "${graph_viewmodel}")
    file(READ "${graph_viewmodel}" graph_viewmodel_contents)
    if(NOT graph_viewmodel_contents MATCHES "availability\\.canEditDependencies")
        record_violation("${graph_viewmodel}"
            "Graph dependency-edit eligibility must project Model availability")
    endif()
endif()

# 项目明确采用 MVVM，因此额外禁止重新引入 Controller 层或类型。
file(GLOB_RECURSE production_entries LIST_DIRECTORIES TRUE
    "${ROOT_DIR}/src/*")
foreach(entry IN LISTS production_entries)
    file(TO_CMAKE_PATH "${entry}" entry_path)
    string(TOLOWER "${entry_path}" entry_lower)
    if(entry_lower MATCHES "/controllers?/"
       OR entry_lower MATCHES "controller\\.(h|hpp|cpp|cc|cxx)$")
        record_violation("${entry}" "Controller layers and types are forbidden")
    endif()
endforeach()

file(GLOB_RECURSE production_cpp LIST_DIRECTORIES FALSE
    "${ROOT_DIR}/src/*.h"
    "${ROOT_DIR}/src/*.hpp"
    "${ROOT_DIR}/src/*.cpp"
    "${ROOT_DIR}/src/*.cc"
    "${ROOT_DIR}/src/*.cxx")
foreach(source_file IN LISTS production_cpp)
    file(READ "${source_file}" contents)
    string(TOLOWER "${contents}" contents_lower)
    if(contents_lower MATCHES "(class|struct)[ \t\r\n]+[a-z0-9_]*controller")
        record_violation("${source_file}" "Controller types are forbidden")
    endif()
    if(contents_lower MATCHES "(class|struct)[ \t\r\n]+[a-z0-9_]*(eventbus|servicelocator)")
        record_violation("${source_file}"
            "EventBus and Service Locator types are forbidden")
    endif()
    if(contents MATCHES "QML_[A-Z_]+|QQml[A-Za-z0-9_]*|QQuick[A-Za-z0-9_]*")
        record_violation("${source_file}"
            "Removed QML/Quick registration and runtime APIs may not remain in production code")
    endif()
endforeach()

file(GLOB_RECURSE removed_view_files LIST_DIRECTORIES FALSE
    "${ROOT_DIR}/src/*.qml"
    "${ROOT_DIR}/src/*.ui"
    "${ROOT_DIR}/tests/*.qml"
    "${ROOT_DIR}/tests/*.ui")
foreach(view_file IN LISTS removed_view_files)
    record_violation("${view_file}" "QML and Qt Designer .ui files are forbidden")
endforeach()

foreach(cmake_file IN ITEMS
        "${ROOT_DIR}/CMakeLists.txt"
        "${ROOT_DIR}/src/app/CMakeLists.txt"
        "${ROOT_DIR}/src/viewmodel/contracts/CMakeLists.txt"
        "${ROOT_DIR}/src/viewmodel/CMakeLists.txt"
        "${ROOT_DIR}/tests/CMakeLists.txt")
    if(EXISTS "${cmake_file}")
        file(READ "${cmake_file}" cmake_contents)
        string(TOLOWER "${cmake_contents}" cmake_lower)
        if(cmake_lower MATCHES "qt_add_qml_module|qt_import_qml_plugins|qt_extract_metatypes|smartmateqmlbaseline|smartmate_(ui|viewmodel_qml)|quicktest|qt6::(qml|quick)")
            record_violation("${cmake_file}"
                "Removed QML/Quick targets and build APIs may not remain")
        endif()
    endif()
endforeach()

file(GLOB_RECURSE widget_cpp LIST_DIRECTORIES FALSE
    "${ROOT_DIR}/src/view/widgets/*.h"
    "${ROOT_DIR}/src/view/widgets/*.cpp")
foreach(source_file IN LISTS widget_cpp)
    file(READ "${source_file}" widget_contents)
    string(TOLOWER "${widget_contents}" widget_lower)
    if(widget_lower MATCHES "qmetaobject::invokemethod|setproperty[ \t\r\n]*\\(")
        record_violation("${source_file}"
            "Widgets may not use string reflection for binding or commands")
    endif()
    if(widget_lower MATCHES "errormessage")
        record_violation("${source_file}"
            "Widgets must consume UiNotification instead of concrete ViewModel errorMessage compatibility APIs")
    endif()
endforeach()

foreach(stage4_widget IN ITEMS
        TaskCategoryDialog TaskCreationPredecessorDialog TaskDependencyDialog)
    set(stage4_source "${ROOT_DIR}/src/view/widgets/task/${stage4_widget}.cpp")
    if(NOT EXISTS "${stage4_source}")
        record_violation("${ROOT_DIR}/src/view/widgets/CMakeLists.txt"
            "${stage4_widget} is required by the category/dependency migration")
    endif()
endforeach()

set(category_widget "${ROOT_DIR}/src/view/widgets/task/TaskCategoryDialog.cpp")
if(EXISTS "${category_widget}")
    file(READ "${category_widget}" category_widget_contents)
    if(NOT category_widget_contents MATCHES "CategoryIdRole")
        record_violation("${category_widget}"
            "Category commands must use the stable CategoryIdRole")
    endif()
endif()

set(dependency_widget "${ROOT_DIR}/src/view/widgets/task/TaskDependencyDialog.cpp")
if(EXISTS "${dependency_widget}")
    file(READ "${dependency_widget}" dependency_widget_contents)
    if(NOT dependency_widget_contents MATCHES "TaskIdRole")
        record_violation("${dependency_widget}"
            "Dependency commands must use the stable TaskIdRole")
    endif()
    if(dependency_widget_contents MATCHES "for[ \t\r\n]*\\([^)]*\\)[ \t\r\n]*\\{[^}]*m_dependencies\\.save")
        record_violation("${dependency_widget}"
            "Dependency persistence must be one atomic Contract save, not a per-row loop")
    endif()
endif()

foreach(stage5_widget IN ITEMS DependencyGraphPage DependencyGraphView TaskGraphItems)
    set(stage5_source "${ROOT_DIR}/src/view/widgets/graph/${stage5_widget}.cpp")
    if(NOT EXISTS "${stage5_source}")
        record_violation("${ROOT_DIR}/src/view/widgets/CMakeLists.txt"
            "${stage5_widget} is required by the dependency graph migration")
    endif()
endforeach()

file(GLOB_RECURSE graph_widget_sources LIST_DIRECTORIES FALSE
    "${ROOT_DIR}/src/view/widgets/graph/*.h"
    "${ROOT_DIR}/src/view/widgets/graph/*.cpp")
foreach(graph_source IN LISTS graph_widget_sources)
    file(READ "${graph_source}" graph_contents)
    string(TOLOWER "${graph_contents}" graph_lower)
    if(graph_lower MATCHES "rolenames[ \t\r\n]*[(]")
        record_violation("${graph_source}"
            "Graph Widgets must use stable Contract roles instead of roleName reflection")
    endif()
    if(graph_lower MATCHES "adjacency|topological|predecessorclosure|successorclosure|depth[ _-]*first|breadth[ _-]*first")
        record_violation("${graph_source}"
            "Graph traversal, topology, and closure calculations must stay outside Widgets")
    endif()
endforeach()

set(graph_items "${ROOT_DIR}/src/view/widgets/graph/TaskGraphItems.cpp")
if(EXISTS "${graph_items}")
    file(READ "${graph_items}" graph_item_contents)
    if(NOT graph_item_contents MATCHES "EdgeRoutePointsRole"
       OR NOT graph_item_contents MATCHES "NodeXRole"
       OR NOT graph_item_contents MATCHES "NodeYRole")
        record_violation("${graph_items}"
            "Graph items must draw Contract-projected node coordinates and edge routes")
    endif()
endif()

get_property(violations GLOBAL PROPERTY MVVM_VIOLATIONS)
if(violations)
    string(REPLACE ";" "\n  - " formatted_violations "${violations}")
    message(FATAL_ERROR
        "MVVM boundary violations detected:\n  - ${formatted_violations}")
endif()

message(STATUS "MVVM boundary check passed")
