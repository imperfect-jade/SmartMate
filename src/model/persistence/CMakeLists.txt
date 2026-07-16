add_library(smartmate_persistence STATIC
    TaskSqlCodec.h
    TaskSqlCodec.cpp
    internal/SqliteTaskRepositorySupport.h
    internal/SqliteTaskRepositorySupport.cpp
    SqliteTaskRepository.h
    SqliteTaskRepository.cpp
    SqliteTaskRepositoryTasks.cpp
    SqliteTaskRepositoryDependencies.cpp
    SqliteTaskRepositoryAtomicCommands.cpp
    SqliteTaskRepositoryCategories.cpp
    SqliteTaskRepositoryEvents.cpp
    QSettingsAppearanceRepository.h
    QSettingsAppearanceRepository.cpp
    QSettingsDesktopPetRepository.h
    QSettingsDesktopPetRepository.cpp
)

target_link_libraries(smartmate_persistence
    # Repository抽象向调用方公开；Qt SQL仅属于具体适配器，不能渗透到Model或ViewModel。
    PUBLIC
        smartmate_model
    PRIVATE
        Qt6::Sql
)
