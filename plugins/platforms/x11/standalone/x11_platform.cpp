/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

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
#include "x11_platform.h"
#include "x11cursor.h"
#include "edge.h"
#include <config-kwin.h>
#include <kwinconfig.h>
#if HAVE_EPOXY_GLX
#include "glxbackend.h"
#endif
#if HAVE_X11_XINPUT
#include "xinputintegration.h"
#endif
#include "eglonxbackend.h"
#include "keyboard_input.h"
#include "logging.h"
#include "screens_xrandr.h"
#include "options.h"

#include <KConfigGroup>
#include <KLocalizedString>

#include <QThread>
#include <QOpenGLContext>
#include <QX11Info>

namespace KWin
{

X11StandalonePlatform::X11StandalonePlatform(QObject *parent)
    : Platform(parent)
{
#if HAVE_X11_XINPUT
    if (!qEnvironmentVariableIsSet("KWIN_NO_XI2")) {
        m_xinputIntegration = new XInputIntegration(this);
        m_xinputIntegration->init();
        if (!m_xinputIntegration->hasXinput()) {
            delete m_xinputIntegration;
            m_xinputIntegration = nullptr;
        } else {
            connect(kwinApp(), &Application::workspaceCreated, m_xinputIntegration, &XInputIntegration::startListening);
        }
    }
#endif
}

X11StandalonePlatform::~X11StandalonePlatform() = default;

void X11StandalonePlatform::init()
{
    if (!QX11Info::isPlatformX11()) {
        emit initFailed();
        return;
    }
    setReady(true);
    emit screensQueried();
}

Screens *X11StandalonePlatform::createScreens(QObject *parent)
{
    return new XRandRScreens(parent);
}

OpenGLBackend *X11StandalonePlatform::createOpenGLBackend()
{
    switch (options->glPlatformInterface()) {
#if HAVE_EPOXY_GLX
    case GlxPlatformInterface:
        if (hasGlx()) {
            return new GlxBackend();
        } else {
            qCWarning(KWIN_X11STANDALONE) << "Glx not available, trying EGL instead.";
            // no break, needs fall-through
        }
#endif
    case EglPlatformInterface:
        return new EglOnXBackend();
    default:
        // no backend available
        return nullptr;
    }
}

Edge *X11StandalonePlatform::createScreenEdge(ScreenEdges *edges)
{
    return new WindowBasedEdge(edges);
}

void X11StandalonePlatform::createPlatformCursor(QObject *parent)
{
    auto c = new X11Cursor(parent, m_xinputIntegration != nullptr);
#if HAVE_X11_XINPUT
    if (m_xinputIntegration) {
        m_xinputIntegration->setCursor(c);
        // we know we have xkb already
        auto xkb = input()->keyboard()->xkb();
        m_xinputIntegration->setXkb(xkb);
        xkb->reconfigure();
    }
#endif
}

bool X11StandalonePlatform::requiresCompositing() const
{
    return false;
}

bool X11StandalonePlatform::openGLCompositingIsBroken() const
{
    const QString unsafeKey(QLatin1String("OpenGLIsUnsafe") + (kwinApp()->isX11MultiHead() ? QString::number(kwinApp()->x11ScreenNumber()) : QString()));
    return KConfigGroup(kwinApp()->config(), "Compositing").readEntry(unsafeKey, false);
}

QString X11StandalonePlatform::compositingNotPossibleReason() const
{
    // first off, check whether we figured that we'll crash on detection because of a buggy driver
    KConfigGroup gl_workaround_group(kwinApp()->config(), "Compositing");
    const QString unsafeKey(QLatin1String("OpenGLIsUnsafe") + (kwinApp()->isX11MultiHead() ? QString::number(kwinApp()->x11ScreenNumber()) : QString()));
    if (gl_workaround_group.readEntry("Backend", "OpenGL") == QLatin1String("OpenGL") &&
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

bool X11StandalonePlatform::compositingPossible() const
{
    // first off, check whether we figured that we'll crash on detection because of a buggy driver
    KConfigGroup gl_workaround_group(kwinApp()->config(), "Compositing");
    const QString unsafeKey(QLatin1String("OpenGLIsUnsafe") + (kwinApp()->isX11MultiHead() ? QString::number(kwinApp()->x11ScreenNumber()) : QString()));
    if (gl_workaround_group.readEntry("Backend", "OpenGL") == QLatin1String("OpenGL") &&
        gl_workaround_group.readEntry(unsafeKey, false))
        return false;


    if (!Xcb::Extensions::self()->isCompositeAvailable()) {
        qCDebug(KWIN_CORE) << "No composite extension available";
        return false;
    }
    if (!Xcb::Extensions::self()->isDamageAvailable()) {
        qCDebug(KWIN_CORE) << "No damage extension available";
        return false;
    }
    if (hasGlx())
        return true;
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (Xcb::Extensions::self()->isRenderAvailable() && Xcb::Extensions::self()->isFixesAvailable())
        return true;
#endif
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES) {
        return true;
    } else if (qstrcmp(qgetenv("KWIN_COMPOSE"), "O2ES") == 0) {
        return true;
    }
    qCDebug(KWIN_CORE) << "No OpenGL or XRender/XFixes support";
    return false;
}

bool X11StandalonePlatform::hasGlx()
{
    return Xcb::Extensions::self()->hasGlx();
}

void X11StandalonePlatform::createOpenGLSafePoint(OpenGLSafePoint safePoint)
{
    const QString unsafeKey(QLatin1String("OpenGLIsUnsafe") + (kwinApp()->isX11MultiHead() ? QString::number(kwinApp()->x11ScreenNumber()) : QString()));
    auto group = KConfigGroup(kwinApp()->config(), "Compositing");
    switch (safePoint) {
    case OpenGLSafePoint::PreInit:
        group.writeEntry(unsafeKey, true);
        group.sync();
        // Deliberately continue with PreFrame
    case OpenGLSafePoint::PreFrame:
        if (m_openGLFreezeProtectionThread == nullptr) {
            Q_ASSERT(m_openGLFreezeProtection == nullptr);
            m_openGLFreezeProtectionThread = new QThread(this);
            m_openGLFreezeProtectionThread->setObjectName("FreezeDetector");
            m_openGLFreezeProtectionThread->start();
            m_openGLFreezeProtection = new QTimer;
            m_openGLFreezeProtection->setInterval(15000);
            m_openGLFreezeProtection->setSingleShot(true);
            m_openGLFreezeProtection->start();
            m_openGLFreezeProtection->moveToThread(m_openGLFreezeProtectionThread);
            connect(m_openGLFreezeProtection, &QTimer::timeout, m_openGLFreezeProtection,
                [] {
                    const QString unsafeKey(QLatin1String("OpenGLIsUnsafe") + (kwinApp()->isX11MultiHead() ? QString::number(kwinApp()->x11ScreenNumber()) : QString()));
                    auto group = KConfigGroup(kwinApp()->config(), "Compositing");
                    group.writeEntry(unsafeKey, true);
                    group.sync();
                    qFatal("Freeze in OpenGL initialization detected");
                }, Qt::DirectConnection);
        } else {
            Q_ASSERT(m_openGLFreezeProtection);
            QMetaObject::invokeMethod(m_openGLFreezeProtection, "start", Qt::QueuedConnection);
        }
        break;
    case OpenGLSafePoint::PostInit:
        group.writeEntry(unsafeKey, false);
        group.sync();
        // Deliberately continue with PostFrame
    case OpenGLSafePoint::PostFrame:
        QMetaObject::invokeMethod(m_openGLFreezeProtection, "stop", Qt::QueuedConnection);
        break;
    case OpenGLSafePoint::PostLastGuardedFrame:
        m_openGLFreezeProtection->deleteLater();
        m_openGLFreezeProtection = nullptr;
        m_openGLFreezeProtectionThread->quit();
        m_openGLFreezeProtectionThread->wait();
        delete m_openGLFreezeProtectionThread;
        m_openGLFreezeProtectionThread = nullptr;
        break;
    }
}

}
