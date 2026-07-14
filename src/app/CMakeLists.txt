# 应用核心只负责运行环境与对象组合，保持可被正式入口和启动测试复用。
add_library(smartmate_app_core STATIC
    AppBootstrapper.h
    AppBootstrapper.cpp
    ApplicationRuntime.h
    ApplicationRuntime.cpp
)

target_include_directories(smartmate_app_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(smartmate_app_core
    PUBLIC
        # app 是唯一组合根；该内部目标集中创建具体 Persistence、Service 与 ViewModel。
        smartmate_persistence
        smartmate_viewmodel
        Qt6::Core
)

# 正式且唯一的 Qt Widgets 可执行目标。
qt_add_executable(SmartMate WIN32
    main.cpp
)

target_link_libraries(SmartMate
    PRIVATE
        smartmate_app_core
        smartmate_widgets
        Qt6::Widgets
)
