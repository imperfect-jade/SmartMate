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

qt_add_executable(SmartMate WIN32
    main_widgets.cpp
)

target_link_libraries(SmartMate
    PRIVATE
        smartmate_app_core
        smartmate_widgets
        Qt6::Widgets
)

if(SMARTMATE_BUILD_QML_BASELINE)
    qt_add_executable(SmartMateQmlBaseline WIN32
        main.cpp
        AppBootstrapperQml.cpp
    )

    target_link_libraries(SmartMateQmlBaseline
        PRIVATE
            smartmate_app_core
            smartmate_viewmodel_qml
            smartmate_viewmodel_qmlplugin
            smartmate_ui
            smartmate_uiplugin
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
    )

    qt_import_qml_plugins(SmartMateQmlBaseline)
endif()
