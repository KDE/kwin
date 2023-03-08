add_subdirectory(helper)

add_library(KWinIntegrationTestFramework SHARED)

qt6_generate_wayland_protocol_client_sources(KWinIntegrationTestFramework
    NO_INCLUDE_CORE_ONLY
    FILES
        ${WaylandProtocols_DATADIR}/unstable/input-method/input-method-unstable-v1.xml
)
qt6_generate_wayland_protocol_client_sources(KWinIntegrationTestFramework
    FILES
        ${WaylandProtocols_DATADIR}/unstable/text-input/text-input-unstable-v3.xml
        ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/stable/xdg-shell/xdg-shell.xml
        ${WaylandProtocols_DATADIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/staging/fractional-scale/fractional-scale-v1.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-output-device-v2.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-output-management-v2.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-screen-edge-v1.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/zkde-screencast-unstable-v1.xml
)

target_sources(KWinIntegrationTestFramework PRIVATE
    ../../src/cursor.cpp

    generic_scene_opengl_test.cpp
    kwin_wayland_test.cpp
    test_helpers.cpp
)
target_link_libraries(KWinIntegrationTestFramework
    PUBLIC
        Qt::Test
        KF6::WaylandClient
        Wayland::Client
        kwin

    PRIVATE
        # Own libraries
        KWinXwaylandServerModule

        # Static plugins
        KWinQpaPlugin
        KF6GlobalAccelKWinPlugin
        KF6WindowSystemKWinPlugin
        KF6IdleTimeKWinPlugin
)
kcoreaddons_target_static_plugins(KWinIntegrationTestFramework "kwin/effects/plugins" LINK_OPTION "PRIVATE")
set_target_properties(KWinIntegrationTestFramework PROPERTIES CXX_VISIBILITY_PRESET default)

function(integrationTest)
    set(optionArgs)
    set(oneValueArgs NAME)
    set(multiValueArgs SRCS LIBS)
    cmake_parse_arguments(ARGS "${optionArgs}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    add_executable(${ARGS_NAME} ${ARGS_SRCS})
    target_link_libraries(${ARGS_NAME} KWinIntegrationTestFramework Qt::Test ${ARGS_LIBS})
    add_test(NAME kwin-${ARGS_NAME} COMMAND dbus-run-session ${CMAKE_BINARY_DIR}/bin/${ARGS_NAME})
endfunction()

integrationTest(NAME testDontCrashGlxgears SRCS dont_crash_glxgears.cpp)
if (KWIN_BUILD_SCREENLOCKER)
    integrationTest(NAME testLockScreen SRCS lockscreen.cpp)
endif()
integrationTest(NAME testDecorationInput SRCS decoration_input_test.cpp)
integrationTest(NAME testInternalWindow SRCS internal_window.cpp)
integrationTest(NAME testTouchInput SRCS touch_input_test.cpp)
integrationTest(NAME testInputStackingOrder SRCS input_stacking_order.cpp)
integrationTest(NAME testPointerInput SRCS pointer_input.cpp)
integrationTest(NAME testPlatformCursor SRCS platformcursor.cpp)
integrationTest(NAME testDontCrashCancelAnimation SRCS dont_crash_cancel_animation.cpp)
integrationTest(NAME testTransientPlacement SRCS transient_placement.cpp)
integrationTest(NAME testDebugConsole SRCS debug_console_test.cpp)
integrationTest(NAME testDontCrashEmptyDeco SRCS dont_crash_empty_deco.cpp)
integrationTest(NAME testPlasmaSurface SRCS plasma_surface_test.cpp)
integrationTest(NAME testMaximized SRCS maximize_test.cpp)
integrationTest(NAME testXdgShellWindow SRCS xdgshellwindow_test.cpp)
integrationTest(NAME testXwaylandSelections SRCS xwayland_selections_test.cpp)
integrationTest(NAME testSceneOpenGL SRCS scene_opengl_test.cpp )
integrationTest(NAME testSceneOpenGLES SRCS scene_opengl_es_test.cpp )
integrationTest(NAME testScreenChanges SRCS screen_changes_test.cpp)
integrationTest(NAME testModiferOnlyShortcut SRCS modifier_only_shortcut_test.cpp)
if (KWIN_BUILD_TABBOX)
    integrationTest(NAME testTabBox SRCS tabbox_test.cpp)
endif()
integrationTest(NAME testWindowSelection SRCS window_selection_test.cpp)
integrationTest(NAME testPointerConstraints SRCS pointer_constraints_test.cpp)
integrationTest(NAME testKeyboardLayout SRCS keyboard_layout_test.cpp)
integrationTest(NAME testKeymapCreationFailure SRCS keymap_creation_failure_test.cpp)
integrationTest(NAME testShowingDesktop SRCS showing_desktop_test.cpp)
integrationTest(NAME testDontCrashUseractionsMenu SRCS dont_crash_useractions_menu.cpp)
integrationTest(NAME testKWinBindings SRCS kwinbindings_test.cpp)
integrationTest(NAME testLayerShellV1Window SRCS layershellv1window_test.cpp)
integrationTest(NAME testVirtualDesktop SRCS virtual_desktop_test.cpp)
integrationTest(NAME testXdgShellWindowRules SRCS xdgshellwindow_rules_test.cpp)
integrationTest(NAME testIdleInhibition SRCS idle_inhibition_test.cpp)
integrationTest(NAME testDontCrashReinitializeCompositor SRCS dont_crash_reinitialize_compositor.cpp)
integrationTest(NAME testNoGlobalShortcuts SRCS no_global_shortcuts_test.cpp)
integrationTest(NAME testPlacement SRCS placement_test.cpp)
integrationTest(NAME testActivation SRCS activation_test.cpp)
integrationTest(NAME testInputMethod SRCS inputmethod_test.cpp)
integrationTest(NAME testScreens SRCS screens_test.cpp)
integrationTest(NAME testScreenEdges SRCS screenedges_test.cpp)
integrationTest(NAME testOutputChanges SRCS outputchanges_test.cpp)
integrationTest(NAME testTiles SRCS tiles_test.cpp)
integrationTest(NAME testFractionalScaling SRCS fractional_scaling_test.cpp)
integrationTest(NAME testMoveResize SRCS move_resize_window_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testStruts SRCS struts_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testShade SRCS shade_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testDontCrashAuroraeDestroyDeco SRCS dont_crash_aurorae_destroy_deco.cpp LIBS XCB::ICCCM)
integrationTest(NAME testPlasmaWindow SRCS plasmawindow_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testX11DesktopWindow SRCS desktop_window_x11_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testXwaylandInput SRCS xwayland_input_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testWindowRules SRCS window_rules_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testX11Window SRCS x11_window_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testQuickTiling SRCS quick_tiling_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testGlobalShortcuts SRCS globalshortcuts_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testStackingOrder SRCS stacking_order_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testDbusInterface SRCS dbus_interface_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testXwaylandServerCrash SRCS xwaylandserver_crash_test.cpp LIBS XCB::ICCCM)
integrationTest(NAME testXwaylandServerRestart SRCS xwaylandserver_restart_test.cpp LIBS XCB::ICCCM)

qt_add_dbus_interfaces(DBUS_SRCS ${CMAKE_BINARY_DIR}/src/org.kde.kwin.VirtualKeyboard.xml)
integrationTest(NAME testVirtualKeyboardDBus SRCS test_virtualkeyboard_dbus.cpp ${DBUS_SRCS})

if (TARGET K::KPipeWire)
    integrationTest(NAME testScreencasting SRCS screencasting_test.cpp LIBS K::KPipeWire)
endif()

if (KWIN_BUILD_ACTIVITIES)
    integrationTest(NAME testActivities SRCS activities_test.cpp LIBS XCB::ICCCM)
endif()

add_subdirectory(scripting)
add_subdirectory(effects)
add_subdirectory(fakes)
