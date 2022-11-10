target_sources(kwin PRIVATE
    wayland_backend.cpp
    wayland_display.cpp
    wayland_logging.cpp
    wayland_output.cpp
    wayland_qpainter_backend.cpp
)

if (HAVE_WAYLAND_EGL)
    target_sources(kwin PRIVATE wayland_egl_backend.cpp)
endif()

target_link_libraries(kwin KF5::WaylandClient)
if (HAVE_WAYLAND_EGL)
    target_link_libraries(kwin Wayland::Egl gbm::gbm)
endif()
