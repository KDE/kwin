set(idletime_plugin_SRCS
    poller.cpp
)

add_library(KF5IdleTimeKWinWaylandPrivatePlugin MODULE ${idletime_plugin_SRCS})
set_target_properties(KF5IdleTimeKWinWaylandPrivatePlugin PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/kf5/org.kde.kidletime.platforms/")
target_link_libraries(KF5IdleTimeKWinWaylandPrivatePlugin
    KF5::IdleTime
    KF5::WaylandClient
    kwin
)

install(
    TARGETS
        KF5IdleTimeKWinWaylandPrivatePlugin
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kf5/org.kde.kidletime.platforms/
)
