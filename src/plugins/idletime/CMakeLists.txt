set(idletime_plugin_SRCS
    poller.cpp
)

add_library(KF6IdleTimeKWinPlugin OBJECT ${idletime_plugin_SRCS})
set_property(TARGET KF6IdleTimeKWinPlugin PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(KF6IdleTimeKWinPlugin PRIVATE QT_STATICPLUGIN)
target_link_libraries(KF6IdleTimeKWinPlugin
    KF6::IdleTime
    kwin
)
