#cmakedefine KWIN_BUILD_DECORATIONS 1
#cmakedefine KWIN_BUILD_TABBOX 1
#cmakedefine KWIN_BUILD_DESKTOPCHANGEOSD 1
#cmakedefine KWIN_BUILD_SCREENEDGES 1
#cmakedefine KWIN_BUILD_KAPPMENU 1
#cmakedefine KWIN_BUILD_ACTIVITIES 1
#define KWIN_NAME "${KWIN_NAME}"
#define KWIN_CONFIG "${KWIN_NAME}rc"
#define KWIN_VERSION_STRING "${KDE4WORKSPACE_VERSION}"
#cmakedefine01 HAVE_WAYLAND
#cmakedefine01 HAVE_WAYLAND_EGL
#cmakedefine01 HAVE_XKB

/* Define to 1 if you have the <unistd.h> header file. */
#cmakedefine HAVE_UNISTD_H 1

/* Define to 1 if you have the <malloc.h> header file. */
#cmakedefine HAVE_MALLOC_H 1
