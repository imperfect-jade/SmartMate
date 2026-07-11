add_library(smartmate_persistence STATIC
    SqliteTaskRepository.h
    SqliteTaskRepository.cpp
)

target_link_libraries(smartmate_persistence
    # Repository抽象向调用方公开；Qt SQL仅属于具体适配器，不能渗透到Model或ViewModel。
    PUBLIC
        smartmate_model
    PRIVATE
        Qt6::Sql
)
