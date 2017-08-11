set(WAYLAND_BACKEND_SOURCES
    logging.cpp
    scene_qpainter_wayland_backend.cpp
    wayland_backend.cpp
)

if(HAVE_WAYLAND_EGL)
    set(WAYLAND_BACKEND_SOURCES egl_wayland_backend.cpp ${WAYLAND_BACKEND_SOURCES})
endif()

add_library(KWinWaylandWaylandBackend MODULE ${WAYLAND_BACKEND_SOURCES})
target_link_libraries(KWinWaylandWaylandBackend kwin KF5::WaylandClient SceneQPainterBackend)

if(HAVE_WAYLAND_EGL)
    target_link_libraries(KWinWaylandWaylandBackend Wayland::Egl)
endif()

install(
    TARGETS
        KWinWaylandWaylandBackend
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/org.kde.kwin.waylandbackends/
)
