set( testUpdateKWin49_SRCS test_update_kwin_49.cpp ../data/update_kwin_49.cpp ../tabbox/tabboxconfig.cpp )
kde4_add_unit_test( testUpdateKWin49 TESTNAME TestUpdateKWin49 ${testUpdateKWin49_SRCS} )

target_link_libraries( testUpdateKWin49 ${KDE4_KDECORE_LIBS} ${QT_QTTEST_LIBRARY} )

########################################################
# Test ScreenPaintData
########################################################
set( testScreenPaintData_SRCS test_screen_paint_data.cpp )
kde4_add_unit_test( testScreenPaintData TESTNAME TestScreenPaintData ${testScreenPaintData_SRCS} )

target_link_libraries( testScreenPaintData kwineffects ${QT_QTCORE_LIBRARY} ${QT_QTTEST_LIBRARY} )

########################################################
# Test WindowPaintData
########################################################
set( testWindowPaintData_SRCS test_window_paint_data.cpp )
kde4_add_unit_test( testWindowPaintData TESTNAME TestWindowPaintData ${testWindowPaintData_SRCS} )

target_link_libraries( testWindowPaintData kwineffects ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTTEST_LIBRARY} )
