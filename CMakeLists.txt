cmake_minimum_required(VERSION 3.16)

set(PROJECT_VERSION "5.27.11") # Handled by release scripts
project(KWin VERSION ${PROJECT_VERSION})

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_MIN_VERSION "5.15.2")
set(KF5_MIN_VERSION "5.102.0")
set(KDE_COMPILERSETTINGS_LEVEL "5.82")

find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${ECM_MODULE_PATH})

include(CMakeDependentOption)
include(CMakePackageConfigHelpers)
include(FeatureSummary)
include(WriteBasicConfigVersionFile)
include(GenerateExportHeader)
include(CheckCXXCompilerFlag)
include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckSymbolExists)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDEClangFormat)
include(KDEGitCommitHooks)

include(ECMFindQmlModule)
include(ECMInstallIcons)
include(ECMOptionalAddSubdirectory)
include(ECMConfiguredInstall)
include(ECMQtDeclareLoggingCategory)

find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Concurrent
    Core
    DBus
    Network
    Quick
    UiTools
    Widgets
)
if (QT_MAJOR_VERSION EQUAL "5")
    find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS X11Extras)
    find_package(Qt5XkbCommonSupport REQUIRED)
else()
    find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS WaylandClient ShaderTools)
    if (Qt6WaylandClient_VERSION VERSION_LESS "6.4.1") # TODO Plasma 6: Drop once minimum Qt version is 6.4.1+
        include(Qt6WaylandClientMacrosKde)
    endif()
endif()

find_package(Qt${QT_MAJOR_VERSION}Test ${QT_MIN_VERSION} CONFIG QUIET)
set_package_properties(Qt${QT_MAJOR_VERSION}Test PROPERTIES
    PURPOSE "Required for tests"
    TYPE OPTIONAL
)
add_feature_info("Qt${QT_MAJOR_VERSION}Test" Qt${QT_MAJOR_VERSION}Test_FOUND "Required for building tests")
if (NOT Qt${QT_MAJOR_VERSION}Test_FOUND)
    set(BUILD_TESTING OFF CACHE BOOL "Build the testing tree.")
endif()

# required frameworks by Core
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS
    Auth
    Config
    ConfigWidgets
    CoreAddons
    Crash
    DBusAddons
    GlobalAccel
    I18n
    IdleTime
    Package
    Plasma
    WidgetsAddons
    WindowSystem
)
# required frameworks by config modules
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS
    Declarative
    KCMUtils
    NewStuff
    Service
    XmlGui
)

find_package(Threads)
set_package_properties(Threads PROPERTIES
    PURPOSE "Needed for VirtualTerminal support in KWin Wayland"
    TYPE REQUIRED
)

find_package(KF5Wayland ${KF5_MIN_VERSION} CONFIG)
set_package_properties(KF5Wayland PROPERTIES
    PURPOSE "Required to build wayland platform plugin and tests"
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

find_package(KF5Runner ${KF5_MIN_VERSION} CONFIG)
set_package_properties(KF5Runner PROPERTIES
    PURPOSE "Enable building of KWin with krunner support"
    TYPE OPTIONAL
    )
add_feature_info("KF5Runner" KF5Runner_FOUND "Enable building of KWin with krunner support")

find_package(KF5Kirigami2 ${KF5_MIN_VERSION} CONFIG)
set_package_properties(KF5Kirigami2 PROPERTIES
    DESCRIPTION "A QtQuick based components set"
    PURPOSE "Required at runtime for Virtual desktop KCM"
    TYPE RUNTIME
)

find_package(KDecoration2 ${PROJECT_VERSION} CONFIG)
set_package_properties(KDecoration2 PROPERTIES
    TYPE REQUIRED
    PURPOSE "Required for server side decoration support"
)

find_package(Breeze 5.9.0 CONFIG)
set_package_properties(Breeze PROPERTIES
    TYPE OPTIONAL
    PURPOSE "For setting the default window decoration plugin"
)
if (${Breeze_FOUND})
    if (${BREEZE_WITH_KDECORATION})
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
    TYPE REQUIRED
    PURPOSE "Required to build KWin with EGL support"
)

find_package(epoxy 1.3)
set_package_properties(epoxy PROPERTIES
    DESCRIPTION "libepoxy"
    URL "https://github.com/anholt/libepoxy"
    TYPE REQUIRED
    PURPOSE "OpenGL dispatch library"
)

set(HAVE_DL_LIBRARY FALSE)
if (epoxy_HAS_GLX)
    find_library(DL_LIBRARY dl)
    if (DL_LIBRARY)
        set(HAVE_DL_LIBRARY TRUE)
    endif()
endif()

check_cxx_source_compiles("
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

int main() {
    const int size = 10;
    int fd = memfd_create(\"test\", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    ftruncate(fd, size);
    fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE | F_SEAL_SEAL);
    mmap(nullptr, size, PROT_WRITE, MAP_SHARED, fd, 0);
}" HAVE_MEMFD)

find_package(Wayland 1.21)
set_package_properties(Wayland PROPERTIES
    TYPE REQUIRED
    PURPOSE "Required for building KWin with Wayland support"
)

find_package(WaylandProtocols 1.31)
set_package_properties(WaylandProtocols PROPERTIES
    TYPE REQUIRED
    PURPOSE "Collection of Wayland protocols that add functionality not available in the Wayland core protocol"
    URL "https://gitlab.freedesktop.org/wayland/wayland-protocols/"
)

find_package(PlasmaWaylandProtocols 1.9.0 CONFIG)
set_package_properties(PlasmaWaylandProtocols PROPERTIES
    TYPE REQUIRED
    PURPOSE "Collection of Plasma-specific Wayland protocols"
    URL "https://invent.kde.org/libraries/plasma-wayland-protocols/"
)

if (QT_MAJOR_VERSION EQUAL "5")
    find_package(QtWaylandScanner)
    set_package_properties(QtWaylandScanner PROPERTIES
        TYPE REQUIRED
        PURPOSE "Required for building KWin with Wayland support"
    )
endif()

find_package(XKB 0.7.0)
set_package_properties(XKB PROPERTIES
    TYPE REQUIRED
    PURPOSE "Required for building KWin with Wayland support"
)
if (XKB_VERSION VERSION_GREATER_EQUAL 1.5.0)
    set(HAVE_XKBCOMMON_NO_SECURE_GETENV 1)
else()
    set(HAVE_XKBCOMMON_NO_SECURE_GETENV 0)
endif()

pkg_check_modules(XKBX11 IMPORTED_TARGET xkbcommon-x11 REQUIRED)
add_feature_info(XKBX11 XKBX11_FOUND "Required for handling keyboard events in X11 backend")

find_package(Libinput 1.19)
set_package_properties(Libinput PROPERTIES TYPE REQUIRED PURPOSE "Required for input handling on Wayland.")

find_package(UDev)
set_package_properties(UDev PROPERTIES
    URL "https://www.freedesktop.org/wiki/Software/systemd/"
    DESCRIPTION "Linux device library."
    TYPE REQUIRED
    PURPOSE "Required for input handling on Wayland."
)

find_package(Libdrm 2.4.112)
set_package_properties(Libdrm PROPERTIES TYPE REQUIRED PURPOSE "Required for drm output on Wayland.")

find_package(gbm)
set_package_properties(gbm PROPERTIES TYPE REQUIRED PURPOSE "Required for egl output of drm backend.")
if (gbm_VERSION VERSION_GREATER_EQUAL 21.1)
    set(HAVE_GBM_BO_GET_FD_FOR_PLANE 1)
else()
    set(HAVE_GBM_BO_GET_FD_FOR_PLANE 0)
endif()
if (gbm_VERSION VERSION_GREATER_EQUAL 21.3)
    set(HAVE_GBM_BO_CREATE_WITH_MODIFIERS2 1)
else()
    set(HAVE_GBM_BO_CREATE_WITH_MODIFIERS2 0)
endif()

pkg_check_modules(Libxcvt IMPORTED_TARGET libxcvt>=0.1.1 REQUIRED)
add_feature_info(Libxcvt Libxcvt_FOUND "Required for generating modes in the drm backend")

find_package(X11)
set_package_properties(X11 PROPERTIES
    DESCRIPTION "X11 libraries"
    URL "https://www.x.org"
    TYPE REQUIRED
)
add_feature_info("XInput" X11_Xi_FOUND "Required for poll-free mouse cursor updates")
set(HAVE_X11_XINPUT ${X11_Xinput_FOUND})

find_package(lcms2)
set_package_properties(lcms2 PROPERTIES
    DESCRIPTION "Small-footprint color management engine"
    URL "http://www.littlecms.com"
    TYPE REQUIRED
    PURPOSE "Required for the color management system"
)

# All the required XCB components
find_package(XCB 1.10 REQUIRED COMPONENTS
    COMPOSITE
    CURSOR
    DAMAGE
    GLX
    ICCCM
    IMAGE
    KEYSYMS
    PRESENT
    RANDR
    RENDER
    SHAPE
    SHM
    SYNC
    XCB
    XFIXES
    XKB
    XINERAMA
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
    TYPE OPTIONAL
)

# dependencies for QPA plugin
if (QT_MAJOR_VERSION EQUAL "5")
    find_package(Qt5FontDatabaseSupport REQUIRED)
    find_package(Qt5ThemeSupport REQUIRED)
    find_package(Qt5ServiceSupport REQUIRED)
    find_package(Qt5EventDispatcherSupport REQUIRED)
endif()

find_package(Freetype)
set_package_properties(Freetype PROPERTIES
    DESCRIPTION "A font rendering engine"
    URL "https://www.freetype.org"
    TYPE REQUIRED
    PURPOSE "Needed for KWin's QPA plugin."
)
find_package(Fontconfig)
set_package_properties(Fontconfig PROPERTIES
    TYPE REQUIRED
    PURPOSE "Needed for KWin's QPA plugin."
)

find_package(Xwayland)
set_package_properties(Xwayland PROPERTIES
    URL "https://x.org"
    DESCRIPTION "Xwayland X server"
    TYPE RUNTIME
    PURPOSE "Needed for running kwin_wayland"
)
set(HAVE_XWAYLAND_LISTENFD ${Xwayland_HAVE_LISTENFD})

find_package(Libcap)
set_package_properties(Libcap PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Needed for running kwin_wayland with real-time scheduling policy"
)
set(HAVE_LIBCAP ${Libcap_FOUND})

find_package(hwdata)
set_package_properties(hwdata PROPERTIES
    TYPE RUNTIME
    PURPOSE "Runtime-only dependency needed for mapping monitor hardware vendor IDs to full names"
    URL "https://github.com/vcrhonek/hwdata"
)

find_package(QAccessibilityClient CONFIG)
set_package_properties(QAccessibilityClient PROPERTIES
    URL "https://www.kde.org"
    DESCRIPTION "KDE client-side accessibility library"
    TYPE OPTIONAL
    PURPOSE "Required to enable accessibility features"
)
set(HAVE_ACCESSIBILITY ${QAccessibilityClient_FOUND})

find_package(Qt5LinuxAccessibilitySupport REQUIRED)

ecm_find_qmlmodule(QtQuick 2.3)
ecm_find_qmlmodule(QtQuick.Controls 2.15)
ecm_find_qmlmodule(QtQuick.Layouts 1.3)
ecm_find_qmlmodule(QtQuick.Window 2.1)
ecm_find_qmlmodule(QtMultimedia 5.0)
ecm_find_qmlmodule(org.kde.kquickcontrolsaddons 2.0)
ecm_find_qmlmodule(org.kde.plasma.core 2.0)
ecm_find_qmlmodule(org.kde.plasma.components 2.0)

########### configure tests ###############
option(KWIN_BUILD_DECORATIONS "Enable building of KWin decorations." ON)
option(KWIN_BUILD_KCMS "Enable building of KWin configuration modules." ON)
option(KWIN_BUILD_NOTIFICATIONS "Enable building of KWin with knotifications support" ON)
option(KWIN_BUILD_SCREENLOCKER "Enable building of KWin lockscreen functionality" ON)
option(KWIN_BUILD_TABBOX "Enable building of KWin Tabbox functionality" ON)
cmake_dependent_option(KWIN_BUILD_ACTIVITIES "Enable building of KWin with kactivities support" ON "KF5Activities_FOUND" OFF)
cmake_dependent_option(KWIN_BUILD_RUNNERS "Enable building of KWin with krunner support" ON "KF5Runner_FOUND" OFF)

set(HAVE_EPOXY_GLX ${epoxy_HAS_GLX})

# for kwin internal things
set(HAVE_X11_XCB ${X11_XCB_FOUND})

check_symbol_exists(SCHED_RESET_ON_FORK "sched.h" HAVE_SCHED_RESET_ON_FORK)
add_feature_info("SCHED_RESET_ON_FORK"
                 HAVE_SCHED_RESET_ON_FORK
                 "Required for running kwin_wayland with real-time scheduling")


pkg_check_modules(PipeWire IMPORTED_TARGET libpipewire-0.3>=0.3.29)
add_feature_info(PipeWire PipeWire_FOUND "Required for Wayland screencasting")

if (KWIN_BUILD_NOTIFICATIONS)
    find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS Notifications)
endif()

if (KWIN_BUILD_SCREENLOCKER)
    find_package(KScreenLocker CONFIG)
    set_package_properties(KScreenLocker PROPERTIES
        TYPE REQUIRED
        PURPOSE "For screenlocker integration in kwin_wayland"
    )
endif()

########### global ###############

include_directories(BEFORE
    ${CMAKE_CURRENT_BINARY_DIR}/src/libkwineffects
    ${CMAKE_CURRENT_BINARY_DIR}/src/wayland
    ${CMAKE_CURRENT_BINARY_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libkwineffects
    ${CMAKE_CURRENT_SOURCE_DIR}/src/effects
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tabbox
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platformsupport
    ${CMAKE_CURRENT_SOURCE_DIR}/src/colors
)

check_cxx_compiler_flag(-Wno-unused-parameter COMPILER_UNUSED_PARAMETER_SUPPORTED)
if (COMPILER_UNUSED_PARAMETER_SUPPORTED)
    add_compile_options(-Wno-unused-parameter)
endif()

add_definitions(
    -DQT_DISABLE_DEPRECATED_BEFORE=0
    -DQT_NO_KEYWORDS
    -DQT_USE_QSTRINGBUILDER
    -DQT_NO_URL_CAST_FROM_STRING
    -DQT_NO_CAST_TO_ASCII
    -DQT_NO_FOREACH

    # Prevent EGL headers from including platform headers, in particular Xlib.h.
    -DMESA_EGL_NO_X11_HEADERS
    -DEGL_NO_X11
    -DEGL_NO_PLATFORM_SPECIFIC_TYPES
)

if (KF5DocTools_FOUND)
    add_subdirectory(doc)
    kdoctools_install(po)
endif()

add_subdirectory(data)
add_subdirectory(kconf_update)
add_subdirectory(src)

if (BUILD_TESTING)
    find_package(Wayland REQUIRED COMPONENTS Client)

    add_subdirectory(autotests)
    add_subdirectory(tests)
endif()

# add clang-format target for all our real source files
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.cpp *.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

set(CMAKECONFIG_INSTALL_DIR "${KDE_INSTALL_CMAKEPACKAGEDIR}/KWinDBusInterface")
configure_package_config_file(KWinDBusInterfaceConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/KWinDBusInterfaceConfig.cmake"
    PATH_VARS KDE_INSTALL_DBUSINTERFACEDIR
    INSTALL_DESTINATION ${CMAKECONFIG_INSTALL_DIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/KWinDBusInterfaceConfig.cmake
    DESTINATION ${CMAKECONFIG_INSTALL_DIR})

ecm_install_configured_files(INPUT plasma-kwin_x11.service.in plasma-kwin_wayland.service.in @ONLY
            DESTINATION ${KDE_INSTALL_SYSTEMDUSERUNITDIR})

ki18n_install(po)
