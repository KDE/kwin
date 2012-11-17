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

kde4_add_unit_test( testTabBoxClientModel TESTNAME kwin-testTabBoxClientModel ${testTabBoxClientModel_SRCS} )

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

kde4_add_unit_test( testTabBoxHandler TESTNAME kwin-testTabBoxHandler ${testTabBoxHandler_SRCS} )

target_link_libraries( testTabBoxHandler ${KDE4_KDEUI_LIBS} ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTDECLARATIVE_LIBRARY} ${QT_QTTEST_LIBRARY} ${X11_LIBRARIES} )

########################################################
# Test TabBox::TabBoxConfig
########################################################
set( testTabBoxConfig_SRCS
    ../tabboxconfig.cpp
    test_tabbox_config.cpp
)

kde4_add_unit_test( testTabBoxConfig TESTNAME kwin-testTabBoxConfig ${testTabBoxConfig_SRCS} )
target_link_libraries( testTabBoxConfig ${QT_QTCORE_LIBRARY} ${QT_QTTEST_LIBRARY} )


########################################################
# Test TabBox::DesktopChainManager
########################################################
set( testDesktopChain_SRCS
    ../desktopchain.cpp
    test_desktopchain.cpp
)

kde4_add_unit_test( testDesktopChain TESTNAME kwin-testDesktopChain ${testDesktopChain_SRCS} )
target_link_libraries( testDesktopChain ${QT_QTCORE_LIBRARY} ${QT_QTTEST_LIBRARY} )
