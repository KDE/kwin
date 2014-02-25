########################################################
# Test ScreenPaintData
########################################################
set( testScreenPaintData_SRCS test_screen_paint_data.cpp )
kde4_add_unit_test( testScreenPaintData TESTNAME kwin-TestScreenPaintData ${testScreenPaintData_SRCS} )

target_link_libraries( testScreenPaintData kwineffects Qt5::Test Qt5::Widgets)

########################################################
# Test WindowPaintData
########################################################
set( testWindowPaintData_SRCS test_window_paint_data.cpp )
kde4_add_unit_test( testWindowPaintData TESTNAME kwin-TestWindowPaintData ${testWindowPaintData_SRCS} )

target_link_libraries( testWindowPaintData kwineffects Qt5::Widgets Qt5::Test )

########################################################
# Test VirtualDesktopManager
########################################################
set( testVirtualDesktops_SRCS
     test_virtual_desktops.cpp
     ../virtualdesktops.cpp
)
kde4_add_unit_test( testVirtualDesktops TESTNAME kwin-TestVirtualDesktops ${testVirtualDesktops_SRCS} )

target_link_libraries( testVirtualDesktops
                       Qt5::Test
                       Qt5::Widgets
                       KF5::I18n
                       KF5::GlobalAccel
                       KF5::ConfigCore
                       KF5::WindowSystem
)

########################################################
# Test ClientMachine
########################################################
set( testClientMachine_SRCS
     test_client_machine.cpp
     ../client_machine.cpp
     ../utils.cpp
     ../atoms.cpp # needed by utils.cpp
     ../cursor.cpp # needed by utils.cpp
)
kde4_add_unit_test( testClientMachine TESTNAME kwin-TestClientMachine ${testClientMachine_SRCS} )

target_link_libraries( testClientMachine
                       Qt5::Concurrent
                       Qt5::Test
                       Qt5::X11Extras
                       Qt5::Widgets
                       KF5::WindowSystem
                       XCB::XCB
                       XCB::XFIXES
                       ${X11_X11_LIB} # to make jenkins happy
                       ${X11_Xcursor_LIB}
)

########################################################
# Test XcbWrapper
########################################################
set( testXcbWrapper_SRCS
     test_xcb_wrapper.cpp
)
kde4_add_unit_test( testXcbWrapper TESTNAME kwin-TestXcbWrapper ${testXcbWrapper_SRCS} )

target_link_libraries( testXcbWrapper
                       Qt5::Test
                       Qt5::X11Extras
                       Qt5::Widgets
                       XCB::XCB
)

########################################################
# Test XcbWindow
########################################################
set( testXcbWindow_SRCS
     test_xcb_window.cpp
)
kde4_add_unit_test( testXcbWindow TESTNAME kwin-TestXcbWindow ${testXcbWindow_SRCS} )

target_link_libraries( testXcbWindow
                       Qt5::Test
                       Qt5::X11Extras
                       Qt5::Widgets
                       XCB::XCB
)
