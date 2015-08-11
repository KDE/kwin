set(WAYLAND_BACKEND_SOURCES
    logging.cpp
    scene_qpainter_wayland_backend.cpp
    screens_wayland.cpp
    wayland_backend.cpp
)
if(Wayland_Egl_FOUND)
    set(WAYLAND_BACKEND_SOURCES ${WAYLAND_BACKEND_SOURCES} egl_wayland_backend.cpp)
endif()

add_library(KWinWaylandWaylandBackend MODULE ${WAYLAND_BACKEND_SOURCES})
target_link_libraries(KWinWaylandWaylandBackend kwin KF5::WaylandClient)

if(Wayland_Egl_FOUND)
    target_link_libraries(KWinWaylandWaylandBackend Wayland::Egl)
endif()

install(
    TARGETS
        KWinWaylandWaylandBackend
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/org.kde.kwin.waylandbackends/
)
