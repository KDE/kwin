set(WAYLAND_BACKEND_SOURCES
    logging.cpp
    scene_qpainter_wayland_backend.cpp
    wayland_backend.cpp
)

if(HAVE_WAYLAND_EGL)
    set(WAYLAND_BACKEND_SOURCES egl_wayland_backend.cpp ${WAYLAND_BACKEND_SOURCES})
endif()

include_directories(${CMAKE_SOURCE_DIR}/platformsupport/scenes/opengl)

if(Vulkan_FOUND)
    set(WAYLAND_BACKEND_SOURCES ${WAYLAND_BACKEND_SOURCES} wayland_vulkan_backend.cpp)
endif()

add_library(KWinWaylandWaylandBackend MODULE ${WAYLAND_BACKEND_SOURCES})
target_link_libraries(KWinWaylandWaylandBackend kwin KF5::WaylandClient SceneQPainterBackend)

if(HAVE_WAYLAND_EGL)
    target_link_libraries(KWinWaylandWaylandBackend SceneOpenGLBackend Wayland::Egl)
endif()

if(Vulkan_FOUND)
    target_link_libraries(KWinWaylandWaylandBackend SceneVulkanBackend kwinvulkanutils)
endif()

install(
    TARGETS
        KWinWaylandWaylandBackend
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/org.kde.kwin.waylandbackends/
)
