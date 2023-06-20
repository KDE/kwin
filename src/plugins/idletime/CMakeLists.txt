set(idletime_plugin_SRCS
    poller.cpp
)

add_library(KF6IdleTimeKWinPlugin OBJECT ${idletime_plugin_SRCS})
target_compile_definitions(KF6IdleTimeKWinPlugin PRIVATE QT_STATICPLUGIN)
target_link_libraries(KF6IdleTimeKWinPlugin
    KF6::IdleTime
    kwin
)
