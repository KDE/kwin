set( testUpdateKWin49_SRCS test_update_kwin_49.cpp ../data/update_kwin_49.cpp ../tabbox/tabboxconfig.cpp )
kde4_add_unit_test( testUpdateKWin49 TESTNAME kwin-TestUpdateKWin49 ${testUpdateKWin49_SRCS} )

target_link_libraries( testUpdateKWin49 ${KDE4_KDECORE_LIBS} ${QT_QTTEST_LIBRARY} )

########################################################
# Test ScreenPaintData
########################################################
set( testScreenPaintData_SRCS test_screen_paint_data.cpp )
kde4_add_unit_test( testScreenPaintData TESTNAME kwin-TestScreenPaintData ${testScreenPaintData_SRCS} )

target_link_libraries( testScreenPaintData kwineffects ${QT_QTCORE_LIBRARY} ${QT_QTTEST_LIBRARY} )

########################################################
# Test WindowPaintData
########################################################
set( testWindowPaintData_SRCS test_window_paint_data.cpp )
kde4_add_unit_test( testWindowPaintData TESTNAME kwin-TestWindowPaintData ${testWindowPaintData_SRCS} )

target_link_libraries( testWindowPaintData kwineffects ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTTEST_LIBRARY} )

########################################################
# Test VirtualDesktopManager
########################################################
set( testVirtualDesktops_SRCS
     test_virtual_desktops.cpp
     ../virtualdesktops.cpp
)
kde4_add_unit_test( testVirtualDesktops TESTNAME kwin-TestVirtualDesktops ${testVirtualDesktops_SRCS} )

target_link_libraries( testVirtualDesktops
                       ${KDE4_KDEUI_LIBS}
                       ${QT_QTCORE_LIBRARY}
                       ${QT_QTTEST_LIBRARY}
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
                       ${QT_QTTEST_LIBRARY}
                       ${QT_QTCORE_LIBRARY}
                       ${KDE4_KDEUI_LIBS}
                       ${XCB_XCB_LIBRARIES}
                       ${XCB_XFIXES_LIBRARIES}
                       ${X11_XCB_LIBRARIES}
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
                       ${QT_QTTEST_LIBRARY}
                       ${QT_QTCORE_LIBRARY}
                       ${QT_QTGUI_LIBRARY}
                       ${XCB_XCB_LIBRARIES}
                       ${X11_XCB_LIBRARIES}
)

########################################################
# Test XcbWindow
########################################################
set( testXcbWindow_SRCS
     test_xcb_window.cpp
)
kde4_add_unit_test( testXcbWindow TESTNAME kwin-TestXcbWindow ${testXcbWindow_SRCS} )

target_link_libraries( testXcbWindow
                       ${QT_QTTEST_LIBRARY}
                       ${QT_QTCORE_LIBRARY}
                       ${QT_QTGUI_LIBRARY}
                       ${XCB_XCB_LIBRARIES}
                       ${X11_XCB_LIBRARIES}
)
