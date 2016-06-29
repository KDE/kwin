add_definitions(-DKWINBACKENDPATH="${CMAKE_BINARY_DIR}/plugins/platforms/virtual/KWinWaylandVirtualBackend.so")
add_definitions(-DKWINQPAPATH="${CMAKE_BINARY_DIR}/plugins/qpa/")
add_subdirectory(helper)
########################################################
# Test Start
########################################################
set( testStart_SRCS start_test.cpp kwin_wayland_test.cpp )
add_executable(testStart ${testStart_SRCS})
target_link_libraries( testStart kwin Qt5::Test)
add_test(kwin-testStart testStart)
ecm_mark_as_test(testStart)

########################################################
# Transient Window without input test
########################################################
set( testTransientNoInput_SRCS transient_no_input_test.cpp kwin_wayland_test.cpp )
add_executable(testTransientNoInput ${testTransientNoInput_SRCS})
target_link_libraries( testTransientNoInput kwin Qt5::Test)
add_test(kwin-testTransientNoInput testTransientNoInput)
ecm_mark_as_test(testTransientNoInput)

########################################################
# Quick Tiling test
########################################################
set( testQuickTiling_SRCS quick_tiling_test.cpp kwin_wayland_test.cpp )
add_executable(testQuickTiling ${testQuickTiling_SRCS})
target_link_libraries( testQuickTiling kwin Qt5::Test)
add_test(kwin-testQuickTiling testQuickTiling)
ecm_mark_as_test(testQuickTiling)

########################################################
# Move/Resize window test
########################################################
if (XCB_ICCCM_FOUND)
    set( testMoveResize_SRCS move_resize_window_test.cpp kwin_wayland_test.cpp )
    add_executable(testMoveResize ${testMoveResize_SRCS})
    target_link_libraries( testMoveResize kwin Qt5::Test XCB::ICCCM)
    add_test(kwin-testMoveResize testMoveResize)
    ecm_mark_as_test(testMoveResize)
endif()

########################################################
# Don't Crash For glxgears
########################################################
set( testDontCrashGlxgears_SRCS dont_crash_glxgears.cpp kwin_wayland_test.cpp )
add_executable(testDontCrashGlxgears ${testDontCrashGlxgears_SRCS})
target_link_libraries( testDontCrashGlxgears kwin Qt5::Test)
add_test(kwin-testDontCrashGlxgears testDontCrashGlxgears)
ecm_mark_as_test(testDontCrashGlxgears)

########################################################
# Lock screen integration test
########################################################
set( testLockScreen_SRCS lockscreen.cpp kwin_wayland_test.cpp )
add_executable(testLockScreen ${testLockScreen_SRCS})
target_link_libraries( testLockScreen kwin Qt5::Test)
add_test(kwin-testLockScreen testLockScreen)
ecm_mark_as_test(testLockScreen)

########################################################
# Decoration input test
########################################################
set( testDecorationInput_SRCS decoration_input_test.cpp kwin_wayland_test.cpp )
add_executable(testDecorationInput ${testDecorationInput_SRCS})
target_link_libraries( testDecorationInput kwin Qt5::Test)
add_test(kwin-testDecorationInput testDecorationInput)
ecm_mark_as_test(testDecorationInput)

########################################################
# Internal Window test
########################################################
set( testInternalWindow_SRCS internal_window.cpp kwin_wayland_test.cpp )
add_executable(testInternalWindow ${testInternalWindow_SRCS})
target_link_libraries( testInternalWindow kwin Qt5::Test)
add_test(kwin-testInternalWindow testInternalWindow)
ecm_mark_as_test(testInternalWindow)

########################################################
# Touch Input Test
########################################################
set( testTouchInput_SRCS touch_input_test.cpp kwin_wayland_test.cpp )
add_executable(testTouchInput ${testTouchInput_SRCS})
target_link_libraries( testTouchInput kwin Qt5::Test)
add_test(kwin-testTouchInput testTouchInput)
ecm_mark_as_test(testTouchInput)

########################################################
# Input Stacking Order Test
########################################################
set( testInputStackingOrder_SRCS input_stacking_order.cpp kwin_wayland_test.cpp )
add_executable(testInputStackingOrder ${testInputStackingOrder_SRCS})
target_link_libraries( testInputStackingOrder kwin Qt5::Test)
add_test(kwin-testInputStackingOrder testInputStackingOrder)
ecm_mark_as_test(testInputStackingOrder)

########################################################
# Pointer Input Test
########################################################
set( testPointerInput_SRCS pointer_input.cpp kwin_wayland_test.cpp )
add_executable(testPointerInput ${testPointerInput_SRCS})
target_link_libraries( testPointerInput kwin Qt5::Test)
add_test(kwin-testPointerInput testPointerInput)
ecm_mark_as_test(testPointerInput)

########################################################
# PlatformCursorTest
########################################################
set( testPlatformCursor_SRCS platformcursor.cpp kwin_wayland_test.cpp )
add_executable(testPlatformCursor ${testPlatformCursor_SRCS})
target_link_libraries( testPlatformCursor kwin Qt5::Test)
add_test(kwin-testPlatformCursor testPlatformCursor)
ecm_mark_as_test(testPlatformCursor)

########################################################
# Don't crash cancel animation test
########################################################
set( testDontCrashCancelAnimation_SRCS dont_crash_cancel_animation.cpp kwin_wayland_test.cpp )
add_executable(testDontCrashCancelAnimation ${testDontCrashCancelAnimation_SRCS})
target_link_libraries( testDontCrashCancelAnimation kwin Qt5::Test)
add_test(kwin-testDontCrashCancelAnimation testDontCrashCancelAnimation)
ecm_mark_as_test(testDontCrashCancelAnimation)

########################################################
# Transient Placement Test
########################################################
set( testTransientPlacmenet_SRCS transient_placement.cpp kwin_wayland_test.cpp )
add_executable(testTransientPlacmenet ${testTransientPlacmenet_SRCS})
target_link_libraries( testTransientPlacmenet kwin Qt5::Test)
add_test(kwin-testTransientPlacmenet testTransientPlacmenet)
ecm_mark_as_test(testTransientPlacmenet)

########################################################
# Debug Console Test
########################################################
set( testDebugConsole_SRCS debug_console_test.cpp kwin_wayland_test.cpp )
add_executable(testDebugConsole ${testDebugConsole_SRCS})
target_link_libraries( testDebugConsole kwin Qt5::Test)
add_test(kwin-testDebugConsole testDebugConsole)
ecm_mark_as_test(testDebugConsole)

########################################################
# Struts Test
########################################################
if (XCB_ICCCM_FOUND)
    set( testStruts_SRCS struts_test.cpp kwin_wayland_test.cpp )
    add_executable(testStruts ${testStruts_SRCS})
    target_link_libraries( testStruts kwin Qt5::Test XCB::ICCCM)
    add_test(kwin-testStruts testStruts)
    ecm_mark_as_test(testStruts)
endif()

########################################################
# Shade Test
########################################################
if (XCB_ICCCM_FOUND)
    set( testShade_SRCS shade_test.cpp kwin_wayland_test.cpp )
    add_executable(testShade ${testShade_SRCS})
    target_link_libraries( testShade kwin Qt5::Test XCB::ICCCM)
    add_test(kwin-testShade testShade)
    ecm_mark_as_test(testShade)
endif()

########################################################
# Dont Crash Empty Deco Test
########################################################
set( testDontCrashEmptyDeco_SRCS dont_crash_empty_deco.cpp kwin_wayland_test.cpp )
add_executable(testDontCrashEmptyDeco ${testDontCrashEmptyDeco_SRCS})
target_link_libraries( testDontCrashEmptyDeco kwin Qt5::Test)
add_test(kwin-testDontCrashEmptyDeco testDontCrashEmptyDeco)
ecm_mark_as_test(testDontCrashEmptyDeco)

########################################################
# Dont Crash Aurorae Destroy Deco
########################################################
if (XCB_ICCCM_FOUND)
    set( testDontCrashAuroraeDestroyDeco_SRCS dont_crash_aurorae_destroy_deco.cpp kwin_wayland_test.cpp )
    add_executable(testDontCrashAuroraeDestroyDeco ${testDontCrashAuroraeDestroyDeco_SRCS})
    target_link_libraries( testDontCrashAuroraeDestroyDeco kwin Qt5::Test XCB::ICCCM)
    add_test(kwin-testDontCrashAuroraeDestroyDeco testDontCrashAuroraeDestroyDeco)
    ecm_mark_as_test(testDontCrashAuroraeDestroyDeco)
endif()

########################################################
# PlasmaWindow test
########################################################
if (XCB_ICCCM_FOUND)
    set( testPlasmaWindow_SRCS plasmawindow_test.cpp kwin_wayland_test.cpp )
    add_executable(testPlasmaWindow ${testPlasmaWindow_SRCS})
    target_link_libraries( testPlasmaWindow kwin Qt5::Test XCB::ICCCM)
    add_test(kwin-testPlasmaWindow testPlasmaWindow)
    ecm_mark_as_test(testPlasmaWindow)
endif()

########################################################
# Plasma Surface Test
########################################################
set( testPlasmaSurface_SRCS plasma_surface_test.cpp kwin_wayland_test.cpp )
add_executable(testPlasmaSurface ${testPlasmaSurface_SRCS})
target_link_libraries( testPlasmaSurface kwin Qt5::Test)
add_test(kwin-testPlasmaSurface testPlasmaSurface)
ecm_mark_as_test(testPlasmaSurface)

########################################################
# Maximized Windows Test
########################################################
set( testMaximized_SRCS maximize_test.cpp kwin_wayland_test.cpp )
add_executable(testMaximized ${testMaximized_SRCS})
target_link_libraries( testMaximized kwin Qt5::Test)
add_test(kwin-testMaximized testMaximized)
ecm_mark_as_test(testMaximized)

########################################################
# Shell Client Test
########################################################
set( testShellClient_SRCS shell_client_test.cpp kwin_wayland_test.cpp )
add_executable(testShellClient ${testShellClient_SRCS})
target_link_libraries( testShellClient kwin Qt5::Test)
add_test(kwin-testShellClient testShellClient)
ecm_mark_as_test(testShellClient)

########################################################
# DontCrashNoBorder Test
########################################################
set( testDontCrashNoBorder_SRCS dont_crash_no_border.cpp kwin_wayland_test.cpp )
add_executable(testDontCrashNoBorder ${testDontCrashNoBorder_SRCS})
target_link_libraries( testDontCrashNoBorder kwin Qt5::Test)
add_test(kwin-testDontCrashNoBorder testDontCrashNoBorder)
ecm_mark_as_test(testDontCrashNoBorder)

########################################################
# XClipboardSync Test
########################################################
set( testXClipboardSync_SRCS xclipboardsync_test.cpp kwin_wayland_test.cpp )
add_executable(testXClipboardSync ${testXClipboardSync_SRCS})
target_link_libraries( testXClipboardSync kwin Qt5::Test)
add_test(kwin-testXClipboardSync testXClipboardSync)
ecm_mark_as_test(testXClipboardSync)

########################################################
# SceneQPainter Test
########################################################
set( testSceneQPainter_SRCS scene_qpainter_test.cpp kwin_wayland_test.cpp )
add_executable(testSceneQPainter ${testSceneQPainter_SRCS})
target_link_libraries( testSceneQPainter kwin Qt5::Test)
add_test(kwin-testSceneQPainter testSceneQPainter)
ecm_mark_as_test(testSceneQPainter)
