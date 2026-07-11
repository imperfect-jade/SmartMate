qt_add_executable(SmartMate WIN32
    main.cpp
    AppBootstrapper.h
    AppBootstrapper.cpp
)

target_link_libraries(SmartMate
    PRIVATE
        # App是唯一组合根，因此只有可执行目标链接具体Persistence和各层模块。
        smartmate_model
        smartmate_persistence
        smartmate_viewmodel
        smartmate_viewmodelplugin
        smartmate_ui
        smartmate_uiplugin
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
)

qt_import_qml_plugins(SmartMate)
