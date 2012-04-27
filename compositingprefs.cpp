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
    : mEnableDirectRendering(true)
{
}

CompositingPrefs::~CompositingPrefs()
{
}

bool CompositingPrefs::openGlIsBroken()
{
    return KConfigGroup(KGlobal::config(), "Compositing").readEntry("OpenGLIsUnsafe", false);
}

bool CompositingPrefs::compositingPossible()
{
    // first off, check whether we figured that we'll crash on detection because of a buggy driver
    KConfigGroup gl_workaround_group(KGlobal::config(), "Compositing");
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
    KConfigGroup gl_workaround_group(KGlobal::config(), "Compositing");
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

#ifndef KWIN_HAVE_OPENGLES
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
#endif
}

} // namespace

