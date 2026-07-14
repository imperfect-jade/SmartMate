cmake_minimum_required(VERSION 3.24)

project(SmartMate VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.10.2 EXACT REQUIRED COMPONENTS
    Core
    Gui
    Sql
    Test
    Widgets
)

qt_standard_project_setup(REQUIRES 6.10)

include(CTest)

add_subdirectory(src/common)
add_subdirectory(src/model)
add_subdirectory(src/model/persistence)
add_subdirectory(src/viewmodel/contracts)
add_subdirectory(src/viewmodel)
add_subdirectory(src/view/widgets)
add_subdirectory(src/app)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
