set(SERVER_LIB_SRCS
    buffer_interface.cpp
    compositor_interface.cpp
    display.cpp
    output_interface.cpp
    seat_interface.cpp
    shell_interface.cpp
    surface_interface.cpp
)

add_library(KF5WaylandServer ${SERVER_LIB_SRCS})
generate_export_header(KF5WaylandServer BASE_NAME KWaylandServer)
add_library(KF5::WaylandServer ALIAS KF5WaylandServer)

target_link_libraries(KF5WaylandServer
    PUBLIC Qt5::Gui
    PRIVATE Wayland::Server
)
