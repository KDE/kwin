set(idletime_plugin_SRCS
    poller.cpp
)

add_library(KF5IdleTimeKWinWaylandPrivatePlugin OBJECT ${idletime_plugin_SRCS})
target_compile_definitions(KF5IdleTimeKWinWaylandPrivatePlugin PRIVATE QT_STATICPLUGIN)
target_link_libraries(KF5IdleTimeKWinWaylandPrivatePlugin KF5::IdleTime KF5::WaylandClient kwin)
