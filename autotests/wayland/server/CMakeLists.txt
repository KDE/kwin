if (Qt6_VERSION VERSION_GREATER_EQUAL "6.8.0")
    set(private_code_option  "PRIVATE_CODE")
endif()
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
qt6_generate_wayland_protocol_client_sources(testTabletInterface 
    ${private_code_option}
    FILES
        ${WaylandProtocols_DATADIR}/stable/tablet/tablet-v2.xml
    )
target_sources(testTabletInterface PRIVATE test_tablet_interface.cpp ${TABLET_SRCS})
target_link_libraries( testTabletInterface Qt::Test kwin Plasma::KWaylandClient Wayland::Client)
add_test(NAME kwayland-testTabletInterface COMMAND testTabletInterface)
ecm_mark_as_test(testTabletInterface)

########################################################
# Test DataControlInterface
########################################################
add_executable(testDataControlInterface test_datacontrol_interface.cpp ${DATACONTROL_SRCS})
qt6_generate_wayland_protocol_client_sources(testDataControlInterface
    ${private_code_option}
    FILES
        ${WaylandProtocols_DATADIR}/staging/ext-data-control/ext-data-control-v1.xml
)
target_sources(testDataControlInterface PRIVATE test_datacontrol_interface.cpp ${DATACONTROL_SRCS})
target_link_libraries( testDataControlInterface Qt::Test kwin Plasma::KWaylandClient Wayland::Client)
add_test(NAME kwayland-testDataControlInterface COMMAND testDataControlInterface)
ecm_mark_as_test(testDataControlInterface)

########################################################
# Test Keyboard Shortcuts Inhibitor Interface
########################################################
add_executable(testKeyboardShortcutsInhibitorInterface)
qt6_generate_wayland_protocol_client_sources(testKeyboardShortcutsInhibitorInterface
    ${private_code_option}
    FILES
        ${WaylandProtocols_DATADIR}/unstable/keyboard-shortcuts-inhibit/keyboard-shortcuts-inhibit-unstable-v1.xml
)
target_sources(testKeyboardShortcutsInhibitorInterface PRIVATE test_keyboard_shortcuts_inhibitor_interface.cpp ${KEYBOARD_SHORTCUTS_INHIBITOR_SRCS})
target_link_libraries(testKeyboardShortcutsInhibitorInterface Qt::Test kwin Plasma::KWaylandClient Wayland::Client)
add_test(NAME kwayland-testKeyboardShortcutsInhibitorInterface COMMAND testKeyboardShortcutsInhibitorInterface)
ecm_mark_as_test(testKeyboardShortcutsInhibitorInterface)

########################################################
# Test Viewporter Interface
########################################################
add_executable(testViewporterInterface)
qt6_generate_wayland_protocol_client_sources(testViewporterInterface
    ${private_code_option}
    FILES
        ${WaylandProtocols_DATADIR}/stable/viewporter/viewporter.xml
)
target_sources(testViewporterInterface PRIVATE test_viewporter_interface.cpp ${VIEWPORTER_SRCS})
target_link_libraries(testViewporterInterface Qt::Test kwin Plasma::KWaylandClient Wayland::Client)
add_test(NAME kwayland-testViewporterInterface COMMAND testViewporterInterface)
ecm_mark_as_test(testViewporterInterface)

########################################################
# Test ScreencastV1Interface
########################################################
add_executable(testScreencastV1Interface)
qt6_generate_wayland_protocol_client_sources(testScreencastV1Interface
    ${private_code_option}
    FILES
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/zkde-screencast-unstable-v1.xml
)
target_sources(testScreencastV1Interface PRIVATE test_screencast.cpp ${SCREENCAST_SRCS})
target_link_libraries(testScreencastV1Interface Qt::Test kwin Wayland::Client Plasma::KWaylandClient)
add_test(NAME kwayland-testScreencastV1Interface COMMAND testScreencastV1Interface)
ecm_mark_as_test(testScreencastV1Interface)

########################################################
# Test InputMethod Interface
########################################################
add_executable(testInputMethodInterface)
qt6_generate_wayland_protocol_client_sources(testInputMethodInterface
    NO_INCLUDE_CORE_ONLY
    FILES
        ${WaylandProtocols_DATADIR}/unstable/input-method/input-method-unstable-v1.xml
    ${private_code_option}
)
target_sources(testInputMethodInterface PRIVATE
    test_inputmethod_interface.cpp
    ${PROJECT_SOURCE_DIR}/tests/fakeoutput.cpp
    ${INPUTMETHOD_SRCS}
)
target_link_libraries(testInputMethodInterface Qt::Test kwin Plasma::KWaylandClient Wayland::Client)
add_test(NAME kwayland-testInputMethodInterface COMMAND testInputMethodInterface)
ecm_mark_as_test(testInputMethodInterface)

########################################################
# Test LayerShellV1 Interface
########################################################
add_executable(testLayerShellV1Interface)
qt6_generate_wayland_protocol_client_sources(testLayerShellV1Interface
    ${private_code_option}
    FILES
        ${PROJECT_SOURCE_DIR}/src/wayland/protocols/wlr-layer-shell-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/stable/xdg-shell/xdg-shell.xml
)
target_sources(testLayerShellV1Interface PRIVATE test_layershellv1_interface.cpp ${LAYERSHELLV1_SRCS})
target_link_libraries(testLayerShellV1Interface Qt::Test kwin Plasma::KWaylandClient Wayland::Client)
add_test(NAME kwayland-testLayerShellV1Interface COMMAND testLayerShellV1Interface)
ecm_mark_as_test(testLayerShellV1Interface)


########################################################
# Test TextInputV3 Interface
########################################################
add_executable(testTextInputV3Interface)
qt6_generate_wayland_protocol_client_sources(testTextInputV3Interface 
    ${private_code_option}
    FILES
        ${WaylandProtocols_DATADIR}/unstable/text-input/text-input-unstable-v3.xml
)
target_sources(testTextInputV3Interface PRIVATE test_textinputv3_interface.cpp ${TEXTINPUTV3_SRCS})
target_link_libraries(testTextInputV3Interface Qt::Test kwin Plasma::KWaylandClient Wayland::Client)
add_test(NAME kwayland-testTextInputV3Interface COMMAND testTextInputV3Interface)
ecm_mark_as_test(testTextInputV3Interface)

########################################################
# Test TextInputV1 Interface
########################################################
add_executable(testTextInputV1Interface)
qt6_generate_wayland_protocol_client_sources(testTextInputV1Interface
    ${private_code_option}
    FILES
        ${WaylandProtocols_DATADIR}/unstable/text-input/text-input-unstable-v1.xml
)
target_sources(testTextInputV1Interface PRIVATE test_textinputv1_interface.cpp ${TEXTINPUTV1_SRCS})
target_link_libraries(testTextInputV1Interface Qt::Test kwin Plasma::KWaylandClient Wayland::Client)
add_test(NAME kwayland-testTextInputV1Interface COMMAND testTextInputV1Interface)
ecm_mark_as_test(testTextInputV1Interface)
