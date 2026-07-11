add_library(smartmate_model STATIC
    domain/AppearanceSettings.h
    domain/AppearanceSettings.cpp
    domain/Task.h
    domain/Task.cpp
    domain/TaskConstraints.h
    domain/TaskCreationRequest.h
    domain/TaskDependency.h
    domain/TaskGraph.h
    domain/TaskStateMachine.h
    domain/TaskStateMachine.cpp
    domain/TaskTypes.h
    dependencies/TaskDependencyGraph.h
    dependencies/TaskDependencyGraph.cpp
    planner/TaskOrderingPolicy.h
    planner/TaskOrderingPolicy.cpp
    repositories/RepositoryException.h
    repositories/IAppearanceSettingsRepository.h
    repositories/ITaskCreationRepository.h
    repositories/ITaskDependencyRepository.h
    repositories/ITaskRepository.h
    services/TaskService.h
    services/TaskService.cpp
    services/TaskResult.h
    services/AppearanceSettingsService.h
    services/AppearanceSettingsService.cpp
)

target_include_directories(smartmate_model
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(smartmate_model
    PUBLIC
        Qt6::Core
)
