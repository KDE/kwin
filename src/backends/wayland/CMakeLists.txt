target_sources(kwin PRIVATE
    wayland_backend.cpp
    wayland_display.cpp
    wayland_egl_backend.cpp
    wayland_logging.cpp
    wayland_output.cpp
    wayland_qpainter_backend.cpp
    wayland_vulkan_backend.cpp
)

target_link_libraries(kwin PRIVATE Plasma::KWaylandClient Wayland::Client gbm::gbm)
