set( testUpdateKWin49_SRCS test_update_kwin_49.cpp ../data/update_kwin_49.cpp ../tabbox/tabboxconfig.cpp )
kde4_add_unit_test( testUpdateKWin49 TESTNAME TestUpdateKWin49 ${testUpdateKWin49_SRCS} )

target_link_libraries( testUpdateKWin49 ${KDE4_KDECORE_LIBS} ${QT_QTTEST_LIBRARY} )
