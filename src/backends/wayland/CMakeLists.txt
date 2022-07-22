target_sources(kwin PRIVATE
    logging.cpp
    scene_qpainter_wayland_backend.cpp
    wayland_backend.cpp
    wayland_output.cpp
)

if (HAVE_WAYLAND_EGL)
    target_sources(kwin PRIVATE egl_wayland_backend.cpp)
endif()

target_link_libraries(kwin KF5::WaylandClient)
if (HAVE_WAYLAND_EGL)
    target_link_libraries(kwin Wayland::Egl gbm::gbm)
endif()
