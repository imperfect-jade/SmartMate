cmake_minimum_required(VERSION 3.24)

project(SmartMate VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")
find_package(Qt6 6.10.2 EXACT REQUIRED COMPONENTS
    Core
    Gui
    Qml
    QmlIntegration
    Quick
    QuickControls2
    QuickTest
    Sql
    Test
)

qt_standard_project_setup(REQUIRES 6.10)

include(CTest)

add_subdirectory(src/model)
add_subdirectory(src/model/persistence)
add_subdirectory(src/viewmodel)
add_subdirectory(src/view)
add_subdirectory(src/app)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
