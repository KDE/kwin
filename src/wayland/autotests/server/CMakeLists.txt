########################################################
# Test WaylandServerDisplay
########################################################
set( testWaylandServerDisplay_SRCS
        test_display.cpp
    )
add_executable(testWaylandServerDisplay ${testWaylandServerDisplay_SRCS})
target_link_libraries( testWaylandServerDisplay Qt5::Test Qt5::Gui Plasma::KWaylandServer Wayland::Server)
add_test(NAME kwayland-testWaylandServerDisplay COMMAND testWaylandServerDisplay)
ecm_mark_as_test(testWaylandServerDisplay)

########################################################
# Test WaylandServerSeat
########################################################
set( testWaylandServerSeat_SRCS
        test_seat.cpp
    )
add_executable(testWaylandServerSeat ${testWaylandServerSeat_SRCS})
target_link_libraries( testWaylandServerSeat Qt5::Test Qt5::Gui Plasma::KWaylandServer Wayland::Server)
add_test(NAME kwayland-testWaylandServerSeat COMMAND testWaylandServerSeat)
ecm_mark_as_test(testWaylandServerSeat)

########################################################
# Test No XDG_RUNTIME_DIR
########################################################
add_executable(testNoXdgRuntimeDir test_no_xdg_runtime_dir.cpp)
target_link_libraries( testNoXdgRuntimeDir Qt5::Test Plasma::KWaylandServer)
add_test(NAME kwayland-testNoXdgRuntimeDir COMMAND testNoXdgRuntimeDir)
ecm_mark_as_test(testNoXdgRuntimeDir)

########################################################
# Test Tablet Interface
########################################################
ecm_add_qtwayland_client_protocol(TABLET_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/tablet/tablet-unstable-v2.xml
    BASENAME tablet-unstable-v2
)
add_executable(testTabletInterface test_tablet_interface.cpp ${TABLET_SRCS})
target_link_libraries( testTabletInterface Qt5::Test Plasma::KWaylandServer KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testTabletInterface COMMAND testTabletInterface)
ecm_mark_as_test(testTabletInterface)

########################################################
# Test DataControlInterface
########################################################
ecm_add_qtwayland_client_protocol(DATACONTROL_SRCS
    PROTOCOL  ${PROJECT_SOURCE_DIR}/src/protocols/wlr-data-control-unstable-v1.xml
    BASENAME wlr-data-control-unstable-v1
)
add_executable(testDataControlInterface test_datacontrol_interface.cpp ${DATACONTROL_SRCS})
target_link_libraries( testDataControlInterface Qt5::Test Plasma::KWaylandServer KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testDataControlInterface COMMAND testDataControlInterface)
ecm_mark_as_test(testDataControlInterface)
