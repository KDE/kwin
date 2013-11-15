include(ECMMarkAsTest)

set( testUpdateKWin49_SRCS test_update_kwin_49.cpp ../data/update_kwin_49.cpp ../tabbox/tabboxconfig.cpp )
#add_executable( testUpdateKWin49 ${testUpdateKWin49_SRCS} )

#target_link_libraries( testUpdateKWin49  Qt5::Test )

#ecm_mark_as_test(testUpdateKWin49)
########################################################
# Test ScreenPaintData
########################################################
set( testScreenPaintData_SRCS test_screen_paint_data.cpp )

add_executable( testScreenPaintData ${testScreenPaintData_SRCS} )

target_link_libraries( testScreenPaintData kwineffects Qt5::Test Qt5::Widgets)

ecm_mark_as_test(testScreenPaintData)
########################################################
# Test WindowPaintData
########################################################
set( testWindowPaintData_SRCS test_window_paint_data.cpp )

add_executable( testWindowPaintData ${testWindowPaintData_SRCS} )

target_link_libraries( testWindowPaintData kwineffects Qt5::Widgets Qt5::Test )

ecm_mark_as_test(testWindowPaintData)
########################################################
# Test VirtualDesktopManager
########################################################
set( testVirtualDesktops_SRCS
     test_virtual_desktops.cpp
     ../virtualdesktops.cpp
)
add_executable( testVirtualDesktops ${testVirtualDesktops_SRCS} )

target_link_libraries( testVirtualDesktops
                       Qt5::Test
                       Qt5::Widgets
                       KF5::KI18n
                       KF5::XmlGui
                       KF5::KWindowSystem
)

ecm_mark_as_test(testVirtualDesktops)
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

add_executable( testClientMachine ${testClientMachine_SRCS} )

target_link_libraries( testClientMachine
                       Qt5::Concurrent
                       Qt5::Test
                       Qt5::X11Extras
                       Qt5::Widgets
                       KF5::KWindowSystem
                       ${XCB_XCB_LIBRARY}
                       ${XCB_XFIXES_LIBRARY}
                       ${X11_X11_LIB} # to make jenkins happy
                       ${X11_Xcursor_LIB}
)

ecm_mark_as_test(testClientMachine)
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
                       ${XCB_XCB_LIBRARY}
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
                       ${XCB_XCB_LIBRARY}
)
