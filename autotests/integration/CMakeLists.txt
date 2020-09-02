add_subdirectory(helper)

set(KWinIntegrationTestFramework_SOURCES
    ../../cursor.cpp

    generic_scene_opengl_test.cpp
    kwin_wayland_test.cpp
    test_helpers.cpp

    ${kwin_XWAYLAND_SRCS}
)
ecm_add_qtwayland_client_protocol(KWinIntegrationTestFramework_SOURCES
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/input-method/input-method-unstable-v1.xml
    BASENAME input-method-unstable-v1
)
ecm_add_qtwayland_client_protocol(KWinIntegrationTestFramework_SOURCES
    PROTOCOL protocols/wlr-layer-shell-unstable-v1.xml
    BASENAME wlr-layer-shell-unstable-v1
)
ecm_add_qtwayland_client_protocol(KWinIntegrationTestFramework_SOURCES
    PROTOCOL ${WaylandProtocols_DATADIR}/stable/xdg-shell/xdg-shell.xml
    BASENAME xdg-shell
)
add_library(KWinIntegrationTestFramework STATIC ${KWinIntegrationTestFramework_SOURCES})
target_link_libraries(KWinIntegrationTestFramework kwin Qt5::Test Wayland::Client)

function(integrationTest)
    set(optionArgs WAYLAND_ONLY)
    set(oneValueArgs NAME)
    set(multiValueArgs SRCS LIBS)
    cmake_parse_arguments(ARGS "${optionArgs}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    add_executable(${ARGS_NAME} ${ARGS_SRCS})
    target_link_libraries(${ARGS_NAME} KWinIntegrationTestFramework kwin Qt5::Test ${ARGS_LIBS})
    add_test(NAME kwin-${ARGS_NAME} COMMAND dbus-run-session ${CMAKE_BINARY_DIR}/bin/${ARGS_NAME})
    if (${ARGS_WAYLAND_ONLY})
        add_executable(${ARGS_NAME}_waylandonly ${ARGS_SRCS} )
        set_target_properties(${ARGS_NAME}_waylandonly PROPERTIES COMPILE_DEFINITIONS "NO_XWAYLAND")
        target_link_libraries(${ARGS_NAME}_waylandonly KWinIntegrationTestFramework kwin Qt5::Test ${ARGS_LIBS})
        add_test(NAME kwin-${ARGS_NAME}-waylandonly COMMAND dbus-run-session ${CMAKE_BINARY_DIR}/bin/${ARGS_NAME}_waylandonly)
    endif()
endfunction()

integrationTest(NAME testDontCrashGlxgears SRCS dont_crash_glxgears.cpp)
integrationTest(NAME testLockScreen SRCS lockscreen.cpp)
integrationTest(WAYLAND_ONLY NAME testDecorationInput SRCS decoration_input_test.cpp)
integrationTest(WAYLAND_ONLY NAME testInternalWindow SRCS internal_window.cpp)
integrationTest(WAYLAND_ONLY NAME testTouchInput SRCS touch_input_test.cpp)
integrationTest(WAYLAND_ONLY NAME testInputStackingOrder SRCS input_stacking_order.cpp)
integrationTest(NAME testPointerInput SRCS pointer_input.cpp)
integrationTest(NAME testPlatformCursor SRCS platformcursor.cpp)
integrationTest(WAYLAND_ONLY NAME testDontCrashCancelAnimation SRCS dont_crash_cancel_animation.cpp)
integrationTest(WAYLAND_ONLY NAME testTransientPlacement SRCS transient_placement.cpp)
integrationTest(NAME testDebugConsole SRCS debug_console_test.cpp)
integrationTest(NAME testDontCrashEmptyDeco SRCS dont_crash_empty_deco.cpp)
integrationTest(WAYLAND_ONLY NAME testPlasmaSurface SRCS plasma_surface_test.cpp)
integrationTest(WAYLAND_ONLY NAME testMaximized SRCS maximize_test.cpp)
integrationTest(WAYLAND_ONLY NAME testXdgShellClient SRCS xdgshellclient_test.cpp)
integrationTest(WAYLAND_ONLY NAME testDontCrashNoBorder SRCS dont_crash_no_border.cpp)
integrationTest(NAME testXwaylandSelections SRCS xwayland_selections_test.cpp)
integrationTest(WAYLAND_ONLY NAME testSceneOpenGL SRCS scene_opengl_test.cpp )
integrationTest(WAYLAND_ONLY NAME testSceneOpenGLShadow SRCS scene_opengl_shadow_test.cpp)
integrationTest(WAYLAND_ONLY NAME testSceneOpenGLES SRCS scene_opengl_es_test.cpp )
integrationTest(WAYLAND_ONLY NAME testNoXdgRuntimeDir SRCS no_xdg_runtime_dir_test.cpp)
integrationTest(WAYLAND_ONLY NAME testScreenChanges SRCS screen_changes_test.cpp)
integrationTest(NAME testModiferOnlyShortcut SRCS modifier_only_shortcut_test.cpp)
integrationTest(WAYLAND_ONLY NAME testTabBox SRCS tabbox_test.cpp)
integrationTest(WAYLAND_ONLY NAME testWindowSelection SRCS window_selection_test.cpp)
integrationTest(WAYLAND_ONLY NAME testPointerConstraints SRCS pointer_constraints_test.cpp)
integrationTest(WAYLAND_ONLY NAME testKeyboardLayout SRCS keyboard_layout_test.cpp)
integrationTest(WAYLAND_ONLY NAME testKeymapCreationFailure SRCS keymap_creation_failure_test.cpp)
integrationTest(WAYLAND_ONLY NAME testShowingDesktop SRCS showing_desktop_test.cpp)
integrationTest(WAYLAND_ONLY NAME testDontCrashUseractionsMenu SRCS dont_crash_useractions_menu.cpp)
integrationTest(WAYLAND_ONLY NAME testKWinBindings SRCS kwinbindings_test.cpp)
integrationTest(WAYLAND_ONLY NAME testLayerShellV1Client SRCS layershellv1client_test.cpp)
integrationTest(WAYLAND_ONLY NAME testVirtualDesktop SRCS virtual_desktop_test.cpp)
integrationTest(WAYLAND_ONLY NAME testXdgShellClientRules SRCS xdgshellclient_rules_test.cpp)
integrationTest(WAYLAND_ONLY NAME testIdleInhibition SRCS idle_inhibition_test.cpp)
integrationTest(WAYLAND_ONLY NAME testColorCorrectNightColor SRCS colorcorrect_nightcolor_test.cpp)
integrationTest(WAYLAND_ONLY NAME testDontCrashCursorPhysicalSizeEmpty SRCS dont_crash_cursor_physical_size_empty.cpp)
integrationTest(WAYLAND_ONLY NAME testDontCrashReinitializeCompositor SRCS dont_crash_reinitialize_compositor.cpp)
integrationTest(WAYLAND_ONLY NAME testNoGlobalShortcuts SRCS no_global_shortcuts_test.cpp)
integrationTest(WAYLAND_ONLY NAME testBufferSizeChange SRCS buffer_size_change_test.cpp )
integrationTest(WAYLAND_ONLY NAME testPlacement SRCS placement_test.cpp)
integrationTest(WAYLAND_ONLY NAME testActivation SRCS activation_test.cpp)
integrationTest(WAYLAND_ONLY NAME testVirtualKeyboard SRCS virtualkeyboard_test.cpp)

if (XCB_ICCCM_FOUND)
    integrationTest(NAME testMoveResize SRCS move_resize_window_test.cpp LIBS XCB::ICCCM)
    integrationTest(NAME testStruts SRCS struts_test.cpp LIBS XCB::ICCCM)
    integrationTest(NAME testShade SRCS shade_test.cpp LIBS XCB::ICCCM)
    integrationTest(NAME testDontCrashAuroraeDestroyDeco SRCS dont_crash_aurorae_destroy_deco.cpp LIBS XCB::ICCCM)
    integrationTest(NAME testPlasmaWindow SRCS plasmawindow_test.cpp LIBS XCB::ICCCM)
    integrationTest(NAME testScreenEdgeClientShow SRCS screenedge_client_show_test.cpp LIBS XCB::ICCCM)
    integrationTest(NAME testX11DesktopWindow SRCS desktop_window_x11_test.cpp LIBS XCB::ICCCM)
    integrationTest(NAME testXwaylandInput SRCS xwayland_input_test.cpp LIBS XCB::ICCCM)
    integrationTest(NAME testWindowRules SRCS window_rules_test.cpp LIBS XCB::ICCCM)
    integrationTest(NAME testX11Client SRCS x11_client_test.cpp LIBS XCB::ICCCM)
    integrationTest(NAME testQuickTiling SRCS quick_tiling_test.cpp LIBS XCB::ICCCM)
    integrationTest(NAME testGlobalShortcuts SRCS globalshortcuts_test.cpp LIBS XCB::ICCCM)
    integrationTest(NAME testSceneQPainter SRCS scene_qpainter_test.cpp LIBS XCB::ICCCM)
    integrationTest(NAME testSceneQPainterShadow SRCS scene_qpainter_shadow_test.cpp LIBS XCB::ICCCM)
    integrationTest(NAME testStackingOrder SRCS stacking_order_test.cpp LIBS XCB::ICCCM)
    integrationTest(NAME testDbusInterface SRCS dbus_interface_test.cpp LIBS XCB::ICCCM)
    integrationTest(NAME testXwaylandServerCrash SRCS xwaylandserver_crash_test.cpp LIBS XCB::ICCCM)
    integrationTest(NAME testXwaylandServerRestart SRCS xwaylandserver_restart_test.cpp LIBS XCB::ICCCM)

    if (KWIN_BUILD_ACTIVITIES)
        integrationTest(NAME testActivities SRCS activities_test.cpp LIBS XCB::ICCCM)
    endif()
endif()

add_subdirectory(scripting)
add_subdirectory(effects)
add_subdirectory(fakes)
