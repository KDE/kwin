set(kwindowsystem_plugin_SRCS
    plugin.cpp
    windoweffects.cpp
    windowshadow.cpp
    windowsystem.cpp
)

add_library(KF5WindowSystemKWinPlugin OBJECT ${kwindowsystem_plugin_SRCS})
set_property(TARGET KF5WindowSystemKWinPlugin PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(KF5WindowSystemKWinPlugin PRIVATE QT_STATICPLUGIN)
target_link_libraries(KF5WindowSystemKWinPlugin kwin)
