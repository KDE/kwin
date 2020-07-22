set(WAYLAND_BACKEND_SOURCES
    logging.cpp
    scene_qpainter_wayland_backend.cpp
    wayland_backend.cpp
    wayland_output.cpp
    ../drm/gbm_dmabuf.cpp
)

if (HAVE_WAYLAND_EGL)
    set(WAYLAND_BACKEND_SOURCES egl_wayland_backend.cpp ${WAYLAND_BACKEND_SOURCES})
endif()

include_directories(${CMAKE_SOURCE_DIR}/platformsupport/scenes/opengl)
add_library(KWinWaylandWaylandBackend MODULE ${WAYLAND_BACKEND_SOURCES})
set_target_properties(KWinWaylandWaylandBackend PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/org.kde.kwin.waylandbackends/")
target_link_libraries(KWinWaylandWaylandBackend kwin KF5::WaylandClient SceneQPainterBackend)

if (HAVE_WAYLAND_EGL)
    target_link_libraries(KWinWaylandWaylandBackend SceneOpenGLBackend Wayland::Egl gbm::gbm)
endif()

install(
    TARGETS
        KWinWaylandWaylandBackend
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/org.kde.kwin.waylandbackends/
)
