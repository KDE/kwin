set(idletime_plugin_SRCS
    poller.cpp
)

add_library(KF5IdleTimeKWinWaylandPrivatePlugin MODULE ${idletime_plugin_SRCS})
target_link_libraries(KF5IdleTimeKWinWaylandPrivatePlugin
    kwin
    KF5::IdleTime
    KF5::WaylandClient
)

install(
    TARGETS
        KF5IdleTimeKWinWaylandPrivatePlugin
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kf5/org.kde.kidletime.platforms/
)
