include_directories(${KWin_SOURCE_DIR}/src)
add_definitions(-DKWIN_UNIT_TEST)
########################################################
# Test TabBox::ClientModel
########################################################
set(testTabBoxClientModel_SRCS
    ../../src/tabbox/clientmodel.cpp
    ../../src/tabbox/desktopmodel.cpp
    ../../src/tabbox/tabbox_logging.cpp
    ../../src/tabbox/tabboxconfig.cpp
    ../../src/tabbox/tabboxhandler.cpp
    mock_tabboxclient.cpp
    mock_tabboxhandler.cpp
    test_tabbox_clientmodel.cpp
)

add_executable(testTabBoxClientModel ${testTabBoxClientModel_SRCS})
set_target_properties(testTabBoxClientModel PROPERTIES COMPILE_DEFINITIONS "NO_NONE_WINDOW")
target_link_libraries(testTabBoxClientModel
    Qt::Core
    Qt::DBus
    Qt::Quick
    Qt::Test
    Qt::Widgets
    Qt::GuiPrivate

    KF5::ConfigCore
    KF5::I18n
    KF5::Package
    KF5::WindowSystem

    XCB::XCB
)
if (QT_MAJOR_VERSION EQUAL "5")
    target_link_libraries(testTabBoxClientModel Qt::X11Extras)
endif()
add_test(NAME kwin-testTabBoxClientModel COMMAND testTabBoxClientModel)
ecm_mark_as_test(testTabBoxClientModel)

########################################################
# Test TabBox::TabBoxHandler
########################################################
set(testTabBoxHandler_SRCS
    ../../src/tabbox/clientmodel.cpp
    ../../src/tabbox/desktopmodel.cpp
    ../../src/tabbox/tabbox_logging.cpp
    ../../src/tabbox/tabboxconfig.cpp
    ../../src/tabbox/tabboxhandler.cpp
    mock_tabboxclient.cpp
    mock_tabboxhandler.cpp
    test_tabbox_handler.cpp
)

add_executable(testTabBoxHandler ${testTabBoxHandler_SRCS})
set_target_properties(testTabBoxHandler PROPERTIES COMPILE_DEFINITIONS "NO_NONE_WINDOW")
target_link_libraries(testTabBoxHandler
    Qt::Core
    Qt::DBus
    Qt::Quick
    Qt::Test
    Qt::Widgets
    Qt::GuiPrivate

    KF5::ConfigCore
    KF5::I18n
    KF5::Package
    KF5::WindowSystem

    XCB::XCB
)
if (QT_MAJOR_VERSION EQUAL "5")
    target_link_libraries(testTabBoxHandler Qt::X11Extras)
endif()
add_test(NAME kwin-testTabBoxHandler COMMAND testTabBoxHandler)
ecm_mark_as_test(testTabBoxHandler)

########################################################
# Test TabBox::TabBoxConfig
########################################################
set(testTabBoxConfig_SRCS
    ../../src/tabbox/tabbox_logging.cpp
    ../../src/tabbox/tabboxconfig.cpp
    test_tabbox_config.cpp
)

add_executable(testTabBoxConfig ${testTabBoxConfig_SRCS})
target_link_libraries(testTabBoxConfig Qt::Core Qt::Test)
add_test(NAME kwin-testTabBoxConfig COMMAND testTabBoxConfig)
ecm_mark_as_test(testTabBoxConfig)


########################################################
# Test TabBox::DesktopChainManager
########################################################
set(testDesktopChain_SRCS
    ../../src/tabbox/desktopchain.cpp
    ../../src/tabbox/tabbox_logging.cpp
    test_desktopchain.cpp
)

add_executable(testDesktopChain ${testDesktopChain_SRCS})
target_link_libraries(testDesktopChain Qt::Core Qt::Test)
add_test(NAME kwin-testDesktopChain COMMAND testDesktopChain)
ecm_mark_as_test(testDesktopChain)
