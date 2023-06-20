set(kwindowsystem_plugin_SRCS
    plugin.cpp
    windoweffects.cpp
    windowshadow.cpp
    windowsystem.cpp
)

add_library(KF6WindowSystemKWinPlugin OBJECT ${kwindowsystem_plugin_SRCS})
target_compile_definitions(KF6WindowSystemKWinPlugin PRIVATE QT_STATICPLUGIN)
target_link_libraries(KF6WindowSystemKWinPlugin kwin)
