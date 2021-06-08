/*

 This file includes config #define's for KWin's libraries
 that are installed. Installed files and files using them
 should be using these instead of their own.

*/

#ifndef KWINCONFIG_H
#define KWINCONFIG_H

#define KWIN_PLUGIN_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"

/*

 These should be primarily used to detect what kind of compositing
 support is available.

*/

#cmakedefine01 HAVE_EPOXY_GLX

#cmakedefine01 HAVE_DL_LIBRARY

#endif
