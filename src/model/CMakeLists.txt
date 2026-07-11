add_library(smartmate_model STATIC
    domain/Task.h
    domain/Task.cpp
    domain/TaskConstraints.h
    domain/TaskTypes.h
    repositories/ITaskRepository.h
    services/TaskService.h
    services/TaskService.cpp
    services/TaskResult.h
)

target_include_directories(smartmate_model
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(smartmate_model
    PUBLIC
        Qt6::Core
)
