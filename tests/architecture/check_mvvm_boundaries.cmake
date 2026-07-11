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

scan_includes("${ROOT_DIR}/src/viewmodel" "ViewModel"
    "qtquick" "qquick" "qqmlengine" "qqmlcontext" "qtsql" "qsql"
    "model/persistence" "view/")

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
endif()

set(view_cmake "${ROOT_DIR}/src/view/CMakeLists.txt")
if(EXISTS "${view_cmake}")
    file(READ "${view_cmake}" view_cmake_contents)
    string(TOLOWER "${view_cmake_contents}" view_cmake_lower)
    if(view_cmake_lower MATCHES "smartmate_persistence|qt6::sql")
        record_violation("${view_cmake}"
            "smartmate_ui may not link concrete persistence or Qt SQL")
    endif()
endif()

file(GLOB_RECURSE qml_files LIST_DIRECTORIES FALSE
    "${ROOT_DIR}/src/view/*.qml")
# View 只能绑定 ViewModel；这些检查阻止 QML 直接接触 Model、Service 或 SQL。
foreach(qml_file IN LISTS qml_files)
    file(READ "${qml_file}" qml_contents)
    string(TOLOWER "${qml_contents}" qml_lower)

    if(qml_lower MATCHES "import[ \t]+(smartmate\\.(model|persistence)|qtsql|qtquick\\.localstorage|qt\\.labs\\.settings)")
        record_violation("${qml_file}"
            "View imports a Model, persistence, SQL, or settings module")
    endif()
    if(qml_lower MATCHES "(service|repository|planningengine)[a-z0-9_]*[ \t]*\\.")
        record_violation("${qml_file}"
            "View appears to call a Service, Repository, or PlanningEngine directly")
    endif()
    if(qml_lower MATCHES "qsql[a-z0-9_]*")
        record_violation("${qml_file}"
            "View contains a Qt SQL type")
    endif()
    if(qml_lower MATCHES "\\.(filter|sort)[ \t\r\n]*\\(")
        record_violation("${qml_file}"
            "View contains JavaScript list filtering or sorting logic")
    endif()
endforeach()

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
endforeach()

get_property(violations GLOBAL PROPERTY MVVM_VIOLATIONS)
if(violations)
    string(REPLACE ";" "\n  - " formatted_violations "${violations}")
    message(FATAL_ERROR
        "MVVM boundary violations detected:\n  - ${formatted_violations}")
endif()

message(STATUS "MVVM boundary check passed")
