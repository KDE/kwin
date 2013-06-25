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

#include "main.h"
#include "xcbutils.h"
#include "kwinglplatform.h"

#include <kconfiggroup.h>
#include <KDE/KLocalizedString>
#include <kdeversion.h>
#include <ksharedconfig.h>

#include <QDebug>
#include <QStandardPaths>
#include <qprocess.h>


namespace KWin
{

extern int screen_number; // main.cpp
extern bool is_multihead;

CompositingPrefs::CompositingPrefs()
{
}

CompositingPrefs::~CompositingPrefs()
{
}

bool CompositingPrefs::openGlIsBroken()
{
    const QString unsafeKey(QStringLiteral("OpenGLIsUnsafe") + (is_multihead ? QString::number(screen_number) : QString()));
    return KConfigGroup(KSharedConfig::openConfig(), "Compositing").readEntry(unsafeKey, false);
}

bool CompositingPrefs::compositingPossible()
{
    // first off, check whether we figured that we'll crash on detection because of a buggy driver
    KConfigGroup gl_workaround_group(KSharedConfig::openConfig(), "Compositing");
    const QString unsafeKey(QStringLiteral("OpenGLIsUnsafe") + (is_multihead ? QString::number(screen_number) : QString()));
    if (gl_workaround_group.readEntry("Backend", "OpenGL") == QStringLiteral("OpenGL") &&
        gl_workaround_group.readEntry(unsafeKey, false))
        return false;

    if (kwinApp()->shouldUseWaylandForCompositing()) {
        // don't do X specific checks if we are not going to use X for compositing
        return true;
    }

    if (!Xcb::Extensions::self()->isCompositeAvailable()) {
        qDebug() << "No composite extension available";
        return false;
    }
    if (!Xcb::Extensions::self()->isDamageAvailable()) {
        qDebug() << "No damage extension available";
        return false;
    }
    if (hasGlx())
        return true;
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (Xcb::Extensions::self()->isRenderAvailable() && Xcb::Extensions::self()->isFixesAvailable())
        return true;
#endif
#ifdef KWIN_HAVE_OPENGLES
    return true;
#endif
    qDebug() << "No OpenGL or XRender/XFixes support";
    return false;
}

QString CompositingPrefs::compositingNotPossibleReason()
{
    // first off, check whether we figured that we'll crash on detection because of a buggy driver
    KConfigGroup gl_workaround_group(KSharedConfig::openConfig(), "Compositing");
    const QString unsafeKey(QStringLiteral("OpenGLIsUnsafe") + (is_multihead ? QString::number(screen_number) : QString()));
    if (gl_workaround_group.readEntry("Backend", "OpenGL") == QStringLiteral("OpenGL") &&
        gl_workaround_group.readEntry(unsafeKey, false))
        return i18n("<b>OpenGL compositing (the default) has crashed KWin in the past.</b><br>"
                    "This was most likely due to a driver bug."
                    "<p>If you think that you have meanwhile upgraded to a stable driver,<br>"
                    "you can reset this protection but <b>be aware that this might result in an immediate crash!</b></p>"
                    "<p>Alternatively, you might want to use the XRender backend instead.</p>");

    if (!Xcb::Extensions::self()->isCompositeAvailable() || !Xcb::Extensions::self()->isDamageAvailable()) {
        return i18n("Required X extensions (XComposite and XDamage) are not available.");
    }
#if !defined( KWIN_HAVE_XRENDER_COMPOSITING )
    if (!hasGlx())
        return i18n("GLX/OpenGL are not available and only OpenGL support is compiled.");
#else
    if (!(hasGlx()
            || (Xcb::Extensions::self()->isRenderAvailable() && Xcb::Extensions::self()->isFixesAvailable()))) {
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

} // namespace

