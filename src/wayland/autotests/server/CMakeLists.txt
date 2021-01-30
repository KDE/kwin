########################################################
# Test WaylandServerDisplay
########################################################
set( testWaylandServerDisplay_SRCS
        test_display.cpp
    )
add_executable(testWaylandServerDisplay ${testWaylandServerDisplay_SRCS})
target_link_libraries( testWaylandServerDisplay Qt::Test Qt::Gui Plasma::KWaylandServer Wayland::Server)
add_test(NAME kwayland-testWaylandServerDisplay COMMAND testWaylandServerDisplay)
ecm_mark_as_test(testWaylandServerDisplay)

########################################################
# Test WaylandServerSeat
########################################################
set( testWaylandServerSeat_SRCS
        test_seat.cpp
    )
add_executable(testWaylandServerSeat ${testWaylandServerSeat_SRCS})
target_link_libraries( testWaylandServerSeat Qt::Test Qt::Gui Plasma::KWaylandServer Wayland::Server)
add_test(NAME kwayland-testWaylandServerSeat COMMAND testWaylandServerSeat)
ecm_mark_as_test(testWaylandServerSeat)

########################################################
# Test No XDG_RUNTIME_DIR
########################################################
add_executable(testNoXdgRuntimeDir test_no_xdg_runtime_dir.cpp)
target_link_libraries( testNoXdgRuntimeDir Qt::Test Plasma::KWaylandServer)
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
target_link_libraries( testTabletInterface Qt::Test Plasma::KWaylandServer KF5::WaylandClient Wayland::Client)
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
target_link_libraries( testDataControlInterface Qt::Test Plasma::KWaylandServer KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testDataControlInterface COMMAND testDataControlInterface)
ecm_mark_as_test(testDataControlInterface)

########################################################
# Test Keyboard Shortcuts Inhibitor Interface
########################################################
ecm_add_qtwayland_client_protocol(KEYBOARD_SHORTCUTS_INHIBITOR_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/keyboard-shortcuts-inhibit/keyboard-shortcuts-inhibit-unstable-v1.xml
    BASENAME keyboard-shortcuts-inhibit-unstable-v1
    )
add_executable(testKeyboardShortcutsInhibitorInterface test_keyboard_shortcuts_inhibitor_interface.cpp ${KEYBOARD_SHORTCUTS_INHIBITOR_SRCS})
target_link_libraries(testKeyboardShortcutsInhibitorInterface Qt::Test Plasma::KWaylandServer KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testKeyboardShortcutsInhibitorInterface COMMAND testKeyboardShortcutsInhibitorInterface)
ecm_mark_as_test(testKeyboardShortcutsInhibitorInterface)

########################################################
# Test Viewporter Interface
########################################################
ecm_add_qtwayland_client_protocol(VIEWPORTER_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/stable/viewporter/viewporter.xml
    BASENAME viewporter
    )
add_executable(testViewporterInterface test_viewporter_interface.cpp ${VIEWPORTER_SRCS})
target_link_libraries(testViewporterInterface Qt::Test Plasma::KWaylandServer KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testViewporterInterface COMMAND testViewporterInterface)
ecm_mark_as_test(testViewporterInterface)

########################################################
# Test ScreencastV1Interface
########################################################
ecm_add_qtwayland_client_protocol(SCREENCAST_SRCS
    PROTOCOL PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/screencast.xml
    BASENAME zkde-screencast-unstable-v1
)
add_executable(testScreencastV1Interface test_screencast.cpp ${SCREENCAST_SRCS})
target_link_libraries(testScreencastV1Interface Qt::Test Plasma::KWaylandServer Wayland::Client KF5::WaylandClient)
add_test(NAME kwayland-testScreencastV1Interface COMMAND testScreencastV1Interface)
ecm_mark_as_test(testScreencastV1Interface)

########################################################
# Test InputMethod Interface
########################################################
ecm_add_qtwayland_client_protocol(INPUTMETHOD_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/input-method/input-method-unstable-v1.xml
    BASENAME input-method-unstable-v1
    )
ecm_add_qtwayland_client_protocol(VIEWPORTER_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/text-input/text-input-unstable-v1.xml
    BASENAME text-input-unstable-v1
    )
add_executable(testInputMethodInterface test_inputmethod_interface.cpp ${INPUTMETHOD_SRCS})
target_link_libraries(testInputMethodInterface Qt::Test Plasma::KWaylandServer KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testInputMethodInterface COMMAND testInputMethodInterface)
ecm_mark_as_test(testInputMethodInterface)

########################################################
# Test LayerShellV1 Interface
########################################################
ecm_add_qtwayland_client_protocol(LAYERSHELLV1_SRCS
    PROTOCOL ${PROJECT_SOURCE_DIR}/src/protocols/wlr-layer-shell-unstable-v1.xml
    BASENAME wlr-layer-shell-unstable-v1
)
ecm_add_qtwayland_client_protocol(LAYERSHELLV1_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/stable/xdg-shell/xdg-shell.xml
    BASENAME xdg-shell
)
add_executable(testLayerShellV1Interface test_layershellv1_interface.cpp ${LAYERSHELLV1_SRCS})
target_link_libraries(testLayerShellV1Interface Qt::Test Plasma::KWaylandServer KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testLayerShellV1Interface COMMAND testLayerShellV1Interface)
ecm_mark_as_test(testLayerShellV1Interface)


########################################################
# Test TextInputV3 Interface
########################################################
ecm_add_qtwayland_client_protocol(TEXTINPUTV3_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/text-input/text-input-unstable-v3.xml
    BASENAME text-input-unstable-v3
)
add_executable(testTextInputV3Interface test_textinputv3_interface.cpp ${TEXTINPUTV3_SRCS})
target_link_libraries(testTextInputV3Interface Qt::Test Plasma::KWaylandServer KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testTextInputV3Interface COMMAND testTextInputV3Interface)
ecm_mark_as_test(testTextInputV3Interface)
