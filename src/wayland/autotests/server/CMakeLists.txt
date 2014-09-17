########################################################
# Test WaylandServerDisplay
########################################################
set( testWaylandServerDisplay_SRCS
        test_display.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/buffer_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/compositor_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/display.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/output_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/seat_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/shell_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/surface_interface.cpp
    )
add_executable(testWaylandServerDisplay ${testWaylandServerDisplay_SRCS})
target_link_libraries( testWaylandServerDisplay Qt5::Test Qt5::Gui Wayland::Server)
add_test(kwin-testWaylandServerDisplay testWaylandServerDisplay)
ecm_mark_as_test(testWaylandServerDisplay)

########################################################
# Test WaylandServerSeat
########################################################
set( testWaylandServerSeat_SRCS
        test_seat.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/buffer_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/compositor_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/display.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/output_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/seat_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/shell_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/surface_interface.cpp
    )
add_executable(testWaylandServerSeat ${testWaylandServerSeat_SRCS})
target_link_libraries( testWaylandServerSeat Qt5::Test Qt5::Gui Wayland::Server)
add_test(kwin-testWaylandServerSeat testWaylandServerSeat)
ecm_mark_as_test(testWaylandServerSeat)
