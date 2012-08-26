########################################################
# Test TabBox::ClientModel
########################################################
set( testTabBoxClientModel_SRCS
     ../clientmodel.cpp
     ../desktopmodel.cpp
     ../tabboxconfig.cpp
     ../tabboxhandler.cpp
     test_tabbox_clientmodel.cpp
     mock_declarative.cpp
     mock_tabboxhandler.cpp
     mock_tabboxclient.cpp
)

kde4_add_unit_test( testTabBoxClientModel TESTNAME testTabBoxClientModel ${testTabBoxClientModel_SRCS} )

target_link_libraries( testTabBoxClientModel ${KDE4_KDEUI_LIBS} ${QT_QTDECLARATIVE_LIBRARY} ${X11_LIBRARIES} ${QT_QTTEST_LIBRARY} )

########################################################
# Test TabBox::TabBoxHandler
########################################################
set( testTabBoxHandler_SRCS
     ../clientmodel.cpp
     ../desktopmodel.cpp
     ../tabboxconfig.cpp
     ../tabboxhandler.cpp
     test_tabbox_handler.cpp
     mock_declarative.cpp
     mock_tabboxhandler.cpp
     mock_tabboxclient.cpp
)

kde4_add_unit_test( testTabBoxHandler TESTNAME testTabBoxHandler ${testTabBoxHandler_SRCS} )

target_link_libraries( testTabBoxHandler ${KDE4_KDEUI_LIBS} ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTDECLARATIVE_LIBRARY} ${QT_QTTEST_LIBRARY} ${X11_LIBRARIES} )

########################################################
# Test TabBox::TabBoxConfig
########################################################
set( testTabBoxConfig_SRCS
    ../tabboxconfig.cpp
    test_tabbox_config.cpp
)

kde4_add_unit_test( testTabBoxConfig TESTNAME testTabBoxConfig ${testTabBoxConfig_SRCS} )
target_link_libraries( testTabBoxConfig ${QT_QTCORE_LIBRARY} ${QT_QTTEST_LIBRARY} )
