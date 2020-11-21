set(idletime_plugin_SRCS
    poller.cpp
)

add_library(KF5IdleTimeKWinPlugin OBJECT ${idletime_plugin_SRCS})
target_compile_definitions(KF5IdleTimeKWinPlugin PRIVATE QT_STATICPLUGIN)
target_link_libraries(KF5IdleTimeKWinPlugin KF5::IdleTime KF5::WaylandClient kwin)
