########################################################
# Test WaylandServerDisplay
########################################################
set( testWaylandServerDisplay_SRCS
        test_display.cpp
    )
add_executable(testWaylandServerDisplay ${testWaylandServerDisplay_SRCS})
target_link_libraries( testWaylandServerDisplay Qt::Test Qt::Gui kwin Wayland::Server)
add_test(NAME kwayland-testWaylandServerDisplay COMMAND testWaylandServerDisplay)
ecm_mark_as_test(testWaylandServerDisplay)

########################################################
# Test WaylandServerSeat
########################################################
set( testWaylandServerSeat_SRCS
        test_seat.cpp
    )
add_executable(testWaylandServerSeat ${testWaylandServerSeat_SRCS})
target_link_libraries( testWaylandServerSeat Qt::Test Qt::Gui kwin Wayland::Server)
add_test(NAME kwayland-testWaylandServerSeat COMMAND testWaylandServerSeat)
ecm_mark_as_test(testWaylandServerSeat)

########################################################
# Test No XDG_RUNTIME_DIR
########################################################
add_executable(testNoXdgRuntimeDir test_no_xdg_runtime_dir.cpp)
target_link_libraries( testNoXdgRuntimeDir Qt::Test kwin)
add_test(NAME kwayland-testNoXdgRuntimeDir COMMAND testNoXdgRuntimeDir)
ecm_mark_as_test(testNoXdgRuntimeDir)

########################################################
# Test Tablet Interface
########################################################
add_executable(testTabletInterface)
if (QT_MAJOR_VERSION EQUAL "5")
    ecm_add_qtwayland_client_protocol(TABLET_SRCS
        PROTOCOL ${WaylandProtocols_DATADIR}/unstable/tablet/tablet-unstable-v2.xml
        BASENAME tablet-unstable-v2
    )
else()
    qt6_generate_wayland_protocol_client_sources(testTabletInterface FILES
        ${WaylandProtocols_DATADIR}/unstable/tablet/tablet-unstable-v2.xml)
endif()
target_sources(testTabletInterface PRIVATE test_tablet_interface.cpp ${TABLET_SRCS})
target_link_libraries( testTabletInterface Qt::Test kwin KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testTabletInterface COMMAND testTabletInterface)
ecm_mark_as_test(testTabletInterface)

########################################################
# Test DataControlInterface
########################################################
add_executable(testDataControlInterface test_datacontrol_interface.cpp ${DATACONTROL_SRCS})
if (QT_MAJOR_VERSION EQUAL "5")
ecm_add_qtwayland_client_protocol(DATACONTROL_SRCS
    PROTOCOL  ${PROJECT_SOURCE_DIR}/src/wayland/protocols/wlr-data-control-unstable-v1.xml
    BASENAME wlr-data-control-unstable-v1
)
else()
    qt6_generate_wayland_protocol_client_sources(testDataControlInterface FILES
        ${PROJECT_SOURCE_DIR}/src/wayland/protocols/wlr-data-control-unstable-v1.xml)
endif()
target_sources(testDataControlInterface PRIVATE test_datacontrol_interface.cpp ${DATACONTROL_SRCS})
target_link_libraries( testDataControlInterface Qt::Test kwin KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testDataControlInterface COMMAND testDataControlInterface)
ecm_mark_as_test(testDataControlInterface)

########################################################
# Test Keyboard Shortcuts Inhibitor Interface
########################################################
add_executable(testKeyboardShortcutsInhibitorInterface)
if (QT_MAJOR_VERSION EQUAL "5")
    ecm_add_qtwayland_client_protocol(KEYBOARD_SHORTCUTS_INHIBITOR_SRCS
        PROTOCOL ${WaylandProtocols_DATADIR}/unstable/keyboard-shortcuts-inhibit/keyboard-shortcuts-inhibit-unstable-v1.xml
        BASENAME keyboard-shortcuts-inhibit-unstable-v1
    )
else()
    qt6_generate_wayland_protocol_client_sources(testKeyboardShortcutsInhibitorInterface FILES
        ${WaylandProtocols_DATADIR}/unstable/keyboard-shortcuts-inhibit/keyboard-shortcuts-inhibit-unstable-v1.xml)
endif()
target_sources(testKeyboardShortcutsInhibitorInterface PRIVATE test_keyboard_shortcuts_inhibitor_interface.cpp ${KEYBOARD_SHORTCUTS_INHIBITOR_SRCS})
target_link_libraries(testKeyboardShortcutsInhibitorInterface Qt::Test kwin KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testKeyboardShortcutsInhibitorInterface COMMAND testKeyboardShortcutsInhibitorInterface)
ecm_mark_as_test(testKeyboardShortcutsInhibitorInterface)

########################################################
# Test Viewporter Interface
########################################################
add_executable(testViewporterInterface)
if (QT_MAJOR_VERSION EQUAL "5")
    ecm_add_qtwayland_client_protocol(VIEWPORTER_SRCS
        PROTOCOL ${WaylandProtocols_DATADIR}/stable/viewporter/viewporter.xml
        BASENAME viewporter
    )
else()
    qt6_generate_wayland_protocol_client_sources(testViewporterInterface FILES
        ${WaylandProtocols_DATADIR}/stable/viewporter/viewporter.xml)
endif()
target_sources(testViewporterInterface PRIVATE test_viewporter_interface.cpp ${VIEWPORTER_SRCS})
target_link_libraries(testViewporterInterface Qt::Test kwin KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testViewporterInterface COMMAND testViewporterInterface)
ecm_mark_as_test(testViewporterInterface)

########################################################
# Test ScreencastV1Interface
########################################################
add_executable(testScreencastV1Interface)
if (QT_MAJOR_VERSION EQUAL "5")
    ecm_add_qtwayland_client_protocol(SCREENCAST_SRCS
        PROTOCOL PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/zkde-screencast-unstable-v1.xml
        BASENAME zkde-screencast-unstable-v1
    )
else()
    qt6_generate_wayland_protocol_client_sources(testScreencastV1Interface FILES
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/zkde-screencast-unstable-v1.xml)
endif()
target_sources(testScreencastV1Interface PRIVATE test_screencast.cpp ${SCREENCAST_SRCS})
target_link_libraries(testScreencastV1Interface Qt::Test kwin Wayland::Client KF5::WaylandClient)
add_test(NAME kwayland-testScreencastV1Interface COMMAND testScreencastV1Interface)
ecm_mark_as_test(testScreencastV1Interface)

########################################################
# Test InputMethod Interface
########################################################
add_executable(testInputMethodInterface)
if (QT_MAJOR_VERSION EQUAL "5")
    ecm_add_qtwayland_client_protocol(INPUTMETHOD_SRCS
        PROTOCOL ${WaylandProtocols_DATADIR}/unstable/input-method/input-method-unstable-v1.xml
        BASENAME input-method-unstable-v1
    )
else()
    qt6_generate_wayland_protocol_client_sources(testInputMethodInterface
        NO_INCLUDE_CORE_ONLY
        FILES
            ${WaylandProtocols_DATADIR}/unstable/input-method/input-method-unstable-v1.xml
    )
endif()
target_sources(testInputMethodInterface PRIVATE test_inputmethod_interface.cpp ../../tests/fakeoutput.cpp ${INPUTMETHOD_SRCS})
target_link_libraries(testInputMethodInterface Qt::Test kwin KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testInputMethodInterface COMMAND testInputMethodInterface)
ecm_mark_as_test(testInputMethodInterface)

########################################################
# Test LayerShellV1 Interface
########################################################
add_executable(testLayerShellV1Interface)
if (QT_MAJOR_VERSION EQUAL "5")
    ecm_add_qtwayland_client_protocol(LAYERSHELLV1_SRCS
        PROTOCOL ${PROJECT_SOURCE_DIR}/src/wayland/protocols/wlr-layer-shell-unstable-v1.xml
        BASENAME wlr-layer-shell-unstable-v1
    )
    ecm_add_qtwayland_client_protocol(LAYERSHELLV1_SRCS
        PROTOCOL ${WaylandProtocols_DATADIR}/stable/xdg-shell/xdg-shell.xml
        BASENAME xdg-shell
    )
else()
    qt6_generate_wayland_protocol_client_sources(testLayerShellV1Interface FILES
        ${PROJECT_SOURCE_DIR}/src/wayland/protocols/wlr-layer-shell-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/stable/xdg-shell/xdg-shell.xml
    )
endif()
target_sources(testLayerShellV1Interface PRIVATE test_layershellv1_interface.cpp ${LAYERSHELLV1_SRCS})
target_link_libraries(testLayerShellV1Interface Qt::Test kwin KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testLayerShellV1Interface COMMAND testLayerShellV1Interface)
ecm_mark_as_test(testLayerShellV1Interface)


########################################################
# Test TextInputV3 Interface
########################################################
add_executable(testTextInputV3Interface)
if (QT_MAJOR_VERSION EQUAL "5")
    ecm_add_qtwayland_client_protocol(TEXTINPUTV3_SRCS
        PROTOCOL ${WaylandProtocols_DATADIR}/unstable/text-input/text-input-unstable-v3.xml
        BASENAME text-input-unstable-v3
    )
else()
    qt6_generate_wayland_protocol_client_sources(testTextInputV3Interface FILES
        ${WaylandProtocols_DATADIR}/unstable/text-input/text-input-unstable-v3.xml)
endif()
target_sources(testTextInputV3Interface PRIVATE test_textinputv3_interface.cpp ${TEXTINPUTV3_SRCS})
target_link_libraries(testTextInputV3Interface Qt::Test kwin KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testTextInputV3Interface COMMAND testTextInputV3Interface)
ecm_mark_as_test(testTextInputV3Interface)

########################################################
# Test TextInputV1 Interface
########################################################
add_executable(testTextInputV1Interface)
if (QT_MAJOR_VERSION EQUAL "5")
    ecm_add_qtwayland_client_protocol(TEXTINPUTV1_SRCS
        PROTOCOL ${WaylandProtocols_DATADIR}/unstable/text-input/text-input-unstable-v1.xml
        BASENAME text-input-unstable-v1
    )
else()
    qt6_generate_wayland_protocol_client_sources(testTextInputV1Interface FILES
        ${WaylandProtocols_DATADIR}/unstable/text-input/text-input-unstable-v1.xml)
endif()
target_sources(testTextInputV1Interface PRIVATE test_textinputv1_interface.cpp ${TEXTINPUTV1_SRCS})
target_link_libraries(testTextInputV1Interface Qt::Test kwin KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testTextInputV1Interface COMMAND testTextInputV1Interface)
ecm_mark_as_test(testTextInputV1Interface)
