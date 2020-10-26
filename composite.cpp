/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "composite.h"

#include "dbusinterface.h"
#include "x11client.h"
#include "decorations/decoratedclient.h"
#include "deleted.h"
#include "effects.h"
#include "internal_client.h"
#include "overlaywindow.h"
#include "platform.h"
#include "scene.h"
#include "screens.h"
#include "shadow.h"
#include "unmanaged.h"
#include "useractions.h"
#include "utils.h"
#include "wayland_server.h"
#include "workspace.h"
#include "xcbutils.h"

#include <kwingltexture.h>

#include <KWaylandServer/surface_interface.h>

#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginLoader>
#include <KPluginMetaData>
#include <KNotification>
#include <KSelectionOwner>

#include <QDateTime>
#include <QFutureWatcher>
#include <QMenu>
#include <QOpenGLContext>
#include <QQuickWindow>
#include <QtConcurrentRun>
#include <QTextStream>
#include <QTimerEvent>

#include <xcb/composite.h>
#include <xcb/damage.h>

#include <cstdio>

Q_DECLARE_METATYPE(KWin::X11Compositor::SuspendReason)

namespace KWin
{

// See main.cpp:
extern int screen_number;

extern bool is_multihead;
extern int currentRefreshRate();

Compositor *Compositor::s_compositor = nullptr;
Compositor *Compositor::self()
{
    return s_compositor;
}

WaylandCompositor *WaylandCompositor::create(QObject *parent)
{
    Q_ASSERT(!s_compositor);
    auto *compositor = new WaylandCompositor(parent);
    s_compositor = compositor;
    return compositor;
}
X11Compositor *X11Compositor::create(QObject *parent)
{
    Q_ASSERT(!s_compositor);
    auto *compositor = new X11Compositor(parent);
    s_compositor = compositor;
    return compositor;
}

class CompositorSelectionOwner : public KSelectionOwner
{
    Q_OBJECT
public:
    CompositorSelectionOwner(const char *selection)
        : KSelectionOwner(selection, connection(), rootWindow())
        , m_owning(false)
    {
        connect (this, &CompositorSelectionOwner::lostOwnership,
                 this, [this]() { m_owning = false; });
    }
    bool owning() const {
        return m_owning;
    }
    void setOwning(bool own) {
        m_owning = own;
    }
private:
    bool m_owning;
};

static inline qint64 milliToNano(int milli) { return qint64(milli) * 1000 * 1000; }
static inline qint64 nanoToMilli(int nano) { return nano / (1000*1000); }

Compositor::Compositor(QObject* workspace)
    : QObject(workspace)
    , m_state(State::Off)
    , m_selectionOwner(nullptr)
    , vBlankInterval(0)
    , fpsInterval(0)
    , m_timeSinceLastVBlank(0)
    , m_scene(nullptr)
    , m_bufferSwapPending(false)
    , m_composeAtSwapCompletion(false)
{
    connect(options, &Options::configChanged, this, &Compositor::configChanged);
    connect(options, &Options::animationSpeedChanged, this, &Compositor::configChanged);

    m_monotonicClock.start();

    // 2 sec which should be enough to restart the compositor.
    static const int compositorLostMessageDelay = 2000;

    m_releaseSelectionTimer.setSingleShot(true);
    m_releaseSelectionTimer.setInterval(compositorLostMessageDelay);
    connect(&m_releaseSelectionTimer, &QTimer::timeout,
            this, &Compositor::releaseCompositorSelection);

    m_unusedSupportPropertyTimer.setInterval(compositorLostMessageDelay);
    m_unusedSupportPropertyTimer.setSingleShot(true);
    connect(&m_unusedSupportPropertyTimer, &QTimer::timeout,
            this, &Compositor::deleteUnusedSupportProperties);

    // Delay the call to start by one event cycle.
    // The ctor of this class is invoked from the Workspace ctor, that means before
    // Workspace is completely constructed, so calling Workspace::self() would result
    // in undefined behavior. This is fixed by using a delayed invocation.
    if (kwinApp()->platform()->isReady()) {
        QTimer::singleShot(0, this, &Compositor::start);
    }
    connect(kwinApp()->platform(), &Platform::readyChanged, this,
        [this] (bool ready) {
            if (ready) {
                start();
            } else {
                stop();
            }
        }, Qt::QueuedConnection
    );

    if (qEnvironmentVariableIsSet("KWIN_MAX_FRAMES_TESTED"))
       m_framesToTestForSafety = qEnvironmentVariableIntValue("KWIN_MAX_FRAMES_TESTED");

    // register DBus
    new CompositorDBusInterface(this);
}

Compositor::~Compositor()
{
    emit aboutToDestroy();
    stop();
    deleteUnusedSupportProperties();
    destroyCompositorSelection();
    s_compositor = nullptr;
}

bool Compositor::setupStart()
{
    if (kwinApp()->isTerminating()) {
        // Don't start while KWin is terminating. An event to restart might be lingering
        // in the event queue due to graphics reset.
        return false;
    }
    if (m_state != State::Off) {
        return false;
    }
    m_state = State::Starting;

    options->reloadCompositingSettings(true);

    initializeX11();

    // There might still be a deleted around, needs to be cleared before
    // creating the scene (BUG 333275).
    if (Workspace::self()) {
        while (!Workspace::self()->deletedList().isEmpty()) {
            Workspace::self()->deletedList().first()->discard();
        }
    }

    emit aboutToToggleCompositing();

    auto supportedCompositors = kwinApp()->platform()->supportedCompositors();
    const auto userConfigIt = std::find(supportedCompositors.begin(), supportedCompositors.end(),
                                        options->compositingMode());

    if (userConfigIt != supportedCompositors.end()) {
        supportedCompositors.erase(userConfigIt);
        supportedCompositors.prepend(options->compositingMode());
    } else {
        qCWarning(KWIN_CORE)
                << "Configured compositor not supported by Platform. Falling back to defaults";
    }

    const auto availablePlugins = KPluginLoader::findPlugins(QStringLiteral("org.kde.kwin.scenes"));

    for (const KPluginMetaData &pluginMetaData : availablePlugins) {
        qCDebug(KWIN_CORE) << "Available scene plugin:" << pluginMetaData.fileName();
    }

    for (auto type : qAsConst(supportedCompositors)) {
        switch (type) {
        case XRenderCompositing:
            qCDebug(KWIN_CORE) << "Attempting to load the XRender scene";
            break;
        case OpenGLCompositing:
        case OpenGL2Compositing:
            qCDebug(KWIN_CORE) << "Attempting to load the OpenGL scene";
            break;
        case QPainterCompositing:
            qCDebug(KWIN_CORE) << "Attempting to load the QPainter scene";
            break;
        case NoCompositing:
            Q_UNREACHABLE();
        }
        const auto pluginIt = std::find_if(availablePlugins.begin(), availablePlugins.end(),
            [type] (const auto &plugin) {
                const auto &metaData = plugin.rawData();
                auto it = metaData.find(QStringLiteral("CompositingType"));
                if (it != metaData.end()) {
                    if ((*it).toInt() == int{type}) {
                        return true;
                    }
                }
                return false;
            });
        if (pluginIt != availablePlugins.end()) {
            std::unique_ptr<SceneFactory>
                    factory{ qobject_cast<SceneFactory*>(pluginIt->instantiate()) };
            if (factory) {
                m_scene = factory->create(this);
                if (m_scene) {
                    if (!m_scene->initFailed()) {
                        qCDebug(KWIN_CORE) << "Instantiated compositing plugin:"
                                           << pluginIt->name();
                        break;
                    } else {
                        delete m_scene;
                        m_scene = nullptr;
                    }
                }
            }
        }
    }

    if (m_scene == nullptr || m_scene->initFailed()) {
        qCCritical(KWIN_CORE) << "Failed to initialize compositing, compositing disabled";
        m_state = State::Off;

        delete m_scene;
        m_scene = nullptr;

        if (m_selectionOwner) {
            m_selectionOwner->setOwning(false);
            m_selectionOwner->release();
        }
        if (!supportedCompositors.contains(NoCompositing)) {
            qCCritical(KWIN_CORE) << "The used windowing system requires compositing";
            qCCritical(KWIN_CORE) << "We are going to quit KWin now as it is broken";
            qApp->quit();
        }
        return false;
    }

    CompositingType compositingType = m_scene->compositingType();
    if (compositingType & OpenGLCompositing) {
        // Override for OpenGl sub-type OpenGL2Compositing.
        compositingType = OpenGLCompositing;
    }
    kwinApp()->platform()->setSelectedCompositor(compositingType);

    if (!Workspace::self() && m_scene && m_scene->compositingType() == QPainterCompositing) {
        // Force Software QtQuick on first startup with QPainter.
        QQuickWindow::setSceneGraphBackend(QSGRendererInterface::Software);
    }

    connect(m_scene, &Scene::resetCompositing, this, &Compositor::reinitialize);
    emit sceneCreated();

    return true;
}

void Compositor::initializeX11()
{
    xcb_connection_t *connection = kwinApp()->x11Connection();
    if (!connection) {
        return;
    }

    if (!m_selectionOwner) {
        char selection_name[ 100 ];
        sprintf(selection_name, "_NET_WM_CM_S%d", Application::x11ScreenNumber());
        m_selectionOwner = new CompositorSelectionOwner(selection_name);
        connect(m_selectionOwner, &CompositorSelectionOwner::lostOwnership,
                this, &Compositor::stop);
    }
    if (!m_selectionOwner->owning()) {
        // Force claim ownership.
        m_selectionOwner->claim(true);
        m_selectionOwner->setOwning(true);
    }

    xcb_composite_redirect_subwindows(connection, kwinApp()->x11RootWindow(),
                                      XCB_COMPOSITE_REDIRECT_MANUAL);
}

void Compositor::cleanupX11()
{
    delete m_selectionOwner;
    m_selectionOwner = nullptr;
}

void Compositor::startupWithWorkspace()
{
    connect(kwinApp(), &Application::x11ConnectionChanged,
            this, &Compositor::initializeX11, Qt::UniqueConnection);
    connect(kwinApp(), &Application::x11ConnectionAboutToBeDestroyed,
            this, &Compositor::cleanupX11, Qt::UniqueConnection);
    initializeX11();

    Workspace::self()->markXStackingOrderAsDirty();
    Q_ASSERT(m_scene);

    connect(workspace(), &Workspace::destroyed, this, [this] { compositeTimer.stop(); });
    fpsInterval = options->maxFpsInterval();

    if (m_scene->syncsToVBlank()) {
        // If we do vsync, set the fps to the next multiple of the vblank rate.
        vBlankInterval = milliToNano(1000) / currentRefreshRate();
        fpsInterval = qMax((fpsInterval / vBlankInterval) * vBlankInterval, vBlankInterval);
    } else {
        // No vsync - DO NOT set "0", would cause div-by-zero segfaults.
        vBlankInterval = milliToNano(1);
    }

    // Sets also the 'effects' pointer.
    kwinApp()->platform()->createEffectsHandler(this, m_scene);
    connect(Workspace::self(), &Workspace::deletedRemoved, m_scene, &Scene::removeToplevel);
    connect(effects, &EffectsHandler::screenGeometryChanged, this, &Compositor::addRepaintFull);

    for (X11Client *c : Workspace::self()->clientList()) {
        c->setupCompositing();
        c->updateShadow();
    }
    for (Unmanaged *c : Workspace::self()->unmanagedList()) {
        c->setupCompositing();
        c->updateShadow();
    }
    for (InternalClient *client : workspace()->internalClients()) {
        client->setupCompositing();
        client->updateShadow();
    }

    if (auto *server = waylandServer()) {
        const auto clients = server->clients();
        for (AbstractClient *c : clients) {
            c->setupCompositing();
            c->updateShadow();
        }
    }

    m_state = State::On;
    emit compositingToggled(true);

    if (m_releaseSelectionTimer.isActive()) {
        m_releaseSelectionTimer.stop();
    }

    // Render at least once.
    addRepaintFull();
    performCompositing();
}

void Compositor::scheduleRepaint()
{
    if (!compositeTimer.isActive())
        setCompositeTimer();
}

void Compositor::stop()
{
    if (m_state == State::Off || m_state == State::Stopping) {
        return;
    }
    m_state = State::Stopping;
    emit aboutToToggleCompositing();

    m_releaseSelectionTimer.start();

    // Some effects might need access to effect windows when they are about to
    // be destroyed, for example to unreference deleted windows, so we have to
    // make sure that effect windows outlive effects.
    delete effects;
    effects = nullptr;

    if (Workspace::self()) {
        for (X11Client *c : Workspace::self()->clientList()) {
            m_scene->removeToplevel(c);
        }
        for (Unmanaged *c : Workspace::self()->unmanagedList()) {
            m_scene->removeToplevel(c);
        }
        for (InternalClient *client : workspace()->internalClients()) {
            m_scene->removeToplevel(client);
        }
        for (X11Client *c : Workspace::self()->clientList()) {
            c->finishCompositing();
        }
        for (Unmanaged *c : Workspace::self()->unmanagedList()) {
            c->finishCompositing();
        }
        for (InternalClient *client : workspace()->internalClients()) {
            client->finishCompositing();
        }
        if (auto *con = kwinApp()->x11Connection()) {
            xcb_composite_unredirect_subwindows(con, kwinApp()->x11RootWindow(),
                                                XCB_COMPOSITE_REDIRECT_MANUAL);
        }
        while (!workspace()->deletedList().isEmpty()) {
            workspace()->deletedList().first()->discard();
        }
    }

    if (waylandServer()) {
        for (AbstractClient *c : waylandServer()->clients()) {
            m_scene->removeToplevel(c);
        }
        for (AbstractClient *c : waylandServer()->clients()) {
            c->finishCompositing();
        }
    }

    delete m_scene;
    m_scene = nullptr;
    compositeTimer.stop();
    repaints_region = QRegion();

    m_state = State::Off;
    emit compositingToggled(false);
}

void Compositor::destroyCompositorSelection()
{
    delete m_selectionOwner;
    m_selectionOwner = nullptr;
}

void Compositor::releaseCompositorSelection()
{
    switch (m_state) {
    case State::On:
        // We are compositing at the moment. Don't release.
        break;
    case State::Off:
        if (m_selectionOwner) {
            qCDebug(KWIN_CORE) << "Releasing compositor selection";
            m_selectionOwner->setOwning(false);
            m_selectionOwner->release();
        }
        break;
    case State::Starting:
    case State::Stopping:
        // Still starting or shutting down the compositor. Starting might fail
        // or after stopping a restart might follow. So test again later on.
        m_releaseSelectionTimer.start();
        break;
    }
}

void Compositor::keepSupportProperty(xcb_atom_t atom)
{
    m_unusedSupportProperties.removeAll(atom);
}

void Compositor::removeSupportProperty(xcb_atom_t atom)
{
    m_unusedSupportProperties << atom;
    m_unusedSupportPropertyTimer.start();
}

void Compositor::deleteUnusedSupportProperties()
{
    if (m_state == State::Starting || m_state == State::Stopping) {
        // Currently still maybe restarting the compositor.
        m_unusedSupportPropertyTimer.start();
        return;
    }
    if (auto *con = kwinApp()->x11Connection()) {
        for (const xcb_atom_t &atom : qAsConst(m_unusedSupportProperties)) {
            // remove property from root window
            xcb_delete_property(con, kwinApp()->x11RootWindow(), atom);
        }
        m_unusedSupportProperties.clear();
    }
}

void Compositor::configChanged()
{
    reinitialize();
    addRepaintFull();
}

void Compositor::reinitialize()
{
    // Reparse config. Config options will be reloaded by start()
    kwinApp()->config()->reparseConfiguration();

    // Restart compositing
    stop();
    start();

    if (effects) { // start() may fail
        effects->reconfigure();
    }
}

void Compositor::addRepaint(int x, int y, int w, int h)
{
    if (m_state != State::On) {
        return;
    }
    repaints_region += QRegion(x, y, w, h);
    scheduleRepaint();
}

void Compositor::addRepaint(const QRect& r)
{
    if (m_state != State::On) {
        return;
    }
    repaints_region += r;
    scheduleRepaint();
}

void Compositor::addRepaint(const QRegion& r)
{
    if (m_state != State::On) {
        return;
    }
    repaints_region += r;
    scheduleRepaint();
}

void Compositor::addRepaintFull()
{
    if (m_state != State::On) {
        return;
    }
    const QSize &s = screens()->size();
    repaints_region = QRegion(0, 0, s.width(), s.height());
    scheduleRepaint();
}

void Compositor::timerEvent(QTimerEvent *te)
{
    if (te->timerId() == compositeTimer.timerId()) {
        performCompositing();
    } else
        QObject::timerEvent(te);
}

void Compositor::aboutToSwapBuffers()
{
    Q_ASSERT(!m_bufferSwapPending);

    m_bufferSwapPending = true;
}

void Compositor::bufferSwapComplete()
{
    Q_ASSERT(m_bufferSwapPending);
    m_bufferSwapPending = false;

    emit bufferSwapCompleted();

    if (m_composeAtSwapCompletion) {
        m_composeAtSwapCompletion = false;
        performCompositing();
    }
}

void Compositor::performCompositing()
{
    // If a buffer swap is still pending, we return to the event loop and
    // continue processing events until the swap has completed.
    if (m_bufferSwapPending) {
        m_composeAtSwapCompletion = true;
        compositeTimer.stop();
        return;
    }

    // If outputs are disabled, we return to the event loop and
    // continue processing events until the outputs are enabled again
    if (!kwinApp()->platform()->areOutputsEnabled()) {
        compositeTimer.stop();
        return;
    }

    // Create a list of all windows in the stacking order
    QList<Toplevel *> windows = Workspace::self()->xStackingOrder();
    QList<Toplevel *> damaged;

    // Reset the damage state of each window and fetch the damage region
    // without waiting for a reply
    for (Toplevel *win : windows) {
        if (win->resetAndFetchDamage()) {
            damaged << win;
        }
    }

    if (damaged.count() > 0) {
        m_scene->triggerFence();
        if (auto c = kwinApp()->x11Connection()) {
            xcb_flush(c);
        }
    }

    // Move elevated windows to the top of the stacking order
    for (EffectWindow *c : static_cast<EffectsHandlerImpl *>(effects)->elevatedWindows()) {
        Toplevel *t = static_cast<EffectWindowImpl *>(c)->window();
        windows.removeAll(t);
        windows.append(t);
    }

    // Get the replies
    for (Toplevel *win : damaged) {
        // Discard the cached lanczos texture
        if (win->effectWindow()) {
            const QVariant texture = win->effectWindow()->data(LanczosCacheRole);
            if (texture.isValid()) {
                delete static_cast<GLTexture *>(texture.value<void*>());
                win->effectWindow()->setData(LanczosCacheRole, QVariant());
            }
        }

        win->getDamageRegionReply();
    }

    if (repaints_region.isEmpty() && !windowRepaintsPending()) {
        m_scene->idle();
        m_timeSinceLastVBlank = fpsInterval - (options->vBlankTime() + 1); // means "start now"
        // Note: It would seem here we should undo suspended unredirect, but when scenes need
        // it for some reason, e.g. transformations or translucency, the next pass that does not
        // need this anymore and paints normally will also reset the suspended unredirect.
        // Otherwise the window would not be painted normally anyway.
        compositeTimer.stop();
        return;
    }

    // Skip windows that are not yet ready for being painted and if screen is locked skip windows
    // that are neither lockscreen nor inputmethod windows.
    //
    // TODO? This cannot be used so carelessly - needs protections against broken clients, the
    // window should not get focus before it's displayed, handle unredirected windows properly and
    // so on.
    for (Toplevel *win : windows) {
        if (!win->readyForPainting()) {
            windows.removeAll(win);
        }
        if (waylandServer() && waylandServer()->isScreenLocked()) {
            if(!win->isLockScreen() && !win->isInputMethod()) {
                windows.removeAll(win);
            }
        }
    }

    QRegion repaints = repaints_region;
    // clear all repaints, so that post-pass can add repaints for the next repaint
    repaints_region = QRegion();

    if (m_framesToTestForSafety > 0 && (m_scene->compositingType() & OpenGLCompositing)) {
        kwinApp()->platform()->createOpenGLSafePoint(Platform::OpenGLSafePoint::PreFrame);
    }
    m_timeSinceLastVBlank = m_scene->paint(repaints, windows);
    if (m_framesToTestForSafety > 0) {
        if (m_scene->compositingType() & OpenGLCompositing) {
            kwinApp()->platform()->createOpenGLSafePoint(Platform::OpenGLSafePoint::PostFrame);
        }
        m_framesToTestForSafety--;
        if (m_framesToTestForSafety == 0 && (m_scene->compositingType() & OpenGLCompositing)) {
            kwinApp()->platform()->createOpenGLSafePoint(
                Platform::OpenGLSafePoint::PostLastGuardedFrame);
        }
    }

    if (waylandServer()) {
        const auto currentTime = static_cast<quint32>(m_monotonicClock.elapsed());
        for (Toplevel *win : qAsConst(windows)) {
            if (auto surface = win->surface()) {
                surface->frameRendered(currentTime);
            }
        }
        if (!kwinApp()->platform()->isCursorHidden()) {
            Cursors::self()->currentCursor()->markAsRendered();
        }
    }

    // Stop here to ensure *we* cause the next repaint schedule - not some effect
    // through m_scene->paint().
    compositeTimer.stop();

    // Trigger at least one more pass even if there would be nothing to paint, so that scene->idle()
    // is called the next time. If there would be nothing pending, it will not restart the timer and
    // scheduleRepaint() would restart it again somewhen later, called from functions that
    // would again add something pending.
    if (m_bufferSwapPending && m_scene->syncsToVBlank()) {
        m_composeAtSwapCompletion = true;
    } else {
        scheduleRepaint();
    }
}

template <class T>
static bool repaintsPending(const QList<T*> &windows)
{
    return std::any_of(windows.begin(), windows.end(),
                       [](T *t) { return !t->repaints().isEmpty(); });
}

bool Compositor::windowRepaintsPending() const
{
    if (repaintsPending(Workspace::self()->clientList())) {
        return true;
    }
    if (repaintsPending(Workspace::self()->unmanagedList())) {
        return true;
    }
    if (repaintsPending(Workspace::self()->deletedList())) {
        return true;
    }
    if (auto *server = waylandServer()) {
        const auto &clients = server->clients();
        auto test = [](AbstractClient *c) {
            return c->readyForPainting() && !c->repaints().isEmpty();
        };
        if (std::any_of(clients.begin(), clients.end(), test)) {
            return true;
        }
    }
    const auto &internalClients = workspace()->internalClients();
    auto internalTest = [] (InternalClient *client) {
        return client->isShown(true) && !client->repaints().isEmpty();
    };
    if (std::any_of(internalClients.begin(), internalClients.end(), internalTest)) {
        return true;
    }
    return false;
}

void Compositor::setCompositeTimer()
{
    if (m_state != State::On) {
        return;
    }

    // Don't start the timer if we're waiting for a swap event
    if (m_bufferSwapPending && m_composeAtSwapCompletion)
        return;

    // Don't start the timer if all outputs are disabled
    if (!kwinApp()->platform()->areOutputsEnabled()) {
        return;
    }

    uint waitTime = 1;

    if (m_scene->blocksForRetrace()) {

        // TODO: make vBlankTime dynamic?!
        // It's required because glXWaitVideoSync will *likely* block a full frame if one enters
        // a retrace pass which can last a variable amount of time, depending on the actual screen
        // Now, my ooold 19" CRT can do such retrace so that 2ms are entirely sufficient,
        // while another ooold 15" TFT requires about 6ms

        qint64 padding = m_timeSinceLastVBlank;
        if (padding > fpsInterval) {
            // We're at low repaints or spent more time in painting than the user wanted to wait
            // for that frame. Align to next vblank:
            padding = vBlankInterval - (padding % vBlankInterval);
        } else {
            // Align to the next maxFps tick:
            // "remaining time of the first vsync" + "time for the other vsyncs of the frame"
            padding = ((vBlankInterval - padding % vBlankInterval) +
                       (fpsInterval / vBlankInterval - 1) * vBlankInterval);
        }

        if (padding < options->vBlankTime()) {
            // We'll likely miss this frame so we add one:
            waitTime = nanoToMilli(padding + vBlankInterval - options->vBlankTime());
        } else {
            waitTime = nanoToMilli(padding - options->vBlankTime());
        }
    }
    else { // w/o blocking vsync we just jump to the next demanded tick
        if (fpsInterval > m_timeSinceLastVBlank) {
            waitTime = nanoToMilli(fpsInterval - m_timeSinceLastVBlank);
            if (!waitTime) {
                // Will ensure we don't block out the eventloop - the system's just not faster ...
                waitTime = 1;
            }
        }
        /* else if (m_scene->syncsToVBlank() && m_timeSinceLastVBlank - fpsInterval < (vBlankInterval<<1)) {
            // NOTICE - "for later" ------------------------------------------------------------------
            // It can happen that we push two frames within one refresh cycle.
            // Swapping will then block even with triple buffering when the GPU does not discard but
            // queues frames
            // now here's the mean part: if we take that as "OMG, we're late - next frame ASAP",
            // there'll immediately be 2 frames in the pipe, swapping will block, we think we're
            // late ... ewww
            // so instead we pad to the clock again and add 2ms safety to ensure the pipe is really
            // free
            // NOTICE: obviously m_timeSinceLastVBlank can be too big because we're too slow as well
            // So if this code was enabled, we'd needlessly half the framerate once more (15 instead of 30)
            waitTime = nanoToMilli(vBlankInterval - (m_timeSinceLastVBlank - fpsInterval)%vBlankInterval) + 2;
        }*/
        else {
            // "0" would be sufficient here, but the compositor isn't the WMs only task.
            waitTime = 1;
        }
    }
    // Force 4fps minimum:
    compositeTimer.start(qMin(waitTime, 250u), this);
}

bool Compositor::isActive()
{
    return m_state == State::On;
}

WaylandCompositor::WaylandCompositor(QObject *parent)
    : Compositor(parent)
{
    connect(kwinApp(), &Application::x11ConnectionAboutToBeDestroyed,
            this, &WaylandCompositor::destroyCompositorSelection);
}

void WaylandCompositor::toggleCompositing()
{
    // For the shortcut. Not possible on Wayland because we always composite.
}

void WaylandCompositor::start()
{
    if (!Compositor::setupStart()) {
        // Internal setup failed, abort.
        return;
    }

    if (Workspace::self()) {
        startupWithWorkspace();
    } else {
        connect(kwinApp(), &Application::workspaceCreated,
                this, &WaylandCompositor::startupWithWorkspace);
    }
}

int WaylandCompositor::refreshRate() const
{
    // TODO: This makes no sense on Wayland. First step would be to atleast
    //       set the refresh rate to the highest available one. Second step
    //       would be to not use a uniform value at all but per screen.
    return KWin::currentRefreshRate();
}

X11Compositor::X11Compositor(QObject *parent)
    : Compositor(parent)
    , m_suspended(options->isUseCompositing() ? NoReasonSuspend : UserSuspend)
    , m_xrrRefreshRate(0)
{
}

void X11Compositor::toggleCompositing()
{
    if (m_suspended) {
        // Direct user call; clear all bits.
        resume(AllReasonSuspend);
    } else {
        // But only set the user one (sufficient to suspend).
        suspend(UserSuspend);
    }
}

void X11Compositor::reinitialize()
{
    // Resume compositing if suspended.
    m_suspended = NoReasonSuspend;
    Compositor::reinitialize();
}

void X11Compositor::configChanged()
{
    if (m_suspended) {
        stop();
        return;
    }
    Compositor::configChanged();
}

void X11Compositor::suspend(X11Compositor::SuspendReason reason)
{
    Q_ASSERT(reason != NoReasonSuspend);
    m_suspended |= reason;

    if (reason & ScriptSuspend) {
        // When disabled show a shortcut how the user can get back compositing.
        const auto shortcuts = KGlobalAccel::self()->shortcut(
            workspace()->findChild<QAction*>(QStringLiteral("Suspend Compositing")));
        if (!shortcuts.isEmpty()) {
            // Display notification only if there is the shortcut.
            const QString message =
                    i18n("Desktop effects have been suspended by another application.<br/>"
                         "You can resume using the '%1' shortcut.",
                         shortcuts.first().toString(QKeySequence::NativeText));
            KNotification::event(QStringLiteral("compositingsuspendeddbus"), message);
        }
    }
    stop();
}

void X11Compositor::resume(X11Compositor::SuspendReason reason)
{
    Q_ASSERT(reason != NoReasonSuspend);
    m_suspended &= ~reason;
    start();
}

void X11Compositor::start()
{
    if (m_suspended) {
        QStringList reasons;
        if (m_suspended & UserSuspend) {
            reasons << QStringLiteral("Disabled by User");
        }
        if (m_suspended & BlockRuleSuspend) {
            reasons << QStringLiteral("Disabled by Window");
        }
        if (m_suspended & ScriptSuspend) {
            reasons << QStringLiteral("Disabled by Script");
        }
        qCDebug(KWIN_CORE) << "Compositing is suspended, reason:" << reasons;
        return;
    } else if (!kwinApp()->platform()->compositingPossible()) {
        qCCritical(KWIN_CORE) << "Compositing is not possible";
        return;
    }
    if (!Compositor::setupStart()) {
        // Internal setup failed, abort.
        return;
    }
    m_xrrRefreshRate = KWin::currentRefreshRate();
    startupWithWorkspace();
}
void X11Compositor::performCompositing()
{
    if (scene()->usesOverlayWindow() && !isOverlayWindowVisible()) {
        // Return since nothing is visible.
        return;
    }
    Compositor::performCompositing();
}

bool X11Compositor::checkForOverlayWindow(WId w) const
{
    if (!scene()) {
        // No scene, so it cannot be the overlay window.
        return false;
    }
    if (!scene()->overlayWindow()) {
        // No overlay window, it cannot be the overlay.
        return false;
    }
    // Compare the window ID's.
    return w == scene()->overlayWindow()->window();
}

bool X11Compositor::isOverlayWindowVisible() const
{
    if (!scene()) {
        return false;
    }
    if (!scene()->overlayWindow()) {
        return false;
    }
    return scene()->overlayWindow()->isVisible();
}

int X11Compositor::refreshRate() const
{
    return m_xrrRefreshRate;
}

void X11Compositor::updateClientCompositeBlocking(X11Client *c)
{
    if (c) {
        if (c->isBlockingCompositing()) {
            // Do NOT attempt to call suspend(true) from within the eventchain!
            if (!(m_suspended & BlockRuleSuspend))
                QMetaObject::invokeMethod(this, [this]() {
                        suspend(BlockRuleSuspend);
                    }, Qt::QueuedConnection);
        }
    }
    else if (m_suspended & BlockRuleSuspend) {
        // If !c we just check if we can resume in case a blocking client was lost.
        bool shouldResume = true;

        for (auto it = Workspace::self()->clientList().constBegin();
             it != Workspace::self()->clientList().constEnd(); ++it) {
            if ((*it)->isBlockingCompositing()) {
                shouldResume = false;
                break;
            }
        }
        if (shouldResume) {
            // Do NOT attempt to call suspend(false) from within the eventchain!
                QMetaObject::invokeMethod(this, [this]() {
                        resume(BlockRuleSuspend);
                    }, Qt::QueuedConnection);
        }
    }
}

X11Compositor *X11Compositor::self()
{
    return qobject_cast<X11Compositor *>(Compositor::self());
}

}

// included for CompositorSelectionOwner
#include "composite.moc"
