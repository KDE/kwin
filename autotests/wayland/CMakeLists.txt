add_definitions(-DKWINBACKENDPATH="${CMAKE_BINARY_DIR}/backends/virtual/KWinWaylandVirtualBackend.so")
add_definitions(-DKWINQPAPATH="${CMAKE_BINARY_DIR}/plugins/qpa/")
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
set( testMoveResize_SRCS move_resize_window_test.cpp kwin_wayland_test.cpp )
add_executable(testMoveResize ${testMoveResize_SRCS})
target_link_libraries( testMoveResize kwin Qt5::Test)
add_test(kwin-testMoveResize testMoveResize)
ecm_mark_as_test(testMoveResize)

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
# Dont Crash Empty Deco Test
########################################################
set( testDontCrashEmptyDeco_SRCS dont_crash_empty_deco.cpp kwin_wayland_test.cpp )
add_executable(testDontCrashEmptyDeco ${testDontCrashEmptyDeco_SRCS})
target_link_libraries( testDontCrashEmptyDeco kwin Qt5::Test)
add_test(kwin-testDontCrashEmptyDeco testDontCrashEmptyDeco)
ecm_mark_as_test(testDontCrashEmptyDeco)
