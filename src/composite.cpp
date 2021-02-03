/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "composite.h"
#include "abstract_output.h"
#include "dbusinterface.h"
#include "x11client.h"
#include "decorations/decoratedclient.h"
#include "deleted.h"
#include "effects.h"
#include "ftrace.h"
#include "internal_client.h"
#include "overlaywindow.h"
#include "platform.h"
#include "renderloop.h"
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

Compositor::Compositor(QObject* workspace)
    : QObject(workspace)
    , m_state(State::Off)
    , m_selectionOwner(nullptr)
    , m_scene(nullptr)
{
    connect(options, &Options::configChanged, this, &Compositor::configChanged);
    connect(options, &Options::animationSpeedChanged, this, &Compositor::configChanged);

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
    FTraceLogger::create();
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

    const Platform *platform = kwinApp()->platform();
    if (platform->isPerScreenRenderingEnabled()) {
        const QVector<AbstractOutput *> outputs = platform->enabledOutputs();
        for (AbstractOutput *output : outputs) {
            registerRenderLoop(output->renderLoop(), output);
        }
        connect(platform, &Platform::outputEnabled,
                this, &Compositor::handleOutputEnabled);
        connect(platform, &Platform::outputDisabled,
                this, &Compositor::handleOutputDisabled);
    } else {
        registerRenderLoop(platform->renderLoop(), nullptr);
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
}

void Compositor::registerRenderLoop(RenderLoop *renderLoop, AbstractOutput *output)
{
    Q_ASSERT(!m_renderLoops.contains(renderLoop));
    m_renderLoops.insert(renderLoop, output);
    connect(renderLoop, &RenderLoop::frameRequested, this, &Compositor::handleFrameRequested);
}

void Compositor::unregisterRenderLoop(RenderLoop *renderLoop)
{
    Q_ASSERT(m_renderLoops.contains(renderLoop));
    m_renderLoops.remove(renderLoop);
    disconnect(renderLoop, &RenderLoop::frameRequested, this, &Compositor::handleFrameRequested);
}

void Compositor::handleOutputEnabled(AbstractOutput *output)
{
    registerRenderLoop(output->renderLoop(), output);
}

void Compositor::handleOutputDisabled(AbstractOutput *output)
{
    unregisterRenderLoop(output->renderLoop());
}

int Compositor::screenForRenderLoop(RenderLoop *renderLoop) const
{
    Q_ASSERT(m_renderLoops.contains(renderLoop));
    AbstractOutput *output = m_renderLoops.value(renderLoop);
    if (!output) {
        return -1;
    }
    return kwinApp()->platform()->enabledOutputs().indexOf(output);
}

void Compositor::scheduleRepaint()
{
    for (auto it = m_renderLoops.constBegin(); it != m_renderLoops.constEnd(); ++it) {
        it.key()->scheduleRepaint();
    }
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

    while (!m_renderLoops.isEmpty()) {
        unregisterRenderLoop(m_renderLoops.firstKey());
    }

    disconnect(kwinApp()->platform(), &Platform::outputEnabled,
               this, &Compositor::handleOutputEnabled);
    disconnect(kwinApp()->platform(), &Platform::outputDisabled,
               this, &Compositor::handleOutputDisabled);

    delete m_scene;
    m_scene = nullptr;

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

void Compositor::addRepaint(int x, int y, int width, int height)
{
    addRepaint(QRegion(x, y, width, height));
}

void Compositor::addRepaint(const QRect &rect)
{
    addRepaint(QRegion(rect));
}

void Compositor::addRepaint(const QRegion &region)
{
    if (m_scene) {
        m_scene->addRepaint(region);
    }
}

void Compositor::addRepaintFull()
{
    addRepaint(screens()->geometry());
}

void Compositor::handleFrameRequested(RenderLoop *renderLoop)
{
    // If outputs are disabled, we return to the event loop and
    // continue processing events until the outputs are enabled again
    if (kwinApp()->platform()->areOutputsEnabled()) {
        composite(renderLoop);
    }
}

void Compositor::composite(RenderLoop *renderLoop)
{
    const int screenId = screenForRenderLoop(renderLoop);

    fTraceDuration("Paint (", screens()->name(screenId), ")");

    // Create a list of all windows in the stacking order
    QList<Toplevel *> windows = Workspace::self()->xStackingOrder();

    // Move elevated windows to the top of the stacking order
    for (EffectWindow *c : static_cast<EffectsHandlerImpl *>(effects)->elevatedWindows()) {
        Toplevel *t = static_cast<EffectWindowImpl *>(c)->window();
        windows.removeAll(t);
        windows.append(t);
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

    if (m_framesToTestForSafety > 0 && (m_scene->compositingType() & OpenGLCompositing)) {
        kwinApp()->platform()->createOpenGLSafePoint(Platform::OpenGLSafePoint::PreFrame);
    }

    const QRegion repaints = m_scene->repaints(screenId);
    m_scene->resetRepaints(screenId);

    m_scene->paint(screenId, repaints, windows, renderLoop);

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
        const std::chrono::milliseconds frameTime =
                std::chrono::duration_cast<std::chrono::milliseconds>(renderLoop->lastPresentationTimestamp());

        for (Toplevel *window : qAsConst(windows)) {
            if (!window->readyForPainting()) {
                continue;
            }
            if (waylandServer()->isScreenLocked() &&
                    !(window->isLockScreen() || window->isInputMethod())) {
                continue;
            }
            if (!window->isOnScreen(screenId)) {
                continue;
            }
            if (auto surface = window->surface()) {
                surface->frameRendered(frameTime.count());
            }
        }
        if (!kwinApp()->platform()->isCursorHidden()) {
            Cursors::self()->currentCursor()->markAsRendered();
        }
    }
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

X11Compositor::X11Compositor(QObject *parent)
    : Compositor(parent)
    , m_suspended(options->isUseCompositing() ? NoReasonSuspend : UserSuspend)
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
    startupWithWorkspace();
}

void X11Compositor::composite(RenderLoop *renderLoop)
{
    if (scene()->overlayWindow() && !isOverlayWindowVisible()) {
        // Return since nothing is visible.
        return;
    }

    QList<Toplevel *> windows = Workspace::self()->xStackingOrder();
    QList<Toplevel *> damaged;

    // Reset the damage state of each window and fetch the damage region
    // without waiting for a reply
    for (Toplevel *win : qAsConst(windows)) {
        if (win->resetAndFetchDamage()) {
            damaged << win;
        }
    }

    if (damaged.count() > 0) {
        scene()->triggerFence();
        if (auto c = kwinApp()->x11Connection()) {
            xcb_flush(c);
        }
    }

    // Get the replies
    for (Toplevel *window : qAsConst(damaged)) {
        window->getDamageRegionReply();
    }

    Compositor::composite(renderLoop);
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
