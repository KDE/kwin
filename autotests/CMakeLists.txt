add_definitions(-DKWIN_UNIT_TEST)
remove_definitions(-DQT_USE_QSTRINGBUILDER)
add_subdirectory(effect)
add_subdirectory(integration)
add_subdirectory(libinput)
add_subdirectory(wayland)
# drm autotests are broken on FreeBSD for yet unknown reasons
# As the test isn't doing anything platform specific, only run it on Linux
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    add_subdirectory(drm)
endif()

########################################################
# Test WindowPaintData
########################################################
set(testWindowPaintData_SRCS test_window_paint_data.cpp)
add_executable(testWindowPaintData ${testWindowPaintData_SRCS})
target_link_libraries(testWindowPaintData kwin Qt::Widgets Qt::Test )
add_test(NAME kwin-testWindowPaintData COMMAND testWindowPaintData)
ecm_mark_as_test(testWindowPaintData)

########################################################
# Test VirtualDesktopManager
########################################################
set(testVirtualDesktops_SRCS
    ../src/virtualdesktops.cpp
    test_virtual_desktops.cpp
)
add_executable(testVirtualDesktops ${testVirtualDesktops_SRCS})

target_link_libraries(testVirtualDesktops
    kwin

    Qt::Test
    Qt::Widgets

    KF6::ConfigCore
    KF6::GlobalAccel
    KF6::I18n
    KF6::WindowSystem
)
add_test(NAME kwin-testVirtualDesktops COMMAND testVirtualDesktops)
ecm_mark_as_test(testVirtualDesktops)

########################################################
# Test ClientMachine
########################################################
set(testClientMachine_SRCS
    ../src/client_machine.cpp
    test_client_machine.cpp
    xcb_scaling_mock.cpp
)
add_executable(testClientMachine ${testClientMachine_SRCS})
set_target_properties(testClientMachine PROPERTIES COMPILE_DEFINITIONS "NO_NONE_WINDOW")

target_link_libraries(testClientMachine
    Qt::Concurrent
    Qt::GuiPrivate
    Qt::Test
    Qt::Widgets

    KF6::ConfigCore
    KF6::WindowSystem

    XCB::XCB
    XCB::XFIXES

    ${X11_X11_LIB} # to make jenkins happy
)
add_test(NAME kwin-testClientMachine COMMAND testClientMachine)
ecm_mark_as_test(testClientMachine)

########################################################
# Test XcbWrapper
########################################################
add_executable(testXcbWrapper test_xcb_wrapper.cpp xcb_scaling_mock.cpp)

target_link_libraries(testXcbWrapper
    Qt::GuiPrivate
    Qt::Test
    Qt::Widgets

    KF6::ConfigCore
    KF6::WindowSystem

    XCB::XCB
)
add_test(NAME kwin-testXcbWrapper COMMAND testXcbWrapper)
ecm_mark_as_test(testXcbWrapper)

add_executable(testXcbSizeHints test_xcb_size_hints.cpp xcb_scaling_mock.cpp)
set_target_properties(testXcbSizeHints PROPERTIES COMPILE_DEFINITIONS "NO_NONE_WINDOW")
target_link_libraries(testXcbSizeHints
    Qt::GuiPrivate
    Qt::Test
    Qt::Widgets

    KF6::ConfigCore
    KF6::WindowSystem

    XCB::ICCCM
    XCB::XCB
)
add_test(NAME kwin-testXcbSizeHints COMMAND testXcbSizeHints)
ecm_mark_as_test(testXcbSizeHints)

########################################################
# Test XcbWindow
########################################################
add_executable(testXcbWindow test_xcb_window.cpp xcb_scaling_mock.cpp)

target_link_libraries(testXcbWindow
    Qt::GuiPrivate
    Qt::Test
    Qt::Widgets

    KF6::ConfigCore
    KF6::WindowSystem

    XCB::XCB
)
add_test(NAME kwin-testXcbWindow COMMAND testXcbWindow)
ecm_mark_as_test(testXcbWindow)

########################################################
# Test OnScreenNotification
########################################################
set(testOnScreenNotification_SRCS
    ../src/input_event_spy.cpp
    ../src/onscreennotification.cpp
    onscreennotificationtest.cpp
)
add_executable(testOnScreenNotification ${testOnScreenNotification_SRCS})

target_link_libraries(testOnScreenNotification
    Qt::DBus
    Qt::Quick
    Qt::Test
    Qt::Widgets # QAction include

    KF6::ConfigCore
)

add_test(NAME kwin-testOnScreenNotification COMMAND testOnScreenNotification)
ecm_mark_as_test(testOnScreenNotification)

########################################################
# Test Gestures
########################################################
set(testGestures_SRCS
    ../src/gestures.cpp
    test_gestures.cpp
)
add_executable(testGestures ${testGestures_SRCS})

target_link_libraries(testGestures
    Qt::Test
    Qt::Widgets
)

add_test(NAME kwin-testGestures COMMAND testGestures)
ecm_mark_as_test(testGestures)

########################################################
# Test X11 TimestampUpdate
########################################################
add_executable(testX11TimestampUpdate test_x11_timestamp_update.cpp)
target_link_libraries(testX11TimestampUpdate
    KF6::CoreAddons
    Qt::Test
    Qt::GuiPrivate
    kwin
)
add_test(NAME kwin-testX11TimestampUpdate COMMAND testX11TimestampUpdate)
ecm_mark_as_test(testX11TimestampUpdate)

set(testOpenGLContextAttributeBuilder_SRCS
    ../src/opengl/abstract_opengl_context_attribute_builder.cpp
    ../src/opengl/egl_context_attribute_builder.cpp
    opengl_context_attribute_builder_test.cpp
)

if (HAVE_GLX)
    set(testOpenGLContextAttributeBuilder_SRCS ${testOpenGLContextAttributeBuilder_SRCS} ../src/backends/x11/standalone/x11_standalone_glx_context_attribute_builder.cpp)
endif()
add_executable(testOpenGLContextAttributeBuilder ${testOpenGLContextAttributeBuilder_SRCS})
target_link_libraries(testOpenGLContextAttributeBuilder epoxy::epoxy Qt::Test)
add_test(NAME kwin-testOpenGLContextAttributeBuilder COMMAND testOpenGLContextAttributeBuilder)
ecm_mark_as_test(testOpenGLContextAttributeBuilder)

set(testXkb_SRCS
    ../src/xkb.cpp
    test_xkb.cpp
)
qt_add_dbus_interface(testXkb_SRCS ${CMAKE_SOURCE_DIR}/src/org.freedesktop.DBus.Properties.xml dbusproperties_interface)
add_executable(testXkb ${testXkb_SRCS})
target_link_libraries(testXkb
    kwin

    Qt::Gui
    Qt::GuiPrivate
    Qt::Test
    Qt::Widgets

    KF6::ConfigCore
    KF6::WindowSystem

    XKB::XKB
)
add_test(NAME kwin-testXkb COMMAND testXkb)
ecm_mark_as_test(testXkb)

########################################################
# Test FTrace
########################################################
add_executable(testFtrace test_ftrace.cpp)
target_link_libraries(testFtrace
    Qt::Test
    kwin
)
add_test(NAME kwin-testFtrace COMMAND testFtrace)
ecm_mark_as_test(testFtrace)

########################################################
# Test KWin Utils
########################################################
add_executable(testUtils test_utils.cpp)
target_link_libraries(testUtils
    Qt::Test
    kwin
)
add_test(NAME kwin-testUtils COMMAND testUtils)
ecm_mark_as_test(testUtils)

########################################################
# Test OutputTransform
########################################################
add_executable(testOutputTransform output_transform_test.cpp)
target_link_libraries(testOutputTransform
    Qt::Test
    kwin
)
add_test(NAME kwin-testOutputTransform COMMAND testOutputTransform)
ecm_mark_as_test(testOutputTransform)

########################################################
# Test Colorspace
########################################################
add_executable(testColorspaces test_colorspaces.cpp)
target_link_libraries(testColorspaces
    Qt::Test
    kwin
)
add_test(NAME kwin-testColorspaces COMMAND testColorspaces)
ecm_mark_as_test(testColorspaces)
