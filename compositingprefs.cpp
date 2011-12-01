/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "compositingprefs.h"

#include "kwinglobals.h"
#include "kwinglplatform.h"

#include <kconfiggroup.h>
#include <kdebug.h>
#include <kxerrorhandler.h>
#include <klocale.h>
#include <kdeversion.h>
#include <ksharedconfig.h>
#include <kstandarddirs.h>

#include <qprocess.h>


namespace KWin
{

CompositingPrefs::CompositingPrefs()
    : mRecommendCompositing(false)
    , mEnableVSync(true)
    , mEnableDirectRendering(true)
    , mStrictBinding(true)
{
}

CompositingPrefs::~CompositingPrefs()
{
}

bool CompositingPrefs::recommendCompositing() const
{
    return mRecommendCompositing;
}

bool CompositingPrefs::openGlIsBroken()
{
    KSharedConfigPtr config = KSharedConfig::openConfig("kwinrc");
    return KConfigGroup(config, "Compositing").readEntry("OpenGLIsUnsafe", false);
}

bool CompositingPrefs::compositingPossible()
{
    // first off, check whether we figured that we'll crash on detection because of a buggy driver
    KSharedConfigPtr config = KSharedConfig::openConfig("kwinrc");
    KConfigGroup gl_workaround_group(config, "Compositing");
    if (gl_workaround_group.readEntry("Backend", "OpenGL") == "OpenGL" &&
        gl_workaround_group.readEntry("OpenGLIsUnsafe", false))
        return false;

    Extensions::init();
    if (!Extensions::compositeAvailable()) {
        kDebug(1212) << "No composite extension available";
        return false;
    }
    if (!Extensions::damageAvailable()) {
        kDebug(1212) << "No damage extension available";
        return false;
    }
    if (hasGlx())
        return true;
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (Extensions::renderAvailable() && Extensions::fixesAvailable())
        return true;
#endif
#ifdef KWIN_HAVE_OPENGLES
    return true;
#endif
    kDebug(1212) << "No OpenGL or XRender/XFixes support";
    return false;
}

QString CompositingPrefs::compositingNotPossibleReason()
{
    // first off, check whether we figured that we'll crash on detection because of a buggy driver
    KSharedConfigPtr config = KSharedConfig::openConfig("kwinrc");
    KConfigGroup gl_workaround_group(config, "Compositing");
    if (gl_workaround_group.readEntry("Backend", "OpenGL") == "OpenGL" &&
        gl_workaround_group.readEntry("OpenGLIsUnsafe", false))
        return i18n("<b>OpenGL compositing (the default) has crashed KWin in the past.</b><br>"
                    "This was most likely due to a driver bug."
                    "<p>If you think that you have meanwhile upgraded to a stable driver,<br>"
                    "you can reset this protection but <b>be aware that this might result in an immediate crash!</b></p>"
                    "<p>Alternatively, you might want to use the XRender backend instead.</p>");

    Extensions::init();
    if (!Extensions::compositeAvailable() || !Extensions::damageAvailable()) {
        return i18n("Required X extensions (XComposite and XDamage) are not available.");
    }
#if !defined( KWIN_HAVE_XRENDER_COMPOSITING )
    if (!hasGlx())
        return i18n("GLX/OpenGL are not available and only OpenGL support is compiled.");
#else
    if (!(hasGlx()
            || (Extensions::renderAvailable() && Extensions::fixesAvailable()))) {
        return i18n("GLX/OpenGL and XRender/XFixes are not available.");
    }
#endif
    return QString();
}

static bool s_glxDetected = false;
static bool s_hasGlx = false;

bool CompositingPrefs::hasGlx()
{
    if (s_glxDetected) {
        return s_hasGlx;
    }
#ifndef KWIN_HAVE_OPENGLES
    int event_base, error_base;
    s_hasGlx = glXQueryExtension(display(), &event_base, &error_base);
#endif
    s_glxDetected = true;
    return s_hasGlx;
}

void CompositingPrefs::detect()
{
    if (!compositingPossible() || openGlIsBroken()) {
        return;
    }

    // NOTICE: this is intended to workaround broken GL implementations that successfully segfault
    // on glXQuery :-(
    // we tag GL as unsafe. It *must* be reset before every return, and in case we "unexpectedly"
    // end (aka "segfaulted") we know that we shall not try again
    KSharedConfigPtr config = KSharedConfig::openConfig("kwinrc");
    KConfigGroup gl_workaround_config = KConfigGroup(config, "Compositing");
    gl_workaround_config.writeEntry("OpenGLIsUnsafe", true);
    gl_workaround_config.sync();

#ifdef KWIN_HAVE_OPENGLES
    bool haveContext = false;
    bool canDetect = false;
    EGLDisplay dpy = eglGetCurrentDisplay();
    if (dpy != EGL_NO_DISPLAY) {
        EGLContext ctx = eglGetCurrentContext();
        if (ctx != EGL_NO_CONTEXT) {
            haveContext = true;
            canDetect = true;
        }
    }
    if (!haveContext) {
        canDetect = initEGLContext();
    }
    if (canDetect) {
        detectDriverAndVersion();
        applyDriverSpecificOptions();
    }
    if (!haveContext) {
        deleteEGLContext();
    }
#else
    // HACK: This is needed for AIGLX
    const bool forceIndirect = qstrcmp(qgetenv("LIBGL_ALWAYS_INDIRECT"), "1") == 0;
    if (!forceIndirect && qstrcmp(qgetenv("KWIN_DIRECT_GL"), "1") != 0) {
        // Start an external helper program that initializes GLX and returns
        // 0 if we can use direct rendering, and 1 otherwise.
        // The reason we have to use an external program is that after GLX
        // has been initialized, it's too late to set the LIBGL_ALWAYS_INDIRECT
        // environment variable.
        // Direct rendering is preferred, since not all OpenGL extensions are
        // available with indirect rendering.
        const QString opengl_test = KStandardDirs::findExe("kwin_opengl_test");
        if (QProcess::execute(opengl_test) != 0) {
            mEnableDirectRendering = false;
            setenv("LIBGL_ALWAYS_INDIRECT", "1", true);
        } else {
            mEnableDirectRendering = true;
        }
    } else {
        mEnableDirectRendering = !forceIndirect;
    }
    if (!hasGlx()) {
        kDebug(1212) << "No GLX available";
        gl_workaround_config.writeEntry("OpenGLIsUnsafe", false);
        gl_workaround_config.sync();
        return;
    }
    int glxmajor, glxminor;
    glXQueryVersion(display(), &glxmajor, &glxminor);
    kDebug(1212) << "glx version is " << glxmajor << "." << glxminor;
    bool hasglx13 = (glxmajor > 1 || (glxmajor == 1 && glxminor >= 3));

    // remember and later restore active context
    GLXContext oldcontext = glXGetCurrentContext();
    GLXDrawable olddrawable = glXGetCurrentDrawable();
    GLXDrawable oldreaddrawable = None;
    if (hasglx13)
        oldreaddrawable = glXGetCurrentReadDrawable();

    if (initGLXContext()) {
        detectDriverAndVersion();
        applyDriverSpecificOptions();
    }
    if (hasglx13)
        glXMakeContextCurrent(display(), olddrawable, oldreaddrawable, oldcontext);
    else
        glXMakeCurrent(display(), olddrawable, oldcontext);
    deleteGLXContext();
#endif
    gl_workaround_config.writeEntry("OpenGLIsUnsafe", false);
    gl_workaround_config.sync();
}

bool CompositingPrefs::initGLXContext()
{
#ifndef KWIN_HAVE_OPENGLES
    mGLContext = NULL;
    KXErrorHandler handler;
    // Most of this code has been taken from glxinfo.c
    QVector<int> attribs;
    attribs << GLX_RGBA;
    attribs << GLX_RED_SIZE << 1;
    attribs << GLX_GREEN_SIZE << 1;
    attribs << GLX_BLUE_SIZE << 1;
    attribs << None;

    XVisualInfo* visinfo = glXChooseVisual(display(), DefaultScreen(display()), attribs.data());
    if (!visinfo) {
        attribs.last() = GLX_DOUBLEBUFFER;
        attribs << None;
        visinfo = glXChooseVisual(display(), DefaultScreen(display()), attribs.data());
        if (!visinfo) {
            kDebug(1212) << "Error: couldn't find RGB GLX visual";
            return false;
        }
    }

    mGLContext = glXCreateContext(display(), visinfo, NULL, True);
    if (!mGLContext) {
        kDebug(1212) << "glXCreateContext failed";
        XDestroyWindow(display(), mGLWindow);
        return false;
    }

    XSetWindowAttributes attr;
    attr.background_pixel = 0;
    attr.border_pixel = 0;
    attr.colormap = XCreateColormap(display(), rootWindow(), visinfo->visual, AllocNone);
    attr.event_mask = StructureNotifyMask | ExposureMask;
    unsigned long mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;
    int width = 100, height = 100;
    mGLWindow = XCreateWindow(display(), rootWindow(), 0, 0, width, height,
                              0, visinfo->depth, InputOutput,
                              visinfo->visual, mask, &attr);

    return glXMakeCurrent(display(), mGLWindow, mGLContext) && !handler.error(true);
#else
    return false;
#endif
}

void CompositingPrefs::deleteGLXContext()
{
#ifndef KWIN_HAVE_OPENGLES
    if (mGLContext == NULL)
        return;
    glXDestroyContext(display(), mGLContext);
    XDestroyWindow(display(), mGLWindow);
#endif
}

bool CompositingPrefs::initEGLContext()
{
#ifdef KWIN_HAVE_OPENGLES
    mEGLDisplay = eglGetDisplay(display());
    if (mEGLDisplay == EGL_NO_DISPLAY) {
        return false;
    }
    if (eglInitialize(mEGLDisplay, 0, 0) == EGL_FALSE) {
        return false;
    }
    eglBindAPI(EGL_OPENGL_ES_API);

    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE,         EGL_WINDOW_BIT,
        EGL_RED_SIZE,             1,
        EGL_GREEN_SIZE,           1,
        EGL_BLUE_SIZE,            1,
        EGL_ALPHA_SIZE,           0,
        EGL_RENDERABLE_TYPE,      EGL_OPENGL_ES2_BIT,
        EGL_CONFIG_CAVEAT,        EGL_NONE,
        EGL_NONE,
    };

    EGLint count;
    EGLConfig configs[1024];
    eglChooseConfig(mEGLDisplay, config_attribs, configs, 1024, &count);

    if (count == 0) {
        // egl_glx only supports indirect rendering, which itself does not allow GLES2
        kWarning(1212) << "You might be using mesa egl_glx, which is not supported!";
        return false;
    }

    EGLint visualId = XVisualIDFromVisual((Visual*)QX11Info::appVisual());

    EGLConfig config = configs[0];
    for (int i = 0; i < count; i++) {
        EGLint val;
        eglGetConfigAttrib(mEGLDisplay, configs[i], EGL_NATIVE_VISUAL_ID, &val);
        if (visualId == val) {
            config = configs[i];
            break;
        }
    }

    XSetWindowAttributes attr;
    attr.background_pixel = 0;
    attr.border_pixel = 0;
    attr.colormap = XCreateColormap(display(), rootWindow(), (Visual*)QX11Info::appVisual(), AllocNone);
    attr.event_mask = StructureNotifyMask | ExposureMask;
    unsigned long mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;
    int width = 100, height = 100;
    mGLWindow = XCreateWindow(display(), rootWindow(), 0, 0, width, height,
                              0, QX11Info::appDepth(), InputOutput,
                              (Visual*)QX11Info::appVisual(), mask, &attr);

    mEGLSurface = eglCreateWindowSurface(mEGLDisplay, config, mGLWindow, 0);

    const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    mEGLContext = eglCreateContext(mEGLDisplay, config, EGL_NO_CONTEXT, context_attribs);
    if (mEGLContext == EGL_NO_CONTEXT) {
        return false;
    }
    if (eglMakeCurrent(mEGLDisplay, mEGLSurface, mEGLSurface, mEGLContext) == EGL_FALSE) {
        return false;
    }
    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        return false;
    }
    return true;
#else
    return false;
#endif
}

void CompositingPrefs::deleteEGLContext()
{
#ifdef KWIN_HAVE_OPENGLES
    eglMakeCurrent(mEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(mEGLDisplay, mEGLContext);
    eglDestroySurface(mEGLDisplay, mEGLSurface);
    eglTerminate(mEGLDisplay);
    eglReleaseThread();
    XDestroyWindow(display(), mGLWindow);
#endif
}

void CompositingPrefs::detectDriverAndVersion()
{
    GLPlatform *gl = GLPlatform::instance();
    gl->detect();
    gl->printResults();
}

// See http://techbase.kde.org/Projects/KWin/HW for a list of some cards that are known to work.
void CompositingPrefs::applyDriverSpecificOptions()
{
    // Always recommend
    mRecommendCompositing = true;

    GLPlatform *gl = GLPlatform::instance();
    mStrictBinding = !gl->supports(LooseBinding);
    if (gl->driver() == Driver_Intel)
        mEnableVSync = false;
}

} // namespace

