set(kwindowsystem_plugin_SRCS
    plugin.cpp
)

add_library(KF6WaylandExtrasKWinPlugin OBJECT ${kwindowsystem_plugin_SRCS})
set_property(TARGET KF6WaylandExtrasKWinPlugin PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(KF6WaylandExtrasKWinPlugin PRIVATE QT_STATICPLUGIN)
target_link_libraries(KF6WaylandExtrasKWinPlugin kwin)
