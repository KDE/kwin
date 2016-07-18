add_definitions(-DKWIN_UNIT_TEST)
remove_definitions(-DQT_USE_QSTRINGBUILDER)
add_subdirectory(libkwineffects)
add_subdirectory(libxrenderutils)
add_subdirectory(integration)
if (HAVE_INPUT)
    add_subdirectory(libinput)
endif()
add_subdirectory(tabbox)

########################################################
# Test ScreenPaintData
########################################################
set( testScreenPaintData_SRCS test_screen_paint_data.cpp )
add_executable(testScreenPaintData ${testScreenPaintData_SRCS})
target_link_libraries( testScreenPaintData kwineffects Qt5::Test Qt5::Widgets KF5::WindowSystem)
add_test(kwin-testScreenPaintData testScreenPaintData)
ecm_mark_as_test(testScreenPaintData)

########################################################
# Test WindowPaintData
########################################################
set( testWindowPaintData_SRCS test_window_paint_data.cpp )
add_executable(testWindowPaintData ${testWindowPaintData_SRCS})
target_link_libraries( testWindowPaintData kwineffects Qt5::Widgets Qt5::Test )
add_test(kwin-testWindowPaintData testWindowPaintData)
ecm_mark_as_test(testWindowPaintData)

########################################################
# Test VirtualDesktopManager
########################################################
set( testVirtualDesktops_SRCS
     test_virtual_desktops.cpp
     ../virtualdesktops.cpp
)
add_executable(testVirtualDesktops ${testVirtualDesktops_SRCS})

target_link_libraries( testVirtualDesktops
                       Qt5::Test
                       Qt5::Widgets
                       KF5::I18n
                       KF5::GlobalAccel
                       KF5::ConfigCore
                       KF5::WindowSystem
)
add_test(kwin-testVirtualDesktops testVirtualDesktops)
ecm_mark_as_test(testVirtualDesktops)

########################################################
# Test ClientMachine
########################################################
set( testClientMachine_SRCS
     test_client_machine.cpp
     ../client_machine.cpp
)
add_executable( testClientMachine ${testClientMachine_SRCS} )

target_link_libraries( testClientMachine
                       Qt5::Concurrent
                       Qt5::Test
                       Qt5::X11Extras
                       Qt5::Widgets
                       KF5::ConfigCore
                       KF5::WindowSystem
                       XCB::XCB
                       XCB::XFIXES
                       ${X11_X11_LIB} # to make jenkins happy
)
add_test(kwin-testClientMachine testClientMachine)
ecm_mark_as_test(testClientMachine)

########################################################
# Test XcbWrapper
########################################################
set( testXcbWrapper_SRCS
     test_xcb_wrapper.cpp
)
add_executable( testXcbWrapper ${testXcbWrapper_SRCS} )

target_link_libraries( testXcbWrapper
                       Qt5::Test
                       Qt5::X11Extras
                       Qt5::Widgets
                       KF5::ConfigCore
                       KF5::WindowSystem
                       XCB::XCB
)
add_test(kwin-testXcbWrapper testXcbWrapper)
ecm_mark_as_test(testXcbWrapper)

if (XCB_ICCCM_FOUND)
    add_executable( testXcbSizeHints test_xcb_size_hints.cpp )
    target_link_libraries( testXcbSizeHints
                           Qt5::Test
                           Qt5::X11Extras
                           KF5::ConfigCore
                           KF5::WindowSystem
                           XCB::XCB
                           XCB::ICCCM
    )
    add_test(kwin-testXcbSizeHints testXcbSizeHints)
    ecm_mark_as_test(testXcbSizeHints)
endif()

########################################################
# Test XcbWindow
########################################################
set( testXcbWindow_SRCS
     test_xcb_window.cpp
)
add_executable( testXcbWindow ${testXcbWindow_SRCS} )

target_link_libraries( testXcbWindow
                       Qt5::Test
                       Qt5::X11Extras
                       Qt5::Widgets
                       KF5::ConfigCore
                       KF5::WindowSystem
                       XCB::XCB
)
add_test(kwin-testXcbWindow testXcbWindow)
ecm_mark_as_test(testXcbWindow)

########################################################
# Test BuiltInEffectLoader
########################################################
set( testBuiltInEffectLoader_SRCS
    test_builtin_effectloader.cpp
    mock_effectshandler.cpp
    ../effectloader.cpp
)
add_executable( testBuiltInEffectLoader ${testBuiltInEffectLoader_SRCS})

target_link_libraries(testBuiltInEffectLoader
    Qt5::Concurrent
    Qt5::Test
    KF5::Package
    kwineffects
    kwin4_effect_builtins
)

add_test(kwin-testBuiltInEffectLoader testBuiltInEffectLoader)
ecm_mark_as_test(testBuiltInEffectLoader)

########################################################
# Test ScriptedEffectLoader
########################################################
include_directories(${KWIN_SOURCE_DIR})
set( testScriptedEffectLoader_SRCS
    test_scripted_effectloader.cpp
    mock_effectshandler.cpp
    ../effectloader.cpp
    ../scripting/scriptedeffect.cpp
    ../scripting/scriptingutils.cpp
    ../scripting/scripting_logging.cpp
)
add_executable( testScriptedEffectLoader ${testScriptedEffectLoader_SRCS})

target_link_libraries(testScriptedEffectLoader
    Qt5::Concurrent
    Qt5::Script
    Qt5::Test
    KF5::ConfigGui
    KF5::GlobalAccel
    KF5::I18n
    KF5::Package
    kwineffects
    kwin4_effect_builtins
)

add_test(kwin-testScriptedEffectLoader testScriptedEffectLoader)
ecm_mark_as_test(testScriptedEffectLoader)

########################################################
# Test PluginEffectLoader
########################################################
set( testPluginEffectLoader_SRCS
    test_plugin_effectloader.cpp
    mock_effectshandler.cpp
    ../effectloader.cpp
)
add_executable( testPluginEffectLoader ${testPluginEffectLoader_SRCS})

target_link_libraries(testPluginEffectLoader
    Qt5::Concurrent
    Qt5::Test
    KF5::Package
    kwineffects
    kwin4_effect_builtins
)

add_test(kwin-testPluginEffectLoader testPluginEffectLoader)
ecm_mark_as_test(testPluginEffectLoader)

########################################################
# FakeEffectPlugin
########################################################
add_library(fakeeffectplugin MODULE fakeeffectplugin.cpp)
set_target_properties(fakeeffectplugin PROPERTIES PREFIX "")
target_link_libraries(fakeeffectplugin kwineffects)

########################################################
# FakeEffectPlugin-Version
########################################################
add_library(effectversionplugin MODULE fakeeffectplugin_version.cpp)
set_target_properties(effectversionplugin PROPERTIES PREFIX "")
target_link_libraries(effectversionplugin kwineffects)

########################################################
# Test Screens
########################################################
set( testScreens_SRCS
    test_screens.cpp
    mock_abstract_client.cpp
    mock_client.cpp
    mock_screens.cpp
    mock_workspace.cpp
    ../screens.cpp
    ../x11eventfilter.cpp
)
kconfig_add_kcfg_files(testScreens_SRCS ../settings.kcfgc)

add_executable( testScreens ${testScreens_SRCS})
target_include_directories(testScreens BEFORE PRIVATE ./)
target_link_libraries(testScreens
    Qt5::Test
    Qt5::X11Extras
    KF5::ConfigCore
    KF5::ConfigGui
    KF5::WindowSystem
)

add_test(kwin_testScreens testScreens)
ecm_mark_as_test(testScreens)

########################################################
# Test XrandRScreens
########################################################
set( testXRandRScreens_SRCS
     test_xrandr_screens.cpp
     mock_abstract_client.cpp
     mock_client.cpp
     mock_screens.cpp
     mock_workspace.cpp
     ../screens.cpp
     ../plugins/platforms/x11/standalone/screens_xrandr.cpp
     ../xcbutils.cpp # init of extensions
     ../x11eventfilter.cpp
)
kconfig_add_kcfg_files(testXRandRScreens_SRCS ../settings.kcfgc)
add_executable( testXRandRScreens ${testXRandRScreens_SRCS} )
target_link_libraries( testXRandRScreens
                       Qt5::Test
                       Qt5::Gui
                       KF5::ConfigCore
                       KF5::ConfigGui
                       KF5::WindowSystem
                       XCB::XCB
                       XCB::RANDR
                       XCB::XFIXES
                       XCB::SYNC
                       XCB::COMPOSITE
                       XCB::DAMAGE
                       XCB::GLX
                       XCB::SHM
)

add_test(kwin-testXRandRScreens testXRandRScreens)
ecm_mark_as_test(testXRandRScreens)

########################################################
# Test ScreenEdges
########################################################
set( testScreenEdges_SRCS
    test_screen_edges.cpp
    mock_abstract_client.cpp
    mock_client.cpp
    mock_screens.cpp
    mock_workspace.cpp
    ../atoms.cpp
    ../screens.cpp
    ../screenedge.cpp
    ../virtualdesktops.cpp
    ../xcbutils.cpp # init of extensions
    ../plugins/platforms/x11/standalone/edge.cpp
)
kconfig_add_kcfg_files(testScreenEdges_SRCS ../settings.kcfgc)
qt5_add_dbus_interface( testScreenEdges_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../org.freedesktop.ScreenSaver.xml screenlocker_interface)

add_executable( testScreenEdges ${testScreenEdges_SRCS})
target_include_directories(testScreenEdges BEFORE PRIVATE ./)
target_link_libraries(testScreenEdges
    Qt5::DBus
    Qt5::Test
    Qt5::X11Extras
    KF5::ConfigCore
    KF5::ConfigGui
    KF5::I18n
    KF5::GlobalAccel
    KF5::WindowSystem
    XCB::XCB
    XCB::RANDR
    XCB::XFIXES
    XCB::SYNC
    XCB::COMPOSITE
    XCB::DAMAGE
    XCB::GLX
    XCB::SHM
)

add_test(kwin_testScreenEdges testScreenEdges)
ecm_mark_as_test(testScreenEdges)
