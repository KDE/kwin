set(WAYLAND_BACKEND_SOURCES
    logging.cpp
    scene_qpainter_wayland_backend.cpp
    wayland_backend.cpp
    egl_wayland_backend.cpp
)

add_library(KWinWaylandWaylandBackend MODULE ${WAYLAND_BACKEND_SOURCES})
target_link_libraries(KWinWaylandWaylandBackend kwin KF5::WaylandClient Wayland::Egl)

install(
    TARGETS
        KWinWaylandWaylandBackend
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/org.kde.kwin.waylandbackends/
)
