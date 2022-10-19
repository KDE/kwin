set(idletime_plugin_SRCS
    poller.cpp
)

add_library(KF5IdleTimeKWinPlugin OBJECT ${idletime_plugin_SRCS})
set_property(TARGET KF5IdleTimeKWinPlugin PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(KF5IdleTimeKWinPlugin PRIVATE QT_STATICPLUGIN)
target_link_libraries(KF5IdleTimeKWinPlugin
    KF5::IdleTime
    kwin
)
