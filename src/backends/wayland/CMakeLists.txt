target_sources(kwin PRIVATE
    wayland_backend.cpp wayland_backend.h
    wayland_display.cpp wayland_display.h
    wayland_egl_backend.cpp wayland_egl_backend.h
    wayland_logging.cpp wayland_logging.h
    wayland_output.cpp wayland_output.h
    wayland_qpainter_backend.cpp wayland_qpainter_backend.h
)

target_link_libraries(kwin KF5::WaylandClient Wayland::Client gbm::gbm)
