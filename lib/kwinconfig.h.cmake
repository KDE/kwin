/*

 This file includes config #define's for KWin's libraries
 that are installed. Installed files and files using them
 should be using these instead of their own.

*/

#ifndef KWINCONFIG_H
#define KWINCONFIG_H

#if ${HAVE_OPENGL}
#define KWIN_HAVE_OPENGL 1
#else
#undef KWIN_HAVE_OPENGL
#endif

#if ${HAVE_XRENDER}
#define KWIN_HAVE_XRENDER 1
#else
#undef KWIN_HAVE_XRENDER
#endif

#if ${HAVE_XFIXES}
#define KWIN_HAVE_XFIXES 1
#else
#undef KWIN_HAVE_XFIXES
#endif

#if ${HAVE_XDAMAGE}
#define KWIN_HAVE_XDAMAGE 1
#else
#undef KWIN_HAVE_XDAMAGE
#endif

#if ${HAVE_XCOMPOSITE}
#define KWIN_HAVE_XCOMPOSITE 1
#else
#undef KWIN_HAVE_XCOMPOSITE
#endif

/*
 
 These should be primarily used to detect what kind of compositing
 support is available.

*/

/* KWIN_HAVE_COMPOSITING - whether any compositing support is available */
#if defined( KWIN_HAVE_XCOMPOSITE ) && defined( KWIN_HAVE_XDAMAGE )
#define KWIN_HAVE_COMPOSITING 1
#else
#undef KWIN_HAVE_COMPOSITING
#endif

/* KWIN_HAVE_OPENGL_COMPOSITING - whether OpenGL-based compositing support is available */
#if defined( KWIN_HAVE_COMPOSITING ) && defined( KWIN_HAVE_OPENGL )
#define KWIN_HAVE_OPENGL_COMPOSITING 1
#else
#undef KWIN_HAVE_OPENGL_COMPOSITING
#endif

/* KWIN_HAVE_XRENDER_COMPOSITING - whether XRender-based compositing support is available */
#if defined( KWIN_HAVE_COMPOSITING ) && defined( KWIN_HAVE_XRENDER ) && defined( KWIN_HAVE_XFIXES )
#define KWIN_HAVE_XRENDER_COMPOSITING 1
#else
#undef KWIN_HAVE_XRENDER_COMPOSITING
#endif

#if !defined( KWIN_HAVE_OPENGL_COMPOSITING ) && !defined( KWIN_HAVE_XRENDER_COMPOSITING )
#undef KWIN_HAVE_COMPOSITING
#endif

#endif
