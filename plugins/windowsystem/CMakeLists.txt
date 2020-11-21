set(kwindowsystem_plugin_SRCS
    plugin.cpp
    windoweffects.cpp
    windowshadow.cpp
    windowsystem.cpp
)

add_library(KF5WindowSystemKWinPrivatePlugin OBJECT ${kwindowsystem_plugin_SRCS})
target_compile_definitions(KF5WindowSystemKWinPrivatePlugin PRIVATE QT_STATICPLUGIN)
target_link_libraries(KF5WindowSystemKWinPrivatePlugin kwin)
