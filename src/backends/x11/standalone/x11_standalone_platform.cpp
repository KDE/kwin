/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_standalone_platform.h"

#include <config-kwin.h>

#include "atoms.h"
#include "core/session.h"
#include "x11_standalone_cursor.h"
#include "x11_standalone_edge.h"
#include "x11_standalone_placeholderoutput.h"
#include "x11_standalone_windowselector.h"
#include <kwinconfig.h>
#if HAVE_EPOXY_GLX
#include "x11_standalone_glx_backend.h"
#endif
#if HAVE_X11_XINPUT
#include "x11_standalone_xinputintegration.h"
#endif
#include "core/renderloop.h"
#include "keyboard_input.h"
#include "options.h"
#include "utils/c_ptr.h"
#include "utils/edid.h"
#include "utils/xcbutils.h"
#include "window.h"
#include "workspace.h"
#include "x11_standalone_effects.h"
#include "x11_standalone_egl_backend.h"
#include "x11_standalone_keyboard.h"
#include "x11_standalone_logging.h"
#include "x11_standalone_non_composited_outline.h"
#include "x11_standalone_output.h"
#include "x11_standalone_overlaywindow.h"
#include "x11_standalone_screenedges_filter.h"

#include "../common/kwinxrenderutils.h"

#include <KConfigGroup>
#include <KCrash>
#include <KLocalizedString>

#include <QOpenGLContext>
#include <QThread>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <private/qtx11extras_p.h>
#else
#include <QX11Info>
#endif

#include <span>

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
    auto *xrrEvent = reinterpret_cast<xcb_randr_screen_change_notify_event_t *>(event);
    xcb_screen_t *screen = Xcb::defaultScreen();
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
    , m_updateOutputsTimer(std::make_unique<QTimer>())
    , m_x11Display(QX11Info::display())
    , m_renderLoop(std::make_unique<RenderLoop>())
{
#if HAVE_X11_XINPUT
    if (!qEnvironmentVariableIsSet("KWIN_NO_XI2")) {
        m_xinputIntegration = std::make_unique<XInputIntegration>(m_x11Display, this);
        m_xinputIntegration->init();
        if (!m_xinputIntegration->hasXinput()) {
            m_xinputIntegration.reset();
        } else {
            connect(kwinApp(), &Application::workspaceCreated, m_xinputIntegration.get(), &XInputIntegration::startListening);
        }
    }
#endif

    m_updateOutputsTimer->setSingleShot(true);
    connect(m_updateOutputsTimer.get(), &QTimer::timeout, this, &X11StandalonePlatform::updateOutputs);

    m_keyboard = std::make_unique<X11Keyboard>();

    setSupportsGammaControl(true);
}

X11StandalonePlatform::~X11StandalonePlatform()
{
    if (m_openGLFreezeProtectionThread) {
        m_openGLFreezeProtectionThread->quit();
        m_openGLFreezeProtectionThread->wait();
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
        m_randrEventFilter = std::make_unique<XrandrEventFilter>(this);
    }
    connect(Cursors::self(), &Cursors::hiddenChanged, this, &X11StandalonePlatform::updateCursor);
    return true;
}

std::unique_ptr<OpenGLBackend> X11StandalonePlatform::createOpenGLBackend()
{
    switch (options->glPlatformInterface()) {
#if HAVE_EPOXY_GLX
    case GlxPlatformInterface:
        if (hasGlx()) {
            return std::make_unique<GlxBackend>(m_x11Display, this);
        } else {
            qCWarning(KWIN_X11STANDALONE) << "Glx not available, trying EGL instead.";
            // no break, needs fall-through
            Q_FALLTHROUGH();
        }
#endif
    case EglPlatformInterface:
        return std::make_unique<EglBackend>(m_x11Display, this);
    default:
        // no backend available
        return nullptr;
    }
}

std::unique_ptr<Edge> X11StandalonePlatform::createScreenEdge(ScreenEdges *edges)
{
    if (!m_screenEdgesFilter) {
        m_screenEdgesFilter = std::make_unique<ScreenEdgesFilter>();
    }
    return std::make_unique<WindowBasedEdge>(edges);
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
    auto timestamp = KConfigGroup(kwinApp()->config(), "Compositing").readEntry(QLatin1String("LastFailureTimestamp"), 0);
    if (timestamp > 0) {
        if (QDateTime::currentSecsSinceEpoch() - timestamp < 60) {
            return true;
        }
    }

    return false;
}

QString X11StandalonePlatform::compositingNotPossibleReason() const
{
    // first off, check whether we figured that we'll crash on detection because of a buggy driver
    KConfigGroup gl_workaround_group(kwinApp()->config(), "Compositing");
    if (gl_workaround_group.readEntry("Backend", "OpenGL") == QLatin1String("OpenGL") && openGLCompositingIsBroken()) {
        return i18n("<b>OpenGL compositing (the default) has crashed KWin in the past.</b><br>"
                    "This was most likely due to a driver bug."
                    "<p>If you think that you have meanwhile upgraded to a stable driver,<br>"
                    "you can reset this protection but <b>be aware that this might result in an immediate crash!</b></p>");
    }

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
    if (gl_workaround_group.readEntry("Backend", "OpenGL") == QLatin1String("OpenGL") && openGLCompositingIsBroken()) {
        qCWarning(KWIN_X11STANDALONE) << "Compositing disabled: video driver seems unstable. If you think it's a false positive, please try again in a few minutes.";
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
    if (hasGlx()) {
        return true;
    }
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
    auto group = KConfigGroup(kwinApp()->config(), "Compositing");
    switch (safePoint) {
    case OpenGLSafePoint::PreInit:
        // Explicitly write the failure timestamp so that if we crash during
        // OpenGL init, we know we should not try again.
        group.writeEntry(QLatin1String("LastFailureTimestamp"), QDateTime::currentSecsSinceEpoch());
        group.sync();
        // Deliberately continue with PreFrame
        Q_FALLTHROUGH();
    case OpenGLSafePoint::PreFrame:
        if (m_openGLFreezeProtectionThread == nullptr) {
            Q_ASSERT(m_openGLFreezeProtection == nullptr);
            m_openGLFreezeProtectionThread = std::make_unique<QThread>();
            m_openGLFreezeProtectionThread->setObjectName("FreezeDetector");
            m_openGLFreezeProtectionThread->start();
            m_openGLFreezeProtection = std::make_unique<QTimer>();
            m_openGLFreezeProtection->setInterval(15000);
            m_openGLFreezeProtection->setSingleShot(true);
            m_openGLFreezeProtection->start();
            const QString configName = kwinApp()->config()->name();
            m_openGLFreezeProtection->moveToThread(m_openGLFreezeProtectionThread.get());
            connect(
                m_openGLFreezeProtection.get(), &QTimer::timeout, m_openGLFreezeProtection.get(),
                [configName] {
                    auto group = KConfigGroup(KSharedConfig::openConfig(configName), "Compositing");
                    group.writeEntry(QLatin1String("LastFailureTimestamp"), QDateTime::currentSecsSinceEpoch());
                    group.sync();
                    KCrash::setDrKonqiEnabled(false);
                    qFatal("Freeze in OpenGL initialization detected");
                },
                Qt::DirectConnection);
        } else {
            Q_ASSERT(m_openGLFreezeProtection);
            QMetaObject::invokeMethod(m_openGLFreezeProtection.get(), QOverload<>::of(&QTimer::start), Qt::QueuedConnection);
        }
        break;
    case OpenGLSafePoint::PostInit:
        group.deleteEntry(QLatin1String("LastFailureTimestamp"));
        group.sync();
        // Deliberately continue with PostFrame
        Q_FALLTHROUGH();
    case OpenGLSafePoint::PostFrame:
        QMetaObject::invokeMethod(m_openGLFreezeProtection.get(), &QTimer::stop, Qt::QueuedConnection);
        break;
    case OpenGLSafePoint::PostLastGuardedFrame:
        m_openGLFreezeProtectionThread->quit();
        m_openGLFreezeProtectionThread->wait();
        m_openGLFreezeProtectionThread.reset();
        m_openGLFreezeProtection.reset();
        break;
    }
}

PlatformCursorImage X11StandalonePlatform::cursorImage() const
{
    auto c = kwinApp()->x11Connection();
    UniqueCPtr<xcb_xfixes_get_cursor_image_reply_t> cursor(
        xcb_xfixes_get_cursor_image_reply(c,
                                          xcb_xfixes_get_cursor_image_unchecked(c),
                                          nullptr));
    if (!cursor) {
        return PlatformCursorImage();
    }

    QImage qcursorimg((uchar *)xcb_xfixes_get_cursor_image_cursor_image(cursor.get()), cursor->width, cursor->height,
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

void X11StandalonePlatform::startInteractiveWindowSelection(std::function<void(KWin::Window *)> callback, const QByteArray &cursorName)
{
    if (!m_windowSelector) {
        m_windowSelector = std::make_unique<WindowSelector>();
    }
    m_windowSelector->start(callback, cursorName);
}

void X11StandalonePlatform::startInteractivePositionSelection(std::function<void(const QPoint &)> callback)
{
    if (!m_windowSelector) {
        m_windowSelector = std::make_unique<WindowSelector>();
    }
    m_windowSelector->start(callback);
}

std::unique_ptr<OverlayWindow> X11StandalonePlatform::createOverlayWindow()
{
    return std::make_unique<OverlayWindowX11>();
}

std::unique_ptr<OutlineVisual> X11StandalonePlatform::createOutline(Outline *outline)
{
    // first try composited Outline
    auto ret = Platform::createOutline(outline);
    if (!ret) {
        ret = std::make_unique<NonCompositedOutlineVisual>(outline);
    }
    return ret;
}

void X11StandalonePlatform::invertScreen()
{
    using namespace Xcb::RandR;
    bool succeeded = false;

    if (Xcb::Extensions::self()->isRandrAvailable()) {
        const auto active_client = workspace()->activeWindow();
        ScreenResources res((active_client && active_client->window() != XCB_WINDOW_NONE) ? active_client->window() : rootWindow());

        if (!res.isNull()) {
            for (auto crtc : std::span(res.crtcs(), res->num_crtcs)) {
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

template<typename T>
void X11StandalonePlatform::doUpdateOutputs()
{
    QVector<Output *> changed;
    QVector<Output *> added;
    QVector<Output *> removed = m_outputs;

    if (Xcb::Extensions::self()->isRandrAvailable()) {
        T resources(rootWindow());
        if (!resources.isNull()) {

            std::span crtcs(resources.crtcs(), resources->num_crtcs);
            for (auto crtc : crtcs) {
                Xcb::RandR::CrtcInfo info(crtc, resources->config_timestamp);

                const QRect geometry = info.rect();
                if (!geometry.isValid()) {
                    continue;
                }

                float refreshRate = -1.0f;

                for (auto mode : std::span(resources.modes(), resources->num_modes)) {
                    if (info->mode == mode.id) {
                        if (mode.htotal != 0 && mode.vtotal != 0) { // BUG 313996
                            // refresh rate calculation - WTF was wikipedia 1998 when I needed it?
                            int dotclock = mode.dot_clock,
                                vtotal = mode.vtotal;
                            if (mode.mode_flags & XCB_RANDR_MODE_FLAG_INTERLACE) {
                                dotclock *= 2;
                            }
                            if (mode.mode_flags & XCB_RANDR_MODE_FLAG_DOUBLE_SCAN) {
                                vtotal *= 2;
                            }
                            refreshRate = dotclock / float(mode.htotal * vtotal);
                        }
                        break; // found mode
                    }
                }

                for (auto xcbOutput : std::span(info.outputs(), info->num_outputs)) {
                    Xcb::RandR::OutputInfo outputInfo(xcbOutput, resources->config_timestamp);
                    if (outputInfo->crtc != crtc) {
                        continue;
                    }

                    X11Output *output = findX11Output(outputInfo.name());
                    if (output) {
                        changed.append(output);
                        removed.removeOne(output);
                    } else {
                        output = new X11Output(this);
                        added.append(output);
                    }

                    // TODO: Perhaps the output has to save the inherited gamma ramp and
                    // restore it during tear down. Currently neither standalone x11 nor
                    // drm platform do this.
                    Xcb::RandR::CrtcGamma gamma(crtc);

                    output->setRenderLoop(m_renderLoop.get());
                    output->setCrtc(crtc);
                    output->setGammaRampSize(gamma.isNull() ? 0 : gamma->size);
                    auto it = std::find(crtcs.begin(), crtcs.end(), crtc);
                    int crtcIndex = std::distance(crtcs.begin(), it);
                    output->setXineramaNumber(crtcIndex);

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

                    X11Output::Information information{
                        .name = outputInfo.name(),
                        .physicalSize = physicalSize,
                    };

                    auto edidProperty = Xcb::RandR::OutputProperty(xcbOutput, atoms->edid, XCB_ATOM_INTEGER, 0, 100, false, false);
                    bool ok;
                    if (auto data = edidProperty.toByteArray(&ok); ok && !data.isEmpty()) {
                        if (auto edid = Edid(data, edidProperty.data()->num_items); edid.isValid()) {
                            information.manufacturer = edid.manufacturerString();
                            information.model = edid.monitorName();
                            information.serialNumber = edid.serialNumber();
                            information.edid = data;
                        }
                    }

                    auto mode = std::make_shared<OutputMode>(geometry.size(), refreshRate * 1000);

                    X11Output::State state = output->m_state;
                    state.modes = {mode};
                    state.currentMode = mode;
                    state.position = geometry.topLeft();

                    output->setInformation(information);
                    output->setState(state);
                    break;
                }
            }
        }
    }

    // The workspace handles having no outputs poorly. If the last output is about to be
    // removed, create a dummy output to avoid crashing.
    if (changed.isEmpty() && added.isEmpty()) {
        auto dummyOutput = new X11PlaceholderOutput(this);
        m_outputs << dummyOutput;
        Q_EMIT outputAdded(dummyOutput);
        dummyOutput->updateEnabled(true);
    }

    // Process new outputs. Note new outputs must be introduced before removing any other outputs.
    for (Output *output : qAsConst(added)) {
        m_outputs.append(output);
        Q_EMIT outputAdded(output);
        if (auto placeholderOutput = qobject_cast<X11PlaceholderOutput *>(output)) {
            placeholderOutput->updateEnabled(true);
        } else if (auto nativeOutput = qobject_cast<X11Output *>(output)) {
            nativeOutput->updateEnabled(true);
        }
    }

    // Outputs have to be removed last to avoid the case where there are no enabled outputs.
    for (Output *output : qAsConst(removed)) {
        m_outputs.removeOne(output);
        if (auto placeholderOutput = qobject_cast<X11PlaceholderOutput *>(output)) {
            placeholderOutput->updateEnabled(false);
        } else if (auto nativeOutput = qobject_cast<X11Output *>(output)) {
            nativeOutput->updateEnabled(false);
        }
        Q_EMIT outputRemoved(output);
        output->unref();
    }

    // Make sure that the position of an output in m_outputs matches its xinerama index, there
    // are X11 protocols that use xinerama indices to identify outputs.
    std::sort(m_outputs.begin(), m_outputs.end(), [](const Output *a, const Output *b) {
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

    Q_EMIT outputsQueried();
}

X11Output *X11StandalonePlatform::findX11Output(const QString &name) const
{
    for (Output *output : m_outputs) {
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

X11Keyboard *X11StandalonePlatform::keyboard() const
{
    return m_keyboard.get();
}

RenderLoop *X11StandalonePlatform::renderLoop() const
{
    return m_renderLoop.get();
}

static bool refreshRate_compare(const Output *first, const Output *smallest)
{
    return first->refreshRate() < smallest->refreshRate();
}

static int currentRefreshRate()
{
    static const int refreshRate = qEnvironmentVariableIntValue("KWIN_X11_REFRESH_RATE");
    if (refreshRate) {
        return refreshRate;
    }

    const QVector<Output *> outputs = kwinApp()->platform()->outputs();
    if (outputs.isEmpty()) {
        return 60000;
    }

    static const QString syncDisplayDevice = qEnvironmentVariable("__GL_SYNC_DISPLAY_DEVICE");
    if (!syncDisplayDevice.isEmpty()) {
        for (const Output *output : outputs) {
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
