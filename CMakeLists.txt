cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(KWIN)
set(PROJECT_VERSION "5.12.80")
set(PROJECT_VERSION_MAJOR 5)

set(QT_MIN_VERSION "5.9.0")
set(KF5_MIN_VERSION "5.42.0")

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH} )

find_package(ECM 5.38 REQUIRED NO_MODULE)

include(FeatureSummary)
include(WriteBasicConfigVersionFile)
include(GenerateExportHeader)

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Concurrent
    Core
    DBus
    Quick
    QuickWidgets
    Sensors
    Script
    UiTools
    Widgets
    X11Extras
)

find_package(Qt5Test ${QT_MIN_VERSION} CONFIG QUIET)
set_package_properties(Qt5Test PROPERTIES
         PURPOSE "Required for tests"
         TYPE OPTIONAL
         )
add_feature_info("Qt5Test" Qt5Test_FOUND "Required for building tests")
if (NOT Qt5Test_FOUND)
    set(BUILD_TESTING OFF CACHE BOOL "Build the testing tree.")
endif()

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

include(ECMInstallIcons)
include(ECMOptionalAddSubdirectory)

add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0 -DQT_USE_QSTRINGBUILDER)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-inconsistent-missing-override")
endif()

find_package(Qt5Multimedia QUIET)
set_package_properties(Qt5Multimedia PROPERTIES
         PURPOSE "Runtime-only dependency for effect video playback"
         TYPE RUNTIME
         )


# required frameworks by Core
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS
    Config
    ConfigWidgets
    CoreAddons
    Crash
    GlobalAccel
    I18n
    Init
    Notifications
    Package
    Plasma
    WidgetsAddons
    WindowSystem
    IconThemes
    IdleTime
    Wayland
)
# required frameworks by config modules
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS
    Completion
    Declarative
    KCMUtils
    KIO
    TextWidgets
    NewStuff
    Service
    XmlGui
)

find_package(Threads)
set_package_properties(Threads PROPERTIES
         PURPOSE "Needed for VirtualTerminal support in KWin Wayland"
         TYPE REQUIRED
         )

# optional frameworks
find_package(KF5Activities ${KF5_MIN_VERSION} CONFIG)
set_package_properties(KF5Activities PROPERTIES
         PURPOSE "Enable building of KWin with kactivities support"
         TYPE OPTIONAL
         )
add_feature_info("KF5Activities" KF5Activities_FOUND "Enable building of KWin with kactivities support")

find_package(KF5DocTools ${KF5_MIN_VERSION} CONFIG)
set_package_properties(KF5DocTools PROPERTIES
         PURPOSE "Enable building documentation"
         TYPE OPTIONAL
         )
add_feature_info("KF5DocTools" KF5DocTools_FOUND "Enable building documentation")

find_package(KDecoration2 CONFIG REQUIRED)

find_package(KScreenLocker CONFIG REQUIRED)
set_package_properties(KScreenLocker PROPERTIES
                       TYPE REQUIRED
                       PURPOSE "For screenlocker integration in kwin_wayland")

find_package(Breeze 5.9.0 CONFIG)
set_package_properties(Breeze PROPERTIES
                       TYPE OPTIONAL
                       PURPOSE "For setting the default window decoration plugin")
if(${Breeze_FOUND})
    if(${BREEZE_WITH_KDECORATION})
        set(HAVE_BREEZE_DECO true)
    else()
        set(HAVE_BREEZE_DECO FALSE)
    endif()
else()
    set(HAVE_BREEZE_DECO FALSE)
endif()
add_feature_info("Breeze-Decoration" HAVE_BREEZE_DECO "Default decoration plugin Breeze")

find_package(EGL)
set_package_properties(EGL PROPERTIES
                       TYPE RUNTIME
                       PURPOSE "Required to build KWin with EGL support"
                      )

find_package(epoxy)
set_package_properties(epoxy PROPERTIES DESCRIPTION "libepoxy"
                       URL "http://github.com/anholt/libepoxy"
                       TYPE REQUIRED
                       PURPOSE "OpenGL dispatch library"
                      )

set(HAVE_DL_LIBRARY FALSE)
if(epoxy_HAS_GLX)
    find_library(DL_LIBRARY dl)
    if(DL_LIBRARY)
        set(HAVE_DL_LIBRARY TRUE)
    endif()
endif()

find_package(Wayland 1.2 REQUIRED COMPONENTS Cursor OPTIONAL_COMPONENTS Egl)
set_package_properties(Wayland PROPERTIES
                       TYPE REQUIRED
                       PURPOSE "Required for building KWin with Wayland support"
                      )
add_feature_info("Wayland::EGL" Wayland_Egl_FOUND "Enable building of Wayland backend and QPA with EGL support.")
set(HAVE_WAYLAND_EGL FALSE)
if(Wayland_Egl_FOUND)
    set(HAVE_WAYLAND_EGL TRUE)
endif()

find_package(XKB 0.7.0)
set_package_properties(XKB PROPERTIES
                       TYPE REQUIRED
                       PURPOSE "Required for building KWin with Wayland support"
                      )

find_package(Libinput 1.9)
set_package_properties(Libinput PROPERTIES TYPE OPTIONAL PURPOSE "Required for input handling on Wayland.")

find_package(UDev)
set_package_properties(UDev PROPERTIES  URL "http://www.freedesktop.org/software/systemd/libudev/"
                                        DESCRIPTION "Linux device library."
                                        TYPE OPTIONAL
                                        PURPOSE "Required for input handling on Wayland."
                        )
set(HAVE_INPUT FALSE)
if (Libinput_FOUND AND UDEV_FOUND)
    set(HAVE_INPUT TRUE)
endif()
set(HAVE_UDEV FALSE)
if (UDEV_FOUND)
    set(HAVE_UDEV TRUE)
endif()

find_package(Libdrm 2.4.62)
set_package_properties(Libdrm PROPERTIES TYPE OPTIONAL PURPOSE "Required for drm output on Wayland.")
set(HAVE_DRM FALSE)
if(Libdrm_FOUND AND UDEV_FOUND)
    set(HAVE_DRM TRUE)
endif()

find_package(gbm)
set_package_properties(gbm PROPERTIES TYPE OPTIONAL PURPOSE "Required for egl ouput of drm backend.")
set(HAVE_GBM FALSE)
if(HAVE_DRM AND gbm_FOUND)
    set(HAVE_GBM TRUE)
endif()

find_package(libhybris)
set_package_properties(libhybris PROPERTIES TYPE OPTIONAL PURPOSE "Required for libhybris backend")
set(HAVE_LIBHYBRIS ${libhybris_FOUND})

find_package(X11)
set_package_properties(X11 PROPERTIES DESCRIPTION "X11 libraries"
                        URL "http://www.x.org"
                        TYPE REQUIRED
                    )
add_feature_info("XInput" X11_Xinput_FOUND "Required for poll-free mouse cursor updates")
set(HAVE_X11_XINPUT ${X11_Xinput_FOUND})

# All the required XCB components
find_package(XCB 1.10
    REQUIRED COMPONENTS
        XCB
        XFIXES
        DAMAGE
        COMPOSITE
        SHAPE
        SYNC
        RENDER
        RANDR
        KEYSYMS
        IMAGE
        SHM
        GLX
        CURSOR
        ICCCM
)
set_package_properties(XCB PROPERTIES TYPE REQUIRED)

# and the optional XCB dependencies
if (XCB_ICCCM_VERSION VERSION_LESS "0.4")
    set(XCB_ICCCM_FOUND FALSE)
endif()
add_feature_info("XCB-ICCCM" XCB_ICCCM_FOUND "Required for building test applications for KWin")

find_package(X11_XCB)
set_package_properties(X11_XCB PROPERTIES
                       PURPOSE "Required for building X11 windowed backend of kwin_wayland"
                       TYPE OPTIONAL)

# dependencies for QPA plugin
find_package(Qt5FontDatabaseSupport REQUIRED)
find_package(Qt5ThemeSupport REQUIRED)
find_package(Qt5EventDispatcherSupport REQUIRED)

find_package(Freetype REQUIRED)
set_package_properties(Freetype PROPERTIES DESCRIPTION "A font rendering engine"
                       URL "http://www.freetype.org"
                       TYPE REQUIRED
                       PURPOSE "Needed for KWin's QPA plugin."
                      )
find_package(Fontconfig REQUIRED)
set_package_properties(Fontconfig PROPERTIES DESCRIPTION "Font access configuration library"
                       URL "http://www.freedesktop.org/wiki/Software/fontconfig"
                       TYPE REQUIRED
                       PURPOSE "Needed for KWin's QPA plugin."
                      )

find_package(Xwayland)
set_package_properties(Xwayland PROPERTIES
    URL "http://x.org"
    DESCRIPTION "Xwayland X server"
    TYPE RUNTIME
    PURPOSE "Needed for running kwin_wayland"
    )

find_package(Libcap)
set_package_properties(Libcap PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Needed for running kwin_wayland with real-time scheduling policy"
)
set(HAVE_LIBCAP ${Libcap_FOUND})

include(ECMQMLModules)
ecm_find_qmlmodule(QtQuick 2.3)
ecm_find_qmlmodule(QtQuick.Controls 1.2)
ecm_find_qmlmodule(QtQuick.Layouts 1.3)
ecm_find_qmlmodule(QtQuick.VirtualKeyboard 2.1)
ecm_find_qmlmodule(QtQuick.Window 2.1)
ecm_find_qmlmodule(QtMultimedia 5.0)
ecm_find_qmlmodule(org.kde.kquickcontrolsaddons 2.0)
ecm_find_qmlmodule(org.kde.plasma.core 2.0)
ecm_find_qmlmodule(org.kde.plasma.components 2.0)

########### configure tests ###############
include(CMakeDependentOption)

option(KWIN_BUILD_DECORATIONS "Enable building of KWin decorations." ON)
option(KWIN_BUILD_KCMS "Enable building of KWin configuration modules." ON)
option(KWIN_BUILD_TABBOX "Enable building of KWin Tabbox functionality" ON)
option(KWIN_BUILD_XRENDER_COMPOSITING "Enable building of KWin with XRender Compositing support" ON)
cmake_dependent_option(KWIN_BUILD_ACTIVITIES "Enable building of KWin with kactivities support" ON "KF5Activities_FOUND" OFF)

# Binary name of KWin
set(KWIN_NAME "kwin")
set(KWIN_INTERNAL_NAME_X11 "kwin_x11")
set(KWIN_INTERNAL_NAME_WAYLAND "kwin_wayland")
set(KWIN_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# KWIN_HAVE_XRENDER_COMPOSITING - whether XRender-based compositing support is available: may be disabled
if( KWIN_BUILD_XRENDER_COMPOSITING )
    set( KWIN_HAVE_XRENDER_COMPOSITING 1 )
endif()

include_directories(${XKB_INCLUDE_DIR})

include_directories(${epoxy_INCLUDE_DIR})
set(HAVE_EPOXY_GLX ${epoxy_HAS_GLX})

# for things that are also used by kwin libraries
configure_file(libkwineffects/kwinconfig.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/libkwineffects/kwinconfig.h )
# for kwin internal things
set(HAVE_X11_XCB ${X11_XCB_FOUND})

include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckSymbolExists)
check_include_files(unistd.h HAVE_UNISTD_H)
check_include_files(malloc.h HAVE_MALLOC_H)

check_include_file("sys/prctl.h" HAVE_SYS_PRCTL_H)
check_symbol_exists(PR_SET_DUMPABLE "sys/prctl.h" HAVE_PR_SET_DUMPABLE)
check_symbol_exists(PR_SET_PDEATHSIG "sys/prctl.h" HAVE_PR_SET_PDEATHSIG)
check_include_file("sys/procctl.h" HAVE_SYS_PROCCTL_H)
check_symbol_exists(PROC_TRACE_CTL "sys/procctl.h" HAVE_PROC_TRACE_CTL)
if (HAVE_PR_SET_DUMPABLE OR HAVE_PROC_TRACE_CTL)
    set(CAN_DISABLE_PTRACE TRUE)
endif()
add_feature_info("prctl/procctl tracing control"
                 CAN_DISABLE_PTRACE
                 "Required for disallowing ptrace on kwin_wayland process")

check_include_file("sys/sysmacros.h" HAVE_SYS_SYSMACROS_H)
configure_file(config-kwin.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kwin.h )

check_include_file("linux/vt.h" HAVE_LINUX_VT_H)
add_feature_info("linux/vt.h"
                 HAVE_LINUX_VT_H
                 "Required for virtual terminal support under wayland")
check_include_file("linux/fb.h"    HAVE_LINUX_FB_H)
add_feature_info("linux/fb.h"
                 HAVE_LINUX_FB_H
                 "Required for the fbdev backend")

check_symbol_exists(SCHED_RESET_ON_FORK "sched.h" HAVE_SCHED_RESET_ON_FORK)
add_feature_info("SCHED_RESET_ON_FORK"
                 HAVE_SCHED_RESET_ON_FORK
                 "Required for running kwin_wayland with real-time scheduling")

########### global ###############
set(kwin_effects_dbus_xml ${CMAKE_CURRENT_SOURCE_DIR}/org.kde.kwin.Effects.xml)

include_directories(BEFORE
    ${CMAKE_CURRENT_BINARY_DIR}/libkwineffects
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/libkwineffects
    ${CMAKE_CURRENT_SOURCE_DIR}/effects
    ${CMAKE_CURRENT_SOURCE_DIR}/tabbox
    ${CMAKE_CURRENT_SOURCE_DIR}/platformsupport
    )

add_subdirectory( libkwineffects )
if(KWIN_BUILD_KCMS)
    add_subdirectory( kcmkwin )
endif()

add_subdirectory( data )

add_subdirectory( effects )
add_subdirectory( scripts )
add_subdirectory( tabbox )
add_subdirectory(scripting)
add_subdirectory(helpers)

########### next target ###############

set(kwin_KDEINIT_SRCS
   workspace.cpp
   dbusinterface.cpp
   abstract_client.cpp
   client.cpp
   client_machine.cpp
   cursor.cpp
   debug_console.cpp
   tabgroup.cpp
   focuschain.cpp
   globalshortcuts.cpp
   input.cpp
   input_event.cpp
   input_event_spy.cpp
   keyboard_input.cpp
   keyboard_layout.cpp
   keyboard_layout_switching.cpp
   keyboard_repeat.cpp
   pointer_input.cpp
   touch_input.cpp
   netinfo.cpp
   placement.cpp
   atoms.cpp
   utils.cpp
   layers.cpp
   main.cpp
   options.cpp
   outline.cpp
   events.cpp
   killwindow.cpp
   geometrytip.cpp
   screens.cpp
   shadow.cpp
   sm.cpp
   group.cpp
   manage.cpp
   overlaywindow.cpp
   activation.cpp
   useractions.cpp
   geometry.cpp
   rules.cpp
   composite.cpp
   toplevel.cpp
   unmanaged.cpp
   scene.cpp
   screenlockerwatcher.cpp
   thumbnailitem.cpp
   lanczosfilter.cpp
   deleted.cpp
   effects.cpp
   effectloader.cpp
   virtualdesktops.cpp
   xcbutils.cpp
   x11eventfilter.cpp
   logind.cpp
   onscreennotification.cpp
   osd.cpp
    screenedge.cpp
    scripting/scripting.cpp
    scripting/workspace_wrapper.cpp
    scripting/meta.cpp
    scripting/scriptedeffect.cpp
    scripting/scriptingutils.cpp
    scripting/timer.cpp
    scripting/scripting_model.cpp
    scripting/dbuscall.cpp
    scripting/screenedgeitem.cpp
    scripting/scripting_logging.cpp
    decorations/decoratedclient.cpp
    decorations/decorationbridge.cpp
    decorations/decorationpalette.cpp
    decorations/settings.cpp
    decorations/decorationrenderer.cpp
    decorations/decorations_logging.cpp
    platform.cpp
    shell_client.cpp
    wayland_server.cpp
    wayland_cursor_theme.cpp
    virtualkeyboard.cpp
    virtualkeyboard_dbus.cpp
    appmenu.cpp
    modifier_only_shortcuts.cpp
    xkb.cpp
    gestures.cpp
    popup_input_filter.cpp
    colorcorrection/manager.cpp
    colorcorrection/colorcorrectdbusinterface.cpp
    colorcorrection/suncalc.cpp
    abstract_opengl_context_attribute_builder.cpp
    egl_context_attribute_builder.cpp
    was_user_interaction_x11_filter.cpp
    moving_client_x11_filter.cpp
    window_property_notify_x11_filter.cpp
    rootinfo_filter.cpp
    orientation_sensor.cpp
    idle_inhibition.cpp
   )

include(ECMQtDeclareLoggingCategory)
ecm_qt_declare_logging_category(kwin_KDEINIT_SRCS
    HEADER
        colorcorrect_logging.h
    IDENTIFIER
        KWIN_COLORCORRECTION
    CATEGORY_NAME
        kwin_colorcorrection
    DEFAULT_SEVERITY
        Critical
)

if(KWIN_BUILD_TABBOX)
    set(
        kwin_KDEINIT_SRCS ${kwin_KDEINIT_SRCS}
        tabbox/tabbox.cpp
        tabbox/clientmodel.cpp
        tabbox/desktopchain.cpp
        tabbox/desktopmodel.cpp
        tabbox/switcheritem.cpp
        tabbox/tabboxconfig.cpp
        tabbox/tabboxhandler.cpp
        tabbox/tabbox_logging.cpp
        tabbox/x11_filter.cpp
  )
endif()

if(KWIN_BUILD_ACTIVITIES)
    set(
        kwin_KDEINIT_SRCS ${kwin_KDEINIT_SRCS}
        activities.cpp
    )
endif()

if(UDEV_FOUND)
    set(kwin_KDEINIT_SRCS
        ${kwin_KDEINIT_SRCS}
        udev.cpp
    )
endif()

if(HAVE_INPUT)
    set(kwin_KDEINIT_SRCS
        ${kwin_KDEINIT_SRCS}
        libinput/context.cpp
        libinput/connection.cpp
        libinput/device.cpp
        libinput/events.cpp
        libinput/libinput_logging.cpp
    )
    if (HAVE_LINUX_VT_H)
         set(kwin_KDEINIT_SRCS
             ${kwin_KDEINIT_SRCS}
             virtual_terminal.cpp
         )
    endif()
endif()

kconfig_add_kcfg_files(kwin_KDEINIT_SRCS settings.kcfgc)
kconfig_add_kcfg_files(kwin_KDEINIT_SRCS colorcorrection/colorcorrect_settings.kcfgc)

qt5_add_dbus_adaptor( kwin_KDEINIT_SRCS org.kde.KWin.xml dbusinterface.h KWin::DBusInterface )
qt5_add_dbus_adaptor( kwin_KDEINIT_SRCS org.kde.kwin.Compositing.xml dbusinterface.h KWin::CompositorDBusInterface )
qt5_add_dbus_adaptor( kwin_KDEINIT_SRCS org.kde.kwin.ColorCorrect.xml colorcorrection/colorcorrectdbusinterface.h KWin::ColorCorrect::ColorCorrectDBusInterface )
qt5_add_dbus_adaptor( kwin_KDEINIT_SRCS ${kwin_effects_dbus_xml} effects.h KWin::EffectsHandlerImpl )
qt5_add_dbus_adaptor( kwin_KDEINIT_SRCS org.kde.kwin.OrientationSensor.xml orientation_sensor.h KWin::OrientationSensor)

qt5_add_dbus_interface( kwin_KDEINIT_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/org.freedesktop.ScreenSaver.xml screenlocker_interface)

qt5_add_dbus_interface( kwin_KDEINIT_SRCS org.kde.kappmenu.xml appmenu_interface )

qt5_add_resources( kwin_KDEINIT_SRCS resources.qrc )

ki18n_wrap_ui(kwin_KDEINIT_SRCS
    debug_console.ui
    shortcutdialog.ui
)

########### target link libraries ###############

set(kwin_OWN_LIBS
    kwineffects
    kwin4_effect_builtins
)

set(kwin_QT_LIBS
    Qt5::Concurrent
    Qt5::DBus
    Qt5::Quick
    Qt5::Sensors
    Qt5::Script
)

set(kwin_KDE_LIBS
    KF5::ConfigCore
    KF5::CoreAddons
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::GlobalAccelPrivate
    KF5::I18n
    KF5::Notifications
    KF5::Package
    KF5::Plasma
    KF5::WindowSystem
    KF5::QuickAddons
    KDecoration2::KDecoration
    KDecoration2::KDecoration2Private
    PW::KScreenLocker
)

set(kwin_XLIB_LIBS
    ${X11_X11_LIB}
    ${X11_ICE_LIB}
    ${X11_SM_LIB}
)

set(kwin_XCB_LIBS
    XCB::XCB
    XCB::XFIXES
    XCB::DAMAGE
    XCB::COMPOSITE
    XCB::SHAPE
    XCB::SYNC
    XCB::RENDER
    XCB::RANDR
    XCB::KEYSYMS
    XCB::SHM
    XCB::GLX
    XCB::ICCCM
)

set(kwin_WAYLAND_LIBS
    XKB::XKB
    KF5::WaylandClient
    KF5::WaylandServer
    Wayland::Cursor
    ${CMAKE_THREAD_LIBS_INIT}
)

if(KWIN_BUILD_ACTIVITIES)
    set(kwin_KDE_LIBS ${kwin_KDE_LIBS} KF5::Activities)
endif()

set(kwinLibs
    ${kwin_OWN_LIBS}
    ${kwin_QT_LIBS}
    ${kwin_KDE_LIBS}
    ${kwin_XLIB_LIBS}
    ${kwin_XCB_LIBS}
    ${kwin_WAYLAND_LIBS}
)

if(UDEV_FOUND)
    set(kwinLibs ${kwinLibs} ${UDEV_LIBS})
endif()

if(HAVE_INPUT)
    set(kwinLibs ${kwinLibs} Libinput::Libinput)
endif()

add_library(kwin SHARED ${kwin_KDEINIT_SRCS})

set_target_properties(kwin PROPERTIES
   VERSION ${PROJECT_VERSION}
   SOVERSION ${PROJECT_VERSION_MAJOR}
   )

target_link_libraries(kwin ${kwinLibs})
generate_export_header(kwin EXPORT_FILE_NAME kwin_export.h)

target_link_libraries(kwin kwinglutils ${epoxy_LIBRARY})

kf5_add_kdeinit_executable(kwin_x11 main_x11.cpp)
target_link_libraries(kdeinit_kwin_x11 kwin KF5::Crash Qt5::X11Extras)

install(TARGETS kwin             ${INSTALL_TARGETS_DEFAULT_ARGS} LIBRARY NAMELINK_SKIP )
install(TARGETS kdeinit_kwin_x11 ${INSTALL_TARGETS_DEFAULT_ARGS} )
install(TARGETS kwin_x11         ${INSTALL_TARGETS_DEFAULT_ARGS} )

add_executable(kwin_wayland tabletmodemanager.cpp main_wayland.cpp)
target_link_libraries(kwin_wayland kwin)
if (HAVE_LIBCAP)
    target_link_libraries(kwin_wayland ${Libcap_LIBRARIES})
endif()

install(TARGETS kwin_wayland         ${INSTALL_TARGETS_DEFAULT_ARGS} )
if (HAVE_LIBCAP)
    install(
    CODE "execute_process(
            COMMAND
                ${SETCAP_EXECUTABLE}
                CAP_SYS_NICE=+ep
                \$ENV{DESTDIR}${CMAKE_INSTALL_FULL_BINDIR}/kwin_wayland)"
    )
endif()

add_subdirectory(platformsupport)
add_subdirectory(plugins)

########### install files ###############

install( FILES kwin.kcfg  DESTINATION  ${KCFG_INSTALL_DIR} RENAME ${KWIN_NAME}.kcfg )
install( FILES colorcorrection/colorcorrect_settings.kcfg  DESTINATION  ${KCFG_INSTALL_DIR} RENAME ${KWIN_NAME}_colorcorrect.kcfg )
install( FILES kwin.notifyrc  DESTINATION  ${KNOTIFYRC_INSTALL_DIR} RENAME ${KWIN_NAME}.notifyrc)
install(
    FILES
        org.kde.KWin.xml
        org.kde.kwin.Compositing.xml
        org.kde.kwin.ColorCorrect.xml
        org.kde.kwin.Effects.xml
    DESTINATION
        ${KDE_INSTALL_DBUSINTERFACEDIR}
)
install( FILES ${CMAKE_CURRENT_BINARY_DIR}/kwin_export.h DESTINATION ${INCLUDE_INSTALL_DIR} COMPONENT Devel)

# Install the KWin/Script service type
install( FILES scripting/kwinscript.desktop DESTINATION ${SERVICETYPES_INSTALL_DIR} )

ecm_install_icons(
    ICONS
        16-apps-kwin.png
        32-apps-kwin.png
        48-apps-kwin.png
        sc-apps-kwin.svgz
    DESTINATION
        ${ICON_INSTALL_DIR}
    THEME
        hicolor
)

add_subdirectory(qml)
add_subdirectory(autotests)
add_subdirectory(tests)
add_subdirectory(packageplugins)

if (KF5DocTools_FOUND)
    add_subdirectory(doc)
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

include(CMakePackageConfigHelpers)
set(CMAKECONFIG_INSTALL_DIR "${CMAKECONFIG_INSTALL_PREFIX}/KWinDBusInterface")
configure_package_config_file(KWinDBusInterfaceConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/KWinDBusInterfaceConfig.cmake"
    PATH_VARS KDE_INSTALL_DBUSINTERFACEDIR
    INSTALL_DESTINATION ${CMAKECONFIG_INSTALL_DIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/KWinDBusInterfaceConfig.cmake
    DESTINATION ${CMAKECONFIG_INSTALL_DIR})
