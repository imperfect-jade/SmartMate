add_library(smartmate_model STATIC
    domain/Task.h
    domain/Task.cpp
    domain/TaskConstraints.h
    domain/TaskCreationRequest.h
    domain/TaskDependency.h
    domain/TaskGraph.h
    domain/TaskTypes.h
    dependencies/TaskDependencyGraph.h
    dependencies/TaskDependencyGraph.cpp
    planner/TaskOrderingPolicy.h
    planner/TaskOrderingPolicy.cpp
    repositories/RepositoryException.h
    repositories/ITaskCreationRepository.h
    repositories/ITaskDependencyRepository.h
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
