#pragma once
#include <QLatin1String>

#define KWIN_PLUGIN_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"

#cmakedefine01 KWIN_BUILD_DECORATIONS
#cmakedefine01 KWIN_BUILD_KCMS
#cmakedefine01 KWIN_BUILD_NOTIFICATIONS
#cmakedefine01 KWIN_BUILD_SCREENLOCKER
#cmakedefine01 KWIN_BUILD_TABBOX
#cmakedefine01 KWIN_BUILD_ACTIVITIES
#cmakedefine01 KWIN_BUILD_GLOBALSHORTCUTS
#cmakedefine01 KWIN_BUILD_X11
constexpr QLatin1String KWIN_CONFIG("kwinrc");
constexpr QLatin1String KWIN_VERSION_STRING("${PROJECT_VERSION}");
constexpr QLatin1String XCB_VERSION_STRING("${XCB_VERSION}");
constexpr QLatin1String KWIN_KILLER_BIN("${KWIN_KILLER_BIN}");
#cmakedefine01 HAVE_X11_XCB
#cmakedefine01 HAVE_X11_XINPUT
#cmakedefine01 HAVE_GBM_BO_GET_FD_FOR_PLANE
#cmakedefine01 HAVE_GBM_BO_CREATE_WITH_MODIFIERS2
#cmakedefine01 HAVE_MEMFD
#cmakedefine01 HAVE_BREEZE_DECO
#cmakedefine01 HAVE_SCHED_RESET_ON_FORK
#cmakedefine01 HAVE_ACCESSIBILITY
#cmakedefine01 HAVE_XKBCOMMON_NO_SECURE_GETENV
#if HAVE_BREEZE_DECO
constexpr QLatin1String BREEZE_KDECORATION_PLUGIN_ID("${BREEZE_KDECORATION_PLUGIN_ID}");
#endif
#cmakedefine01 HAVE_XWAYLAND_ENABLE_EI_PORTAL
#cmakedefine01 HAVE_GLX
#cmakedefine01 HAVE_DL_LIBRARY
#cmakedefine01 HAVE_WL_DISPLAY_SET_DEFAULT_MAX_BUFFER_SIZE
#cmakedefine01 HAVE_WL_FIXES

constexpr QLatin1String XWAYLAND_SESSION_SCRIPTS("${XWAYLAND_SESSION_SCRIPTS}");

#cmakedefine01 HAVE_LIBINPUT_INPUT_AREA
