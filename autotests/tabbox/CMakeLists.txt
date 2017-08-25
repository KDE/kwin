include_directories(${KWIN_SOURCE_DIR})
add_definitions(-DKWIN_UNIT_TEST)
########################################################
# Test TabBox::ClientModel
########################################################
set( testTabBoxClientModel_SRCS
     ../../tabbox/clientmodel.cpp
     ../../tabbox/desktopmodel.cpp
     ../../tabbox/tabboxconfig.cpp
     ../../tabbox/tabboxhandler.cpp
     ../../tabbox/tabbox_logging.cpp
     test_tabbox_clientmodel.cpp
     mock_tabboxhandler.cpp
     mock_tabboxclient.cpp
)

add_executable( testTabBoxClientModel ${testTabBoxClientModel_SRCS} )
set_target_properties(testTabBoxClientModel PROPERTIES COMPILE_DEFINITIONS "NO_NONE_WINDOW")
target_link_libraries( testTabBoxClientModel
    Qt5::Core
    Qt5::Widgets
    Qt5::Script
    Qt5::Quick
    Qt5::Test
    Qt5::X11Extras
    KF5::ConfigCore
    KF5::I18n
    KF5::Package
    KF5::WindowSystem
    XCB::XCB
)
add_test(NAME kwin-testTabBoxClientModel COMMAND testTabBoxClientModel)
ecm_mark_as_test(testTabBoxClientModel)

########################################################
# Test TabBox::TabBoxHandler
########################################################
set( testTabBoxHandler_SRCS
     ../../tabbox/clientmodel.cpp
     ../../tabbox/desktopmodel.cpp
     ../../tabbox/tabboxconfig.cpp
     ../../tabbox/tabboxhandler.cpp
     ../../tabbox/tabbox_logging.cpp
     test_tabbox_handler.cpp
     mock_tabboxhandler.cpp
     mock_tabboxclient.cpp
)

add_executable( testTabBoxHandler ${testTabBoxHandler_SRCS} )
set_target_properties(testTabBoxHandler PROPERTIES COMPILE_DEFINITIONS "NO_NONE_WINDOW")
target_link_libraries( testTabBoxHandler
    Qt5::Core
    Qt5::Widgets
    Qt5::Script
    Qt5::Quick
    Qt5::Test
    Qt5::X11Extras
    KF5::ConfigCore
    KF5::I18n
    KF5::Package
    KF5::WindowSystem
    XCB::XCB
)
add_test(NAME kwin-testTabBoxHandler COMMAND testTabBoxHandler)
ecm_mark_as_test(testTabBoxHandler)

########################################################
# Test TabBox::TabBoxConfig
########################################################
set( testTabBoxConfig_SRCS
    ../../tabbox/tabboxconfig.cpp
    ../../tabbox/tabbox_logging.cpp
    test_tabbox_config.cpp
)

add_executable( testTabBoxConfig ${testTabBoxConfig_SRCS} )
target_link_libraries( testTabBoxConfig Qt5::Core Qt5::Test )
add_test(NAME kwin-testTabBoxConfig COMMAND testTabBoxConfig)
ecm_mark_as_test(testTabBoxConfig)


########################################################
# Test TabBox::DesktopChainManager
########################################################
set( testDesktopChain_SRCS
    ../../tabbox/desktopchain.cpp
    ../../tabbox/tabbox_logging.cpp
    test_desktopchain.cpp
)

add_executable( testDesktopChain ${testDesktopChain_SRCS} )
target_link_libraries( testDesktopChain Qt5::Core Qt5::Test )
add_test(NAME kwin-testDesktopChain COMMAND testDesktopChain)
ecm_mark_as_test(testDesktopChain)
