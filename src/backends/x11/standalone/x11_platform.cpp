/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_platform.h"
#include "x11cursor.h"
#include "x11placeholderoutput.h"
#include "edge.h"
#include "session.h"
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
#include "eglbackend.h"
#include "keyboard_input.h"
#include "logging.h"
#include "screenedges_filter.h"
#include "options.h"
#include "overlaywindow_x11.h"
#include "non_composited_outline.h"
#include "workspace.h"
#include "x11_output.h"
#include "utils/xcbutils.h"
#include "renderloop.h"

#include <kwinxrenderutils.h>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KCrash>

#include <QThread>
#include <QOpenGLContext>
#include <QX11Info>

namespace KWin
{

class XrandrEventFilter : public X11EventFilter
{
public:
    explicit XrandrEventFilter(X11StandalonePlatform *backend);

    bool event(xcb_generic_event_t *event) override;

private:
    X11StandalonePlatform *m_backend;
};

XrandrEventFilter::XrandrEventFilter(X11StandalonePlatform *backend)
    : X11EventFilter(Xcb::Extensions::self()->randrNotifyEvent())
    , m_backend(backend)
{
}

bool XrandrEventFilter::event(xcb_generic_event_t *event)
{
    Q_ASSERT((event->response_type & ~0x80) == Xcb::Extensions::self()->randrNotifyEvent());
    // let's try to gather a few XRandR events, unlikely that there is just one
    m_backend->scheduleUpdateOutputs();

    // update default screen
    auto *xrrEvent = reinterpret_cast<xcb_randr_screen_change_notify_event_t*>(event);
    xcb_screen_t *screen = kwinApp()->x11DefaultScreen();
    if (xrrEvent->rotation & (XCB_RANDR_ROTATION_ROTATE_90 | XCB_RANDR_ROTATION_ROTATE_270)) {
        screen->width_in_pixels = xrrEvent->height;
        screen->height_in_pixels = xrrEvent->width;
        screen->width_in_millimeters = xrrEvent->mheight;
        screen->height_in_millimeters = xrrEvent->mwidth;
    } else {
        screen->width_in_pixels = xrrEvent->width;
        screen->height_in_pixels = xrrEvent->height;
        screen->width_in_millimeters = xrrEvent->mwidth;
        screen->height_in_millimeters = xrrEvent->mheight;
    }

    return false;
}

X11StandalonePlatform::X11StandalonePlatform(QObject *parent)
    : Platform(parent)
    , m_session(Session::create(Session::Type::Noop, this))
    , m_updateOutputsTimer(new QTimer(this))
    , m_x11Display(QX11Info::display())
    , m_renderLoop(new RenderLoop(this))
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

    m_updateOutputsTimer->setSingleShot(true);
    connect(m_updateOutputsTimer, &QTimer::timeout, this, &X11StandalonePlatform::updateOutputs);

    setSupportsGammaControl(true);
    setPerScreenRenderingEnabled(false);
}

X11StandalonePlatform::~X11StandalonePlatform()
{
    if (m_openGLFreezeProtectionThread) {
        m_openGLFreezeProtectionThread->quit();
        m_openGLFreezeProtectionThread->wait();
        delete m_openGLFreezeProtectionThread;
    }
    if (sceneEglDisplay() != EGL_NO_DISPLAY) {
        eglTerminate(sceneEglDisplay());
    }
    if (isReady()) {
        XRenderUtils::cleanup();
    }
}

bool X11StandalonePlatform::initialize()
{
    if (!QX11Info::isPlatformX11()) {
        return false;
    }
    XRenderUtils::init(kwinApp()->x11Connection(), kwinApp()->x11RootWindow());
    setReady(true);
    initOutputs();

    if (Xcb::Extensions::self()->isRandrAvailable()) {
        m_randrEventFilter.reset(new XrandrEventFilter(this));
    }
    connect(Cursors::self(), &Cursors::hiddenChanged, this, &X11StandalonePlatform::updateCursor);
    return true;
}

Session *X11StandalonePlatform::session() const
{
    return m_session;
}

OpenGLBackend *X11StandalonePlatform::createOpenGLBackend()
{
    switch (options->glPlatformInterface()) {
#if HAVE_EPOXY_GLX
    case GlxPlatformInterface:
        if (hasGlx()) {
            return new GlxBackend(m_x11Display, this);
        } else {
            qCWarning(KWIN_X11STANDALONE) << "Glx not available, trying EGL instead.";
            // no break, needs fall-through
            Q_FALLTHROUGH();
        }
#endif
    case EglPlatformInterface:
        return new EglBackend(m_x11Display, this);
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
        xkb->setConfig(kwinApp()->kxkbConfig());
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
                    "you can reset this protection but <b>be aware that this might result in an immediate crash!</b></p>");

    if (!Xcb::Extensions::self()->isCompositeAvailable() || !Xcb::Extensions::self()->isDamageAvailable()) {
        return i18n("Required X extensions (XComposite and XDamage) are not available.");
    }
    if (!hasGlx()) {
        return i18n("GLX/OpenGL is not available.");
    }
    return QString();
}

bool X11StandalonePlatform::compositingPossible() const
{
    // first off, check whether we figured that we'll crash on detection because of a buggy driver
    KConfigGroup gl_workaround_group(kwinApp()->config(), "Compositing");
    const QString unsafeKey(QLatin1String("OpenGLIsUnsafe") + (kwinApp()->isX11MultiHead() ? QString::number(kwinApp()->x11ScreenNumber()) : QString()));
    if (gl_workaround_group.readEntry("Backend", "OpenGL") == QLatin1String("OpenGL") &&
        gl_workaround_group.readEntry(unsafeKey, false)) {
        qCWarning(KWIN_X11STANDALONE) << "Compositing disabled: video driver seems unstable. If you think it's a false positive, please remove "
                                      << unsafeKey << " from [Compositing] in kwinrc and restart kwin.";
        return false;
    }

    if (!Xcb::Extensions::self()->isCompositeAvailable()) {
        qCWarning(KWIN_X11STANDALONE) << "Compositing disabled: no composite extension available";
        return false;
    }
    if (!Xcb::Extensions::self()->isDamageAvailable()) {
        qCWarning(KWIN_X11STANDALONE) << "Compositing disabled: no damage extension available";
        return false;
    }
    if (hasGlx())
        return true;
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES) {
        return true;
    } else if (qstrcmp(qgetenv("KWIN_COMPOSE"), "O2ES") == 0) {
        return true;
    }
    qCWarning(KWIN_X11STANDALONE) << "Compositing disabled: no OpenGL support";
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
            QMetaObject::invokeMethod(m_openGLFreezeProtection, QOverload<>::of(&QTimer::start), Qt::QueuedConnection);
        }
        break;
    case OpenGLSafePoint::PostInit:
        group.writeEntry(unsafeKey, false);
        group.sync();
        // Deliberately continue with PostFrame
        Q_FALLTHROUGH();
    case OpenGLSafePoint::PostFrame:
        QMetaObject::invokeMethod(m_openGLFreezeProtection, &QTimer::stop, Qt::QueuedConnection);
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

void X11StandalonePlatform::updateCursor()
{
    if (Cursors::self()->isCursorHidden()) {
        xcb_xfixes_hide_cursor(kwinApp()->x11Connection(), kwinApp()->x11RootWindow());
    } else {
        xcb_xfixes_show_cursor(kwinApp()->x11Connection(), kwinApp()->x11RootWindow());
    }
}

void X11StandalonePlatform::startInteractiveWindowSelection(std::function<void(KWin::Toplevel*)> callback, const QByteArray &cursorName)
{
    if (m_windowSelector.isNull()) {
        m_windowSelector.reset(new WindowSelector);
    }
    m_windowSelector->start(callback, cursorName);
}

void X11StandalonePlatform::startInteractivePositionSelection(std::function<void (const QPoint &)> callback)
{
    if (m_windowSelector.isNull()) {
        m_windowSelector.reset(new WindowSelector);
    }
    m_windowSelector->start(callback);
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

OutlineVisual *X11StandalonePlatform::createOutline(Outline *outline)
{
    // first try composited Outline
    auto ret = Platform::createOutline(outline);
    if (!ret) {
        ret = new NonCompositedOutlineVisual(outline);
    }
    return ret;
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
                    qCDebug(KWIN_X11STANDALONE) << "inverting screen using xcb_randr_set_crtc_gamma";
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
    compositors << NoCompositing;
    return compositors;
}

void X11StandalonePlatform::initOutputs()
{
    doUpdateOutputs<Xcb::RandR::ScreenResources>();
    updateRefreshRate();
}

void X11StandalonePlatform::scheduleUpdateOutputs()
{
    m_updateOutputsTimer->start();
}

void X11StandalonePlatform::updateOutputs()
{
    doUpdateOutputs<Xcb::RandR::CurrentResources>();
    updateRefreshRate();
}

template <typename T>
void X11StandalonePlatform::doUpdateOutputs()
{
    QVector<AbstractOutput *> changed;
    QVector<AbstractOutput *> added;
    QVector<AbstractOutput *> removed = m_outputs;

    if (Xcb::Extensions::self()->isRandrAvailable()) {
        T resources(rootWindow());
        if (!resources.isNull()) {
            xcb_randr_crtc_t *crtcs = resources.crtcs();
            const xcb_randr_mode_info_t *modes = resources.modes();

            QVector<Xcb::RandR::CrtcInfo> infos(resources->num_crtcs);
            for (int i = 0; i < resources->num_crtcs; ++i) {
                infos[i] = Xcb::RandR::CrtcInfo(crtcs[i], resources->config_timestamp);
            }

            for (int i = 0; i < resources->num_crtcs; ++i) {
                Xcb::RandR::CrtcInfo info(infos.at(i));

                const QRect geometry = info.rect();
                if (!geometry.isValid()) {
                    continue;
                }

                xcb_randr_output_t *outputs = info.outputs();
                QVector<Xcb::RandR::OutputInfo> outputInfos(outputs ? resources->num_outputs : 0);
                if (outputs) {
                    for (int i = 0; i < resources->num_outputs; ++i) {
                        outputInfos[i] = Xcb::RandR::OutputInfo(outputs[i], resources->config_timestamp);
                    }
                }

                float refreshRate = -1.0f;
                for (int j = 0; j < resources->num_modes; ++j) {
                    if (info->mode == modes[j].id) {
                        if (modes[j].htotal != 0 && modes[j].vtotal != 0) { // BUG 313996
                            // refresh rate calculation - WTF was wikipedia 1998 when I needed it?
                            int dotclock = modes[j].dot_clock,
                                vtotal = modes[j].vtotal;
                            if (modes[j].mode_flags & XCB_RANDR_MODE_FLAG_INTERLACE)
                                dotclock *= 2;
                            if (modes[j].mode_flags & XCB_RANDR_MODE_FLAG_DOUBLE_SCAN)
                                vtotal *= 2;
                            refreshRate = dotclock/float(modes[j].htotal*vtotal);
                        }
                        break; // found mode
                    }
                }

                for (int j = 0; j < info->num_outputs; ++j) {
                    Xcb::RandR::OutputInfo outputInfo(outputInfos.at(j));
                    if (outputInfo->crtc != crtcs[i]) {
                        continue;
                    }

                    X11Output *output = findX11Output(outputInfo.name());
                    if (output) {
                        changed.append(output);
                        removed.removeOne(output);
                    } else {
                        output = new X11Output(outputInfo.name());
                        added.append(output);
                    }

                    // TODO: Perhaps the output has to save the inherited gamma ramp and
                    // restore it during tear down. Currently neither standalone x11 nor
                    // drm platform do this.
                    Xcb::RandR::CrtcGamma gamma(crtcs[i]);

                    output->setCrtc(crtcs[i]);
                    output->setGammaRampSize(gamma.isNull() ? 0 : gamma->size);
                    output->setGeometry(geometry);
                    output->setRefreshRate(refreshRate * 1000);
                    output->setXineramaNumber(i);

                    QSize physicalSize(outputInfo->mm_width, outputInfo->mm_height);
                    switch (info->rotation) {
                    case XCB_RANDR_ROTATION_ROTATE_0:
                    case XCB_RANDR_ROTATION_ROTATE_180:
                        break;
                    case XCB_RANDR_ROTATION_ROTATE_90:
                    case XCB_RANDR_ROTATION_ROTATE_270:
                        physicalSize.transpose();
                        break;
                    case XCB_RANDR_ROTATION_REFLECT_X:
                    case XCB_RANDR_ROTATION_REFLECT_Y:
                        break;
                    }
                    output->setPhysicalSize(physicalSize);
                    break;
                }
            }
        }
    }

    // The workspace handles having no outputs poorly. If the last output is about to be
    // removed, create a dummy output to avoid crashing.
    if (changed.isEmpty() && added.isEmpty()) {
        auto dummyOutput = new X11PlaceholderOutput();
        m_outputs << dummyOutput;
        Q_EMIT outputAdded(dummyOutput);
        Q_EMIT outputEnabled(dummyOutput);
    }

    // Process new outputs. Note new outputs must be introduced before removing any other outputs.
    for (AbstractOutput *output : qAsConst(added)) {
        m_outputs.append(output);
        Q_EMIT outputAdded(output);
        Q_EMIT outputEnabled(output);
    }

    // Outputs have to be removed last to avoid the case where there are no enabled outputs.
    for (AbstractOutput *output : qAsConst(removed)) {
        m_outputs.removeOne(output);
        Q_EMIT outputDisabled(output);
        Q_EMIT outputRemoved(output);
        delete output;
    }

    // Make sure that the position of an output in m_outputs matches its xinerama index, there
    // are X11 protocols that use xinerama indices to identify outputs.
    std::sort(m_outputs.begin(), m_outputs.end(), [](const AbstractOutput *a, const AbstractOutput *b) {
        const auto xa = qobject_cast<const X11Output *>(a);
        if (!xa) {
            return false;
        }
        const auto xb = qobject_cast<const X11Output *>(b);
        if (!xb) {
            return true;
        }
        return xa->xineramaNumber() < xb->xineramaNumber();
    });

    Q_EMIT screensQueried();
}

X11Output *X11StandalonePlatform::findX11Output(const QString &name) const
{
    for (AbstractOutput *output : m_outputs) {
        if (output->name() == name) {
            return qobject_cast<X11Output *>(output);
        }
    }
    return nullptr;
}

Outputs X11StandalonePlatform::outputs() const
{
    return m_outputs;
}

Outputs X11StandalonePlatform::enabledOutputs() const
{
    return m_outputs;
}

RenderLoop *X11StandalonePlatform::renderLoop() const
{
    return m_renderLoop;
}

static bool refreshRate_compare(const AbstractOutput *first, const AbstractOutput *smallest)
{
    return first->refreshRate() < smallest->refreshRate();
}

static int currentRefreshRate()
{
    static const int refreshRate = qEnvironmentVariableIntValue("KWIN_X11_REFRESH_RATE");
    if (refreshRate) {
        return refreshRate;
    }

    const QVector<AbstractOutput *> outputs = kwinApp()->platform()->enabledOutputs();
    if (outputs.isEmpty()) {
        return 60000;
    }

    static const QString syncDisplayDevice = qEnvironmentVariable("__GL_SYNC_DISPLAY_DEVICE");
    if (!syncDisplayDevice.isEmpty()) {
        for (const AbstractOutput *output : outputs) {
            if (output->name() == syncDisplayDevice) {
                return output->refreshRate();
            }
        }
    }

    auto syncIt = std::min_element(outputs.begin(), outputs.end(), refreshRate_compare);
    return (*syncIt)->refreshRate();
}

void X11StandalonePlatform::updateRefreshRate()
{
    int refreshRate = currentRefreshRate();
    if (refreshRate <= 0) {
        qCWarning(KWIN_X11STANDALONE) << "Bogus refresh rate" << refreshRate;
        refreshRate = 60000;
    }

    m_renderLoop->setRefreshRate(refreshRate);
}

}
