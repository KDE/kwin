set(kwindowsystem_plugin_SRCS
    plugin.cpp
    windoweffects.cpp
    windowshadow.cpp
    windowsystem.cpp
)

add_library(KF6WindowSystemKWinPlugin OBJECT ${kwindowsystem_plugin_SRCS})
set_property(TARGET KF6WindowSystemKWinPlugin PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(KF6WindowSystemKWinPlugin PRIVATE QT_STATICPLUGIN)
target_link_libraries(KF6WindowSystemKWinPlugin kwin)
