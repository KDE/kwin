/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

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
#include "composite.h"

#include "dbusinterface.h"
#include "client.h"
#include "decorations/decoratedclient.h"
#include "deleted.h"
#include "effects.h"
#include "overlaywindow.h"
#include "platform.h"
#include "scene.h"
#include "screens.h"
#include "shadow.h"
#include "shell_client.h"
#include "unmanaged.h"
#include "useractions.h"
#include "utils.h"
#include "wayland_server.h"
#include "workspace.h"
#include "xcbutils.h"

#include <kwingltexture.h>

#include <KWayland/Server/surface_interface.h>

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

Q_DECLARE_METATYPE(KWin::Compositor::SuspendReason)

namespace KWin
{

extern int currentRefreshRate();

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

KWIN_SINGLETON_FACTORY_VARIABLE(Compositor, s_compositor)

static inline qint64 milliToNano(int milli) { return qint64(milli) * 1000 * 1000; }
static inline qint64 nanoToMilli(int nano) { return nano / (1000*1000); }

Compositor::Compositor(QObject* workspace)
    : QObject(workspace)
    , m_suspended(options->isUseCompositing() ? NoReasonSuspend : UserSuspend)
    , m_selectionOwner(NULL)
    , vBlankInterval(0)
    , fpsInterval(0)
    , m_xrrRefreshRate(0)
    , m_finishing(false)
    , m_starting(false)
    , m_timeSinceLastVBlank(0)
    , m_scene(NULL)
    , m_bufferSwapPending(false)
    , m_composeAtSwapCompletion(false)
{
    qRegisterMetaType<Compositor::SuspendReason>("Compositor::SuspendReason");
    connect(&compositeResetTimer, &QTimer::timeout, this, &Compositor::restart);
    connect(options, &Options::configChanged, this, &Compositor::slotConfigChanged);
    compositeResetTimer.setSingleShot(true);
    m_monotonicClock.start();

    // 2 sec which should be enough to restart the compositor
    static const int compositorLostMessageDelay = 2000;

    m_releaseSelectionTimer.setSingleShot(true);
    m_releaseSelectionTimer.setInterval(compositorLostMessageDelay);
    connect(&m_releaseSelectionTimer, &QTimer::timeout, this, &Compositor::releaseCompositorSelection);

    m_unusedSupportPropertyTimer.setInterval(compositorLostMessageDelay);
    m_unusedSupportPropertyTimer.setSingleShot(true);
    connect(&m_unusedSupportPropertyTimer, &QTimer::timeout, this, &Compositor::deleteUnusedSupportProperties);

    // delay the call to setup by one event cycle
    // The ctor of this class is invoked from the Workspace ctor, that means before
    // Workspace is completely constructed, so calling Workspace::self() would result
    // in undefined behavior. This is fixed by using a delayed invocation.
    if (kwinApp()->platform()->isReady()) {
        QMetaObject::invokeMethod(this, "setup", Qt::QueuedConnection);
    }
    connect(kwinApp()->platform(), &Platform::readyChanged, this,
        [this] (bool ready) {
            if (ready) {
                setup();
            } else {
                finish();
            }
        }, Qt::QueuedConnection
    );
    connect(kwinApp(), &Application::x11ConnectionAboutToBeDestroyed, this,
        [this] {
            delete m_selectionOwner;
            m_selectionOwner = nullptr;
        }
    );

    if (qEnvironmentVariableIsSet("KWIN_MAX_FRAMES_TESTED"))
       m_framesToTestForSafety = qEnvironmentVariableIntValue("KWIN_MAX_FRAMES_TESTED");

    // register DBus
    new CompositorDBusInterface(this);
}

Compositor::~Compositor()
{
    emit aboutToDestroy();
    finish();
    deleteUnusedSupportProperties();
    delete m_selectionOwner;
    s_compositor = NULL;
}


void Compositor::setup()
{
    if (kwinApp()->isTerminating()) {
        // don't setup while KWin is terminating. An event to restart might be lingering in the event queue due to graphics reset
        return;
    }
    if (hasScene())
        return;
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
    m_starting = true;

    options->reloadCompositingSettings(true);
    slotCompositingOptionsInitialized();
}

extern int screen_number; // main.cpp
extern bool is_multihead;

void Compositor::slotCompositingOptionsInitialized()
{
    setupX11Support();

    // There might still be a deleted around, needs to be cleared before creating the scene (BUG 333275)
    if (Workspace::self()) {
        while (!Workspace::self()->deletedList().isEmpty()) {
            Workspace::self()->deletedList().first()->discard();
        }
    }

    emit aboutToToggleCompositing();

    auto supportedCompositors = kwinApp()->platform()->supportedCompositors();
    const auto userConfigIt = std::find(supportedCompositors.begin(), supportedCompositors.end(), options->compositingMode());
    if (userConfigIt != supportedCompositors.end()) {
        supportedCompositors.erase(userConfigIt);
        supportedCompositors.prepend(options->compositingMode());
    } else {
        qCWarning(KWIN_CORE) << "Configured compositor not supported by Platform. Falling back to defaults";
    }

    const auto availablePlugins = KPluginLoader::findPlugins(QStringLiteral("org.kde.kwin.scenes"));

    for (auto type : qAsConst(supportedCompositors)) {
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
            std::unique_ptr<SceneFactory> factory{qobject_cast<SceneFactory*>(pluginIt->instantiate())};
            if (factory) {
                m_scene = factory->create(this);
                if (m_scene) {
                    if (!m_scene->initFailed()) {
                        qCDebug(KWIN_CORE) << "Instantiated compositing plugin:" << pluginIt->name();
                        break;
                    } else {
                        delete m_scene;
                        m_scene = nullptr;
                    }
                }
            }
        }
    }

    if (m_scene == NULL || m_scene->initFailed()) {
        qCCritical(KWIN_CORE) << "Failed to initialize compositing, compositing disabled";
        delete m_scene;
        m_scene = NULL;
        m_starting = false;
        if (m_selectionOwner) {
            m_selectionOwner->setOwning(false);
            m_selectionOwner->release();
        }
        if (!supportedCompositors.contains(NoCompositing)) {
            qCCritical(KWIN_CORE) << "The used windowing system requires compositing";
            qCCritical(KWIN_CORE) << "We are going to quit KWin now as it is broken";
            qApp->quit();
        }
        return;
    }

    kwinApp()->platform()->setSelectedCompositor(m_scene->compositingType() & OpenGLCompositing ? OpenGLCompositing : m_scene->compositingType());

    if (!Workspace::self() && m_scene && m_scene->compositingType() == QPainterCompositing) {
        // Force Software QtQuick on first startup with QPainter
        QQuickWindow::setSceneGraphBackend(QSGRendererInterface::Software);
    }

    connect(m_scene, &Scene::resetCompositing, this, &Compositor::restart);
    emit sceneCreated();

    if (Workspace::self()) {
        startupWithWorkspace();
    } else {
        connect(kwinApp(), &Application::workspaceCreated, this, &Compositor::startupWithWorkspace);
    }
}

void Compositor::claimCompositorSelection()
{
    if (!m_selectionOwner) {
        char selection_name[ 100 ];
        sprintf(selection_name, "_NET_WM_CM_S%d", Application::x11ScreenNumber());
        m_selectionOwner = new CompositorSelectionOwner(selection_name);
        connect(m_selectionOwner, &CompositorSelectionOwner::lostOwnership, this, &Compositor::finish);
    }

    if (!m_selectionOwner) // no X11 yet
        return;

    if (!m_selectionOwner->owning()) {
        m_selectionOwner->claim(true);   // force claiming
        m_selectionOwner->setOwning(true);
    }
}

void Compositor::setupX11Support()
{
    auto c = kwinApp()->x11Connection();
    if (!c) {
        delete m_selectionOwner;
        m_selectionOwner = nullptr;
        return;
    }
    claimCompositorSelection();
    xcb_composite_redirect_subwindows(c, kwinApp()->x11RootWindow(), XCB_COMPOSITE_REDIRECT_MANUAL);
}

void Compositor::startupWithWorkspace()
{
    if (!m_starting) {
        return;
    }
    connect(kwinApp(), &Application::x11ConnectionChanged, this, &Compositor::setupX11Support, Qt::UniqueConnection);
    Workspace::self()->markXStackingOrderAsDirty();
    Q_ASSERT(m_scene);
    connect(workspace(), &Workspace::destroyed, this, [this] { compositeTimer.stop(); });
    setupX11Support();
    m_xrrRefreshRate = KWin::currentRefreshRate();
    fpsInterval = options->maxFpsInterval();
    if (m_scene->syncsToVBlank()) {  // if we do vsync, set the fps to the next multiple of the vblank rate
        vBlankInterval = milliToNano(1000) / m_xrrRefreshRate;
        fpsInterval = qMax((fpsInterval / vBlankInterval) * vBlankInterval, vBlankInterval);
    } else
        vBlankInterval = milliToNano(1); // no sync - DO NOT set "0", would cause div-by-zero segfaults.
    m_timeSinceLastVBlank = fpsInterval - (options->vBlankTime() + 1); // means "start now" - we don't have even a slight idea when the first vsync will occur
    scheduleRepaint();
    kwinApp()->platform()->createEffectsHandler(this, m_scene);   // sets also the 'effects' pointer
    connect(Workspace::self(), &Workspace::deletedRemoved, m_scene, &Scene::removeToplevel);
    connect(effects, &EffectsHandler::screenGeometryChanged, this, &Compositor::addRepaintFull);
    addRepaintFull();
    foreach (Client * c, Workspace::self()->clientList()) {
        c->setupCompositing();
        c->getShadow();
    }
    foreach (Client * c,  Workspace::self()->desktopList())
        c->setupCompositing();
    foreach (Unmanaged * c, Workspace::self()->unmanagedList()) {
        c->setupCompositing();
        c->getShadow();
    }
    if (auto w = waylandServer()) {
        const auto clients = w->clients();
        for (auto c : clients) {
            c->setupCompositing();
            c->getShadow();
        }
        const auto internalClients = w->internalClients();
        for (auto c : internalClients) {
            c->setupCompositing();
            c->getShadow();
        }
    }

    emit compositingToggled(true);

    m_starting = false;
    if (m_releaseSelectionTimer.isActive()) {
        m_releaseSelectionTimer.stop();
    }

    // render at least once
    performCompositing();
}

void Compositor::scheduleRepaint()
{
    if (!compositeTimer.isActive())
        setCompositeTimer();
}

void Compositor::finish()
{
    if (!hasScene())
        return;
    m_finishing = true;
    m_releaseSelectionTimer.start();

    emit aboutToToggleCompositing();

    // Some effects might need access to effect windows when they are about to
    // be destroyed, for example to unreference deleted windows, so we have to
    // make sure that effect windows outlive effects.
    delete effects;
    effects = nullptr;

    if (Workspace::self()) {
        foreach (Client * c, Workspace::self()->clientList())
            m_scene->removeToplevel(c);
        foreach (Client * c, Workspace::self()->desktopList())
            m_scene->removeToplevel(c);
        foreach (Unmanaged * c, Workspace::self()->unmanagedList())
            m_scene->removeToplevel(c);
        foreach (Client * c, Workspace::self()->clientList())
        c->finishCompositing();
        foreach (Client * c, Workspace::self()->desktopList())
        c->finishCompositing();
        foreach (Unmanaged * c, Workspace::self()->unmanagedList())
        c->finishCompositing();
        if (auto c = kwinApp()->x11Connection()) {
            xcb_composite_unredirect_subwindows(c, kwinApp()->x11RootWindow(), XCB_COMPOSITE_REDIRECT_MANUAL);
        }
        while (!workspace()->deletedList().isEmpty()) {
            workspace()->deletedList().first()->discard();
        }
    }
    if (waylandServer()) {
        foreach (ShellClient *c, waylandServer()->clients()) {
            m_scene->removeToplevel(c);
        }
        foreach (ShellClient *c, waylandServer()->internalClients()) {
            m_scene->removeToplevel(c);
        }
        foreach (ShellClient *c, waylandServer()->clients()) {
            c->finishCompositing();
        }
        foreach (ShellClient *c, waylandServer()->internalClients()) {
            c->finishCompositing();
        }
    }
    delete m_scene;
    m_scene = NULL;
    compositeTimer.stop();
    repaints_region = QRegion();
    m_finishing = false;
    emit compositingToggled(false);
}

void Compositor::releaseCompositorSelection()
{
    if (hasScene() && !m_finishing) {
        // compositor is up and running again, no need to release the selection
        return;
    }
    if (m_starting) {
        // currently still starting the compositor, it might fail, so restart the timer to test again
        m_releaseSelectionTimer.start();
        return;
    }

    if (m_finishing) {
        // still shutting down, a restart might follow, so restart the timer to test again
        m_releaseSelectionTimer.start();
        return;
    }
    qCDebug(KWIN_CORE) << "Releasing compositor selection";
    if (m_selectionOwner) {
        m_selectionOwner->setOwning(false);
        m_selectionOwner->release();
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
    if (m_starting) {
        // currently still starting the compositor
        m_unusedSupportPropertyTimer.start();
        return;
    }
    if (m_finishing) {
        // still shutting down, a restart might follow
        m_unusedSupportPropertyTimer.start();
        return;
    }
    if (const auto c = kwinApp()->x11Connection()) {
        foreach (const xcb_atom_t &atom, m_unusedSupportProperties) {
            // remove property from root window
            xcb_delete_property(c, kwinApp()->x11RootWindow(), atom);
        }
    }
}

void Compositor::slotConfigChanged()
{
    if (!m_suspended) {
        setup();
        if (effects)   // setupCompositing() may fail
            effects->reconfigure();
        addRepaintFull();
    } else
        finish();
}

void Compositor::slotReinitialize()
{
    // Reparse config. Config options will be reloaded by setup()
    kwinApp()->config()->reparseConfiguration();

    // Restart compositing
    finish();
    // resume compositing if suspended
    m_suspended = NoReasonSuspend;
    setup();

    if (effects) { // setup() may fail
        effects->reconfigure();
    }
}

// for the shortcut
void Compositor::toggleCompositing()
{
    if (kwinApp()->platform()->requiresCompositing()) {
        // we are not allowed to turn on/off compositing
        return;
    }
    if (m_suspended) { // direct user call; clear all bits
        resume(AllReasonSuspend);
    } else { // but only set the user one (sufficient to suspend)
        suspend(UserSuspend);
    }
}

void Compositor::updateCompositeBlocking()
{
    updateClientCompositeBlocking(NULL);
}

void Compositor::updateClientCompositeBlocking(Client *c)
{
    if (kwinApp()->platform()->requiresCompositing()) {
        return;
    }
    if (c) { // if c == 0 we just check if we can resume
        if (c->isBlockingCompositing()) {
            if (!(m_suspended & BlockRuleSuspend)) // do NOT attempt to call suspend(true); from within the eventchain!
                QMetaObject::invokeMethod(this, "suspend", Qt::QueuedConnection, Q_ARG(Compositor::SuspendReason, BlockRuleSuspend));
        }
    }
    else if (m_suspended & BlockRuleSuspend) {  // lost a client and we're blocked - can we resume?
        bool resume = true;
        for (ClientList::ConstIterator it = Workspace::self()->clientList().constBegin(); it != Workspace::self()->clientList().constEnd(); ++it) {
            if ((*it)->isBlockingCompositing()) {
                resume = false;
                break;
            }
        }
        if (resume) { // do NOT attempt to call suspend(false); from within the eventchain!
            QMetaObject::invokeMethod(this, "resume", Qt::QueuedConnection, Q_ARG(Compositor::SuspendReason, BlockRuleSuspend));
        }
    }
}

void Compositor::suspend(Compositor::SuspendReason reason)
{
    if (kwinApp()->platform()->requiresCompositing()) {
        return;
    }
    Q_ASSERT(reason != NoReasonSuspend);
    m_suspended |= reason;
    if (reason & KWin::Compositor::ScriptSuspend) {
        // when disabled show a shortcut how the user can get back compositing
        const auto shortcuts = KGlobalAccel::self()->shortcut(workspace()->findChild<QAction*>(QStringLiteral("Suspend Compositing")));
        if (!shortcuts.isEmpty()) {
            // display notification only if there is the shortcut
            const QString message = i18n("Desktop effects have been suspended by another application.<br/>"
                                         "You can resume using the '%1' shortcut.", shortcuts.first().toString(QKeySequence::NativeText));
            KNotification::event(QStringLiteral("compositingsuspendeddbus"), message);
        }
    }
    finish();
}

void Compositor::resume(Compositor::SuspendReason reason)
{
    Q_ASSERT(reason != NoReasonSuspend);
    m_suspended &= ~reason;
    setup(); // signal "toggled" is eventually emitted from within setup
}

void Compositor::restart()
{
    if (hasScene()) {
        finish();
        QTimer::singleShot(0, this, &Compositor::setup);
    }
}

void Compositor::addRepaint(int x, int y, int w, int h)
{
    if (!hasScene())
        return;
    repaints_region += QRegion(x, y, w, h);
    scheduleRepaint();
}

void Compositor::addRepaint(const QRect& r)
{
    if (!hasScene())
        return;
    repaints_region += r;
    scheduleRepaint();
}

void Compositor::addRepaint(const QRegion& r)
{
    if (!hasScene())
        return;
    repaints_region += r;
    scheduleRepaint();
}

void Compositor::addRepaintFull()
{
    if (!hasScene())
        return;
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
    assert(!m_bufferSwapPending);

    m_bufferSwapPending = true;
}

void Compositor::bufferSwapComplete()
{
    assert(m_bufferSwapPending);
    m_bufferSwapPending = false;

    emit bufferSwapCompleted();

    if (m_composeAtSwapCompletion) {
        m_composeAtSwapCompletion = false;
        performCompositing();
    }
}

void Compositor::performCompositing()
{
    if (m_scene->usesOverlayWindow() && !isOverlayWindowVisible())
        return; // nothing is visible anyway

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
    ToplevelList windows = Workspace::self()->xStackingOrder();
    ToplevelList damaged;

    // Reset the damage state of each window and fetch the damage region
    // without waiting for a reply
    foreach (Toplevel *win, windows) {
        if (win->resetAndFetchDamage())
            damaged << win;
    }

    if (damaged.count() > 0) {
        m_scene->triggerFence();
        if (auto c = kwinApp()->x11Connection()) {
            xcb_flush(c);
        }
    }

    // Move elevated windows to the top of the stacking order
    foreach (EffectWindow *c, static_cast<EffectsHandlerImpl *>(effects)->elevatedWindows()) {
        Toplevel* t = static_cast< EffectWindowImpl* >(c)->window();
        windows.removeAll(t);
        windows.append(t);
    }

    // Get the replies
    foreach (Toplevel *win, damaged) {
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

    // skip windows that are not yet ready for being painted and if screen is locked skip windows that are
    // neither lockscreen nor inputmethod windows
    // TODO ?
    // this cannot be used so carelessly - needs protections against broken clients, the window
    // should not get focus before it's displayed, handle unredirected windows properly and so on.
    foreach (Toplevel *t, windows) {
        if (!t->readyForPainting()) {
            windows.removeAll(t);
        }
        if (waylandServer() && waylandServer()->isScreenLocked()) {
            if(!t->isLockScreen() && !t->isInputMethod()) {
                windows.removeAll(t);
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
            kwinApp()->platform()->createOpenGLSafePoint(Platform::OpenGLSafePoint::PostLastGuardedFrame);
        }
    }

    if (waylandServer()) {
        const auto currentTime = static_cast<quint32>(m_monotonicClock.elapsed());
        for (Toplevel *win : qAsConst(windows)) {
            if (auto surface = win->surface()) {
                surface->frameRendered(currentTime);
            }
        }
    }

    compositeTimer.stop(); // stop here to ensure *we* cause the next repaint schedule - not some effect through m_scene->paint()

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
    return std::any_of(windows.begin(), windows.end(), [] (T *t) { return !t->repaints().isEmpty(); });
}

bool Compositor::windowRepaintsPending() const
{
    if (repaintsPending(Workspace::self()->clientList())) {
        return true;
    }
    if (repaintsPending(Workspace::self()->desktopList())) {
        return true;
    }
    if (repaintsPending(Workspace::self()->unmanagedList())) {
        return true;
    }
    if (repaintsPending(Workspace::self()->deletedList())) {
        return true;
    }
    if (auto w = waylandServer()) {
        const auto &clients = w->clients();
        auto test = [] (ShellClient *c) {
            return c->readyForPainting() && !c->repaints().isEmpty();
        };
        if (std::any_of(clients.begin(), clients.end(), test)) {
            return true;
        }
        const auto &internalClients = w->internalClients();
        auto internalTest = [] (ShellClient *c) {
            return c->isShown(true) && !c->repaints().isEmpty();
        };
        if (std::any_of(internalClients.begin(), internalClients.end(), internalTest)) {
            return true;
        }
    }
    return false;
}

void Compositor::setCompositeResetTimer(int msecs)
{
    compositeResetTimer.start(msecs);
}

void Compositor::setCompositeTimer()
{
    if (!hasScene())  // should not really happen, but there may be e.g. some damage events still pending
        return;
    if (m_starting || !Workspace::self()) {
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
            // we're at low repaints or spent more time in painting than the user wanted to wait for that frame
            padding = vBlankInterval - (padding%vBlankInterval); // -> align to next vblank
        } else {  // -> align to the next maxFps tick
            padding = ((vBlankInterval - padding%vBlankInterval) + (fpsInterval/vBlankInterval-1)*vBlankInterval);
            //               "remaining time of the first vsync" + "time for the other vsyncs of the frame"
        }

        if (padding < options->vBlankTime()) { // we'll likely miss this frame
            waitTime = nanoToMilli(padding + vBlankInterval - options->vBlankTime()); // so we add one
        } else {
            waitTime = nanoToMilli(padding - options->vBlankTime());
        }
    }
    else { // w/o blocking vsync we just jump to the next demanded tick
        if (fpsInterval > m_timeSinceLastVBlank) {
            waitTime = nanoToMilli(fpsInterval - m_timeSinceLastVBlank);
            if (!waitTime) {
                waitTime = 1; // will ensure we don't block out the eventloop - the system's just not faster ...
            }
        }/* else if (m_scene->syncsToVBlank() && m_timeSinceLastVBlank - fpsInterval < (vBlankInterval<<1)) {
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
        }*/ else {
            waitTime = 1; // ... "0" would be sufficient, but the compositor isn't the WMs only task
        }
    }
    compositeTimer.start(qMin(waitTime, 250u), this); // force 4fps minimum
}

bool Compositor::isActive()
{
    return !m_finishing && hasScene();
}

bool Compositor::checkForOverlayWindow(WId w) const
{
    if (!hasScene()) {
        // no scene, so it cannot be the overlay window
        return false;
    }
    if (!m_scene->overlayWindow()) {
        // no overlay window, it cannot be the overlay
        return false;
    }
    // and compare the window ID's
    return w == m_scene->overlayWindow()->window();
}

bool Compositor::isOverlayWindowVisible() const
{
    if (!hasScene()) {
        return false;
    }
    if (!m_scene->overlayWindow()) {
        return false;
    }
    return m_scene->overlayWindow()->isVisible();
}

} // namespace

// included for CompositorSelectionOwner
#include "composite.moc"
