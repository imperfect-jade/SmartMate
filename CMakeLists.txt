cmake_minimum_required(VERSION 3.24)

project(SmartMate VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SMARTMATE_BUILD_QML_BASELINE
    "Build the temporary Qt Quick/QML behavior baseline before final cleanup"
    ON
)

set(smartmate_qt_components
    Core
    Gui
    Sql
    Test
    Widgets
)
if(SMARTMATE_BUILD_QML_BASELINE)
    set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")
    list(APPEND smartmate_qt_components
        Qml
        QmlIntegration
        Quick
        QuickControls2
        QuickTest
    )
endif()

find_package(Qt6 6.10.2 EXACT REQUIRED COMPONENTS
    ${smartmate_qt_components}
)

qt_standard_project_setup(REQUIRES 6.10)

include(CTest)

add_subdirectory(src/common)
add_subdirectory(src/model)
add_subdirectory(src/model/persistence)
add_subdirectory(src/viewmodel/contracts)
add_subdirectory(src/viewmodel)
if(SMARTMATE_BUILD_QML_BASELINE)
    add_subdirectory(src/viewmodel/qml)
    add_subdirectory(src/view)
endif()
add_subdirectory(src/view/widgets)
add_subdirectory(src/app)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
