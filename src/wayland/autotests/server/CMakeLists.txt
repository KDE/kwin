########################################################
# Test WaylandServerDisplay
########################################################
set( testWaylandServerDisplay_SRCS
        test_display.cpp
    )
add_executable(testWaylandServerDisplay ${testWaylandServerDisplay_SRCS})
target_link_libraries( testWaylandServerDisplay Qt5::Test Qt5::Gui KF5::WaylandServer Wayland::Server)
add_test(kwin-testWaylandServerDisplay testWaylandServerDisplay)
ecm_mark_as_test(testWaylandServerDisplay)

########################################################
# Test WaylandServerSeat
########################################################
set( testWaylandServerSeat_SRCS
        test_seat.cpp
    )
add_executable(testWaylandServerSeat ${testWaylandServerSeat_SRCS})
target_link_libraries( testWaylandServerSeat Qt5::Test Qt5::Gui KF5::WaylandServer Wayland::Server)
add_test(kwin-testWaylandServerSeat testWaylandServerSeat)
ecm_mark_as_test(testWaylandServerSeat)
