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
#include "sync_filter.h"
#include "windowselector.h"
#include <config-kwin.h>
#include <kwinconfig.h>
#if HAVE_EPOXY_GLX
#include "glxbackend.h"
#endif
#if HAVE_X11_XINPUT
#include "xinputintegration.h"
#endif
#include "abstract_client.h"
#include "effects_x11.h"
#include "eglonxbackend.h"
#include "keyboard_input.h"
#include "logging.h"
#include "screens_xrandr.h"
#include "screenedges_filter.h"
#include "options.h"
#include "overlaywindow_x11.h"
#include "non_composited_outline.h"
#include "workspace.h"
#include "x11_decoration_renderer.h"

#include <kwinxrenderutils.h>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KCrash>

#include <QThread>
#include <QOpenGLContext>
#include <QX11Info>

namespace KWin
{

X11StandalonePlatform::X11StandalonePlatform(QObject *parent)
    : Platform(parent)
    , m_x11Display(QX11Info::display())
{
#if HAVE_X11_XINPUT
    if (!qEnvironmentVariableIsSet("KWIN_NO_XI2")) {
        m_xinputIntegration = new XInputIntegration(m_x11Display, this);
        m_xinputIntegration->init();
        if (!m_xinputIntegration->hasXinput()) {
            delete m_xinputIntegration;
            m_xinputIntegration = nullptr;
        } else {
            connect(kwinApp(), &Application::workspaceCreated, m_xinputIntegration, &XInputIntegration::startListening);
        }
    }
#endif
    connect(kwinApp(), &Application::workspaceCreated, this,
        [this] {
            if (Xcb::Extensions::self()->isSyncAvailable()) {
                m_syncFilter = std::make_unique<SyncFilter>();
            }
        }
    );
}

X11StandalonePlatform::~X11StandalonePlatform()
{
    if (m_openGLFreezeProtectionThread) {
        m_openGLFreezeProtectionThread->quit();
        m_openGLFreezeProtectionThread->wait();
        delete m_openGLFreezeProtectionThread;
    }
    if (isReady()) {
        XRenderUtils::cleanup();
    }
}

void X11StandalonePlatform::init()
{
    if (!QX11Info::isPlatformX11()) {
        emit initFailed();
        return;
    }
    XRenderUtils::init(kwinApp()->x11Connection(), kwinApp()->x11RootWindow());
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
            return new GlxBackend(m_x11Display);
        } else {
            qCWarning(KWIN_X11STANDALONE) << "Glx not available, trying EGL instead.";
            // no break, needs fall-through
            Q_FALLTHROUGH();
        }
#endif
    case EglPlatformInterface:
        return new EglOnXBackend(m_x11Display);
    default:
        // no backend available
        return nullptr;
    }
}

Edge *X11StandalonePlatform::createScreenEdge(ScreenEdges *edges)
{
    if (m_screenEdgesFilter.isNull()) {
        m_screenEdgesFilter.reset(new ScreenEdgesFilter);
    }
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
        Q_FALLTHROUGH();
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
            const QString configName = kwinApp()->config()->name();
            m_openGLFreezeProtection->moveToThread(m_openGLFreezeProtectionThread);
            connect(m_openGLFreezeProtection, &QTimer::timeout, m_openGLFreezeProtection,
                [configName] {
                    const QString unsafeKey(QLatin1String("OpenGLIsUnsafe") + (kwinApp()->isX11MultiHead() ? QString::number(kwinApp()->x11ScreenNumber()) : QString()));
                    auto group = KConfigGroup(KSharedConfig::openConfig(configName), "Compositing");
                    group.writeEntry(unsafeKey, true);
                    group.sync();
                    KCrash::setDrKonqiEnabled(false);
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
        Q_FALLTHROUGH();
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

PlatformCursorImage X11StandalonePlatform::cursorImage() const
{
    auto c = kwinApp()->x11Connection();
    QScopedPointer<xcb_xfixes_get_cursor_image_reply_t, QScopedPointerPodDeleter> cursor(
        xcb_xfixes_get_cursor_image_reply(c,
                                          xcb_xfixes_get_cursor_image_unchecked(c),
                                          nullptr));
    if (cursor.isNull()) {
        return PlatformCursorImage();
    }

    QImage qcursorimg((uchar *) xcb_xfixes_get_cursor_image_cursor_image(cursor.data()), cursor->width, cursor->height,
                      QImage::Format_ARGB32_Premultiplied);
    // deep copy of image as the data is going to be freed
    return PlatformCursorImage(qcursorimg.copy(), QPoint(cursor->xhot, cursor->yhot));
}

void X11StandalonePlatform::doHideCursor()
{
    xcb_xfixes_hide_cursor(kwinApp()->x11Connection(), kwinApp()->x11RootWindow());
}

void X11StandalonePlatform::doShowCursor()
{
    xcb_xfixes_show_cursor(kwinApp()->x11Connection(), kwinApp()->x11RootWindow());
}

void X11StandalonePlatform::startInteractiveWindowSelection(std::function<void(KWin::Toplevel*)> callback, const QByteArray &cursorName)
{
    if (m_windowSelector.isNull()) {
        m_windowSelector.reset(new WindowSelector);
    }
    m_windowSelector->start(callback, cursorName);
}

void X11StandalonePlatform::setupActionForGlobalAccel(QAction *action)
{
    connect(action, &QAction::triggered, kwinApp(), [action] {
        QVariant timestamp = action->property("org.kde.kglobalaccel.activationTimestamp");
        bool ok = false;
        const quint32 t = timestamp.toULongLong(&ok);
        if (ok) {
            kwinApp()->setX11Time(t);
        }
    });
}

OverlayWindow *X11StandalonePlatform::createOverlayWindow()
{
    return new OverlayWindowX11();
}

/*
 Updates xTime(). This used to simply fetch current timestamp from the server,
 but that can cause xTime() to be newer than timestamp of events that are
 still in our events queue, thus e.g. making XSetInputFocus() caused by such
 event to be ignored. Therefore events queue is searched for first
 event with timestamp, and extra PropertyNotify is generated in order to make
 sure such event is found.
*/
void X11StandalonePlatform::updateXTime()
{
    // NOTE: QX11Info::getTimestamp does not yet search the event queue as the old
    // solution did. This means there might be regressions currently. See the
    // documentation above on how it should be done properly.
    kwinApp()->setX11Time(QX11Info::getTimestamp(), Application::TimestampUpdate::Always);
}

OutlineVisual *X11StandalonePlatform::createOutline(Outline *outline)
{
    // first try composited Outline
    auto ret = Platform::createOutline(outline);
    if (!ret) {
        ret = new NonCompositedOutlineVisual(outline);
    }
    return ret;
}

Decoration::Renderer *X11StandalonePlatform::createDecorationRenderer(Decoration::DecoratedClientImpl *client)
{
    auto renderer = Platform::createDecorationRenderer(client);
    if (!renderer) {
        renderer = new Decoration::X11Renderer(client);
    }
    return renderer;
}

void X11StandalonePlatform::invertScreen()
{
    using namespace Xcb::RandR;
    bool succeeded = false;

    if (Xcb::Extensions::self()->isRandrAvailable()) {
        const auto active_client = workspace()->activeClient();
        ScreenResources res((active_client && active_client->window() != XCB_WINDOW_NONE) ? active_client->window() : rootWindow());

        if (!res.isNull()) {
            for (int j = 0; j < res->num_crtcs; ++j) {
                auto crtc = res.crtcs()[j];
                CrtcGamma gamma(crtc);
                if (gamma.isNull()) {
                    continue;
                }
                if (gamma->size) {
                    qCDebug(KWIN_CORE) << "inverting screen using xcb_randr_set_crtc_gamma";
                    const int half = gamma->size / 2 + 1;

                    uint16_t *red = gamma.red();
                    uint16_t *green = gamma.green();
                    uint16_t *blue = gamma.blue();
                    for (int i = 0; i < half; ++i) {
                        auto invert = [&gamma, i](uint16_t *ramp) {
                            qSwap(ramp[i], ramp[gamma->size - 1 - i]);
                        };
                        invert(red);
                        invert(green);
                        invert(blue);
                    }
                    xcb_randr_set_crtc_gamma(connection(), crtc, gamma->size, red, green, blue);
                    succeeded = true;
                }
            }
        }
    }
    if (!succeeded) {
        Platform::invertScreen();
    }
}

void X11StandalonePlatform::createEffectsHandler(Compositor *compositor, Scene *scene)
{
    new EffectsHandlerImplX11(compositor, scene);
}

QVector<CompositingType> X11StandalonePlatform::supportedCompositors() const
{
    QVector<CompositingType> compositors;
#if HAVE_EPOXY_GLX
    compositors << OpenGLCompositing;
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    compositors << XRenderCompositing;
#endif
    compositors << NoCompositing;
    return compositors;
}

}
