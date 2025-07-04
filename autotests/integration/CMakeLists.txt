add_subdirectory(helper)

add_library(KWinIntegrationTestFramework STATIC)

if (Qt6_VERSION VERSION_GREATER_EQUAL "6.8.0")
    set(private_code_option  "PRIVATE_CODE")
endif()
qt6_generate_wayland_protocol_client_sources(KWinIntegrationTestFramework
    ${private_code_option}
    NO_INCLUDE_CORE_ONLY
    FILES
        ${WaylandProtocols_DATADIR}/unstable/input-method/input-method-unstable-v1.xml
)
qt6_generate_wayland_protocol_client_sources(KWinIntegrationTestFramework
    ${private_code_option}
    FILES
        ${CMAKE_SOURCE_DIR}/src/wayland/protocols/wlr-layer-shell-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/stable/presentation-time/presentation-time.xml
        ${WaylandProtocols_DATADIR}/stable/xdg-shell/xdg-shell.xml
        ${WaylandProtocols_DATADIR}/staging/color-management/color-management-v1.xml
        ${WaylandProtocols_DATADIR}/stable/tablet/tablet-v2.xml
        ${WaylandProtocols_DATADIR}/staging/cursor-shape/cursor-shape-v1.xml
        ${WaylandProtocols_DATADIR}/staging/fifo/fifo-v1.xml
        ${WaylandProtocols_DATADIR}/staging/fractional-scale/fractional-scale-v1.xml
        ${WaylandProtocols_DATADIR}/staging/security-context/security-context-v1.xml
        ${WaylandProtocols_DATADIR}/staging/xdg-dialog/xdg-dialog-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/text-input/text-input-unstable-v3.xml
        ${WaylandProtocols_DATADIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml

        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-output-device-v2.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-output-management-v2.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-screen-edge-v1.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/zkde-screencast-unstable-v1.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/fake-input.xml
)
target_sources(KWinIntegrationTestFramework PRIVATE
    generic_scene_opengl_test.cpp
    kwin_wayland_test.cpp
    test_helpers.cpp
)
target_link_libraries(KWinIntegrationTestFramework
    PUBLIC
        Qt::Test
        Qt::Concurrent
        Plasma::KWaylandClient
        Wayland::Client
        Libdrm::Libdrm
        kwin

    PRIVATE
        # Static plugins
        KWinQpaPlugin
        KF6WindowSystemKWinPlugin
        KF6IdleTimeKWinPlugin
)
if(KWIN_BUILD_X11)
    target_link_libraries(KWinIntegrationTestFramework PRIVATE KWinXwaylandServerModule)
endif()
if(TARGET KF6GlobalAccelKWinPlugin)
    target_link_libraries(KWinIntegrationTestFramework PUBLIC KF6GlobalAccelKWinPlugin)
endif()

if(TARGET PW::KScreenLocker)
    target_link_libraries(KWinIntegrationTestFramework PUBLIC PW::KScreenLocker)
endif()

function(integrationTest)
    set(optionArgs BUILTIN_EFFECTS)
    set(oneValueArgs NAME)
    set(multiValueArgs SRCS LIBS)
    cmake_parse_arguments(ARGS "${optionArgs}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if (NOT KWIN_BUILD_X11 AND ARGS_LIBS MATCHES XCB::)
        return()
    endif()
    add_executable(${ARGS_NAME} ${ARGS_SRCS})
    target_link_libraries(${ARGS_NAME} KWinIntegrationTestFramework Qt::Test ${ARGS_LIBS})
    if(${ARGS_BUILTIN_EFFECTS})
        kcoreaddons_target_static_plugins(${ARGS_NAME} NAMESPACE "kwin/effects/plugins")
    endif()
    add_test(NAME kwin-${ARGS_NAME} COMMAND dbus-run-session ${CMAKE_BINARY_DIR}/bin/${ARGS_NAME})
endfunction()
if(KWIN_BUILD_X11)
    integrationTest(NAME testDontCrashGlxgears SRCS dont_crash_glxgears.cpp LIBS KF6::I18n KDecoration3::KDecoration)
endif()
if (KWIN_BUILD_SCREENLOCKER)
    integrationTest(NAME testLockScreen SRCS lockscreen.cpp LIBS KF6::GlobalAccel)
endif()
integrationTest(NAME testBounceKeys SRCS bounce_keys_test.cpp)
integrationTest(NAME testButtonRebind SRCS buttonrebind_test.cpp)
integrationTest(NAME testDecorationInput SRCS decoration_input_test.cpp LIBS KDecoration3::KDecoration KDecoration3::KDecoration3Private)
integrationTest(NAME testInternalWindow SRCS internal_window.cpp)
integrationTest(NAME testTouchInput SRCS touch_input_test.cpp)
integrationTest(NAME testInputStackingOrder SRCS input_stacking_order.cpp)
integrationTest(NAME testPointerInput SRCS pointer_input.cpp LIBS Libdrm::Libdrm XCB::ICCCM)
integrationTest(NAME testPlatformCursor SRCS platformcursor.cpp)
integrationTest(NAME testDontCrashCancelAnimation SRCS dont_crash_cancel_animation.cpp LIBS KDecoration3::KDecoration)
integrationTest(NAME testTransientPlacement SRCS transient_placement.cpp)
integrationTest(NAME testDebugConsole SRCS debug_console_test.cpp)
integrationTest(NAME testPlasmaSurface SRCS plasma_surface_test.cpp)
integrationTest(NAME testMaximized SRCS maximize_test.cpp LIBS KDecoration3::KDecoration KF6::Package)
integrationTest(NAME testXdgShellWindow SRCS xdgshellwindow_test.cpp LIBS KDecoration3::KDecoration)
integrationTest(NAME testSceneOpenGL SRCS scene_opengl_test.cpp )
integrationTest(NAME testSceneOpenGLES SRCS scene_opengl_es_test.cpp )
integrationTest(NAME testScreenChanges SRCS screen_changes_test.cpp)
if (KWIN_BUILD_TABBOX)
    integrationTest(NAME testTabBox SRCS tabbox_test.cpp)
endif()
integrationTest(NAME testWindowSelection SRCS window_selection_test.cpp)
integrationTest(NAME testPointerConstraints SRCS pointer_constraints_test.cpp)
integrationTest(NAME testKeyboardLayout SRCS keyboard_layout_test.cpp LIBS KF6::GlobalAccel XKB::XKB)
integrationTest(NAME testKeymapCreationFailure SRCS keymap_creation_failure_test.cpp LIBS KF6::GlobalAccel)
integrationTest(NAME testShowingDesktop SRCS showing_desktop_test.cpp)
integrationTest(NAME testDontCrashUseractionsMenu SRCS dont_crash_useractions_menu.cpp LIBS KF6::I18n)
integrationTest(NAME testLayerShellV1Window SRCS layershellv1window_test.cpp)
integrationTest(NAME testVirtualDesktop SRCS virtual_desktop_test.cpp)
integrationTest(NAME testXdgShellWindowRules SRCS xdgshellwindow_rules_test.cpp)
integrationTest(NAME testIdleInhibition SRCS idle_inhibition_test.cpp)
integrationTest(NAME testDontCrashReinitializeCompositor SRCS dont_crash_reinitialize_compositor.cpp BUILTIN_EFFECTS)
integrationTest(NAME testNoGlobalShortcuts SRCS no_global_shortcuts_test.cpp LIBS KF6::GlobalAccel)
integrationTest(NAME testPlacement SRCS placement_test.cpp)
integrationTest(NAME testActivation SRCS activation_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testInputMethod SRCS inputmethod_test.cpp LIBS XKB::XKB)
integrationTest(NAME testScreens SRCS screens_test.cpp)
integrationTest(NAME testScreenEdges SRCS screenedges_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testOutputChanges SRCS outputchanges_test.cpp LIBS XCB::ICCCM Qt::Sensors)
integrationTest(NAME testTiles SRCS tiles_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testFractionalScaling SRCS fractional_scaling_test.cpp)
integrationTest(NAME testMoveResize SRCS move_resize_window_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testDontCrashAuroraeDestroyDeco SRCS dont_crash_aurorae_destroy_deco.cpp LIBS XCB::ICCCM KDecoration3::KDecoration)
integrationTest(NAME testPlasmaWindow SRCS plasmawindow_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testX11DesktopWindow SRCS desktop_window_x11_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testXwaylandInput SRCS xwayland_input_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testWindowRules SRCS window_rules_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testX11Window SRCS x11_window_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testQuickTiling SRCS quick_tiling_test.cpp LIBS XCB::ICCCM KDecoration3::KDecoration)
integrationTest(NAME testStackingOrder SRCS stacking_order_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testDbusInterface SRCS dbus_interface_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testXwaylandServerCrash SRCS xwaylandserver_crash_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testXwaylandServerRestart SRCS xwaylandserver_restart_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testFakeInput SRCS fakeinput_test.cpp)
integrationTest(NAME testSecurityContext SRCS security_context_test.cpp)
integrationTest(NAME testStickyKeys SRCS sticky_keys_test.cpp)
integrationTest(NAME testWorkspace SRCS workspace_test.cpp)
integrationTest(NAME testMouseActions SRCS mouseactions_test.cpp LIBS)
integrationTest(NAME testColorManagement SRCS test_colormanagement.cpp)
integrationTest(NAME testKeyboardInput SRCS keyboard_input_test.cpp)
integrationTest(NAME testFifo SRCS test_fifo.cpp)
integrationTest(NAME testMouseKeys SRCS mouse_keys_test.cpp)

if(KWIN_BUILD_X11)
    integrationTest(NAME testDontCrashEmptyDeco SRCS dont_crash_empty_deco.cpp LIBS KDecoration3::KDecoration)
    integrationTest(NAME testXwaylandSelections SRCS xwayland_selections_test.cpp)
    integrationTest(NAME testXinerama SRCS xinerama_test.cpp)
    integrationTest(NAME testX11KeyRead SRCS x11keyread.cpp LIBS XCB::XINPUT)
endif()

qt_add_dbus_interfaces(DBUS_SRCS ${CMAKE_BINARY_DIR}/src/org.kde.kwin.VirtualKeyboard.xml)
integrationTest(NAME testVirtualKeyboardDBus SRCS test_virtualkeyboard_dbus.cpp ${DBUS_SRCS})

if (KWIN_BUILD_GLOBALSHORTCUTS)
integrationTest(NAME testGlobalShortcuts SRCS globalshortcuts_test.cpp LIBS XCB::ICCCM KF6::GlobalAccel KF6::I18n XKB::XKB)
integrationTest(NAME testKWinBindings SRCS kwinbindings_test.cpp LIBS KF6::I18n)
endif()
if (TARGET K::KPipeWire)
    integrationTest(NAME testScreencasting SRCS screencasting_test.cpp LIBS K::KPipeWire)
endif()

if (KWIN_BUILD_ACTIVITIES)
    integrationTest(NAME testActivities SRCS activities_test.cpp LIBS XCB::ICCCM Plasma::Activities)
endif()

if (KWIN_BUILD_EIS)
    pkg_check_modules(PKG_libei REQUIRED IMPORTED_TARGET libei-1.0)
    integrationTest(NAME testInputCapture SRCS input_capture_test.cpp  LIBS PkgConfig::PKG_libei)
    if (PKG_libei_VERSION VERSION_GREATER_EQUAL "1.4.0")
        target_compile_definitions(testInputCapture PRIVATE -DHAVE_EI_EVENT_SYNC=1)
    else()
        target_compile_definitions(testInputCapture PRIVATE -DHAVE_EI_EVENT_SYNC=0)
    endif()
endif()


add_subdirectory(scripting)
add_subdirectory(effects)
add_subdirectory(fakes)
