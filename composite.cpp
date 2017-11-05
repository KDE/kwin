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
#include "utils.h"
#include <QTextStream>
#include "workspace.h"
#include "client.h"
#include "unmanaged.h"
#include "deleted.h"
#include "effects.h"
#include "overlaywindow.h"
#include "scene.h"
#include "screens.h"
#include "shadow.h"
#include "useractions.h"
#include "xcbutils.h"
#include "platform.h"
#include "shell_client.h"
#include "wayland_server.h"
#include "decorations/decoratedclient.h"

#include <kwingltexture.h>

#include <KWayland/Server/surface_interface.h>

#include <stdio.h>

#include <QtConcurrentRun>
#include <QFutureWatcher>
#include <QMenu>
#include <QTimerEvent>
#include <QDateTime>
#include <QOpenGLContext>
#include <QQuickWindow>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginLoader>
#include <KPluginMetaData>
#include <KNotification>
#include <KSelectionWatcher>

#include <xcb/composite.h>
#include <xcb/damage.h>

Q_DECLARE_METATYPE(KWin::Compositor::SuspendReason)

namespace KWin
{

extern int currentRefreshRate();

CompositorSelectionOwner::CompositorSelectionOwner(const char *selection) : KSelectionOwner(selection, connection(), rootWindow()), owning(false)
{
    connect (this, SIGNAL(lostOwnership()), SLOT(looseOwnership()));
}

void CompositorSelectionOwner::looseOwnership()
{
    owning = false;
}

KWIN_SINGLETON_FACTORY_VARIABLE(Compositor, s_compositor)

static inline qint64 milliToNano(int milli) { return qint64(milli) * 1000 * 1000; }
static inline qint64 nanoToMilli(int nano) { return nano / (1000*1000); }

Compositor::Compositor(QObject* workspace)
    : QObject(workspace)
    , m_suspended(options->isUseCompositing() ? NoReasonSuspend : UserSuspend)
    , cm_selection(NULL)
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
    connect(&compositeResetTimer, SIGNAL(timeout()), SLOT(restart()));
    connect(options, &Options::configChanged, this, &Compositor::slotConfigChanged);
    compositeResetTimer.setSingleShot(true);
    nextPaintReference.invalidate(); // Initialize the timer

    // 2 sec which should be enough to restart the compositor
    static const int compositorLostMessageDelay = 2000;

    m_releaseSelectionTimer.setSingleShot(true);
    m_releaseSelectionTimer.setInterval(compositorLostMessageDelay);
    connect(&m_releaseSelectionTimer, SIGNAL(timeout()), SLOT(releaseCompositorSelection()));

    m_unusedSupportPropertyTimer.setInterval(compositorLostMessageDelay);
    m_unusedSupportPropertyTimer.setSingleShot(true);
    connect(&m_unusedSupportPropertyTimer, SIGNAL(timeout()), SLOT(deleteUnusedSupportProperties()));

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
            delete cm_selection;
            cm_selection = nullptr;
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
    delete cm_selection;
    s_compositor = NULL;
}


void Compositor::setup()
{
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

    if (!options->isCompositingInitialized()) {
        options->reloadCompositingSettings(true);
        slotCompositingOptionsInitialized();
    } else {
        slotCompositingOptionsInitialized();
    }
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
        if (cm_selection) {
            cm_selection->owning = false;
            cm_selection->release();
        }
        if (!supportedCompositors.contains(NoCompositing)) {
            qCCritical(KWIN_CORE) << "The used windowing system requires compositing";
            qCCritical(KWIN_CORE) << "We are going to quit KWin now as it is broken";
            qApp->quit();
        }
        return;
    }

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
    if (!cm_selection) {
        char selection_name[ 100 ];
        sprintf(selection_name, "_NET_WM_CM_S%d", Application::x11ScreenNumber());
        cm_selection = new CompositorSelectionOwner(selection_name);
        connect(cm_selection, SIGNAL(lostOwnership()), SLOT(finish()));
    }

    if (!cm_selection) // no X11 yet
        return;

    if (!cm_selection->owning) {
        cm_selection->claim(true);   // force claiming
        cm_selection->owning = true;
    }
}

void Compositor::setupX11Support()
{
    auto c = kwinApp()->x11Connection();
    if (!c) {
        delete cm_selection;
        cm_selection = nullptr;
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
    connect(Workspace::self(), &Workspace::deletedRemoved, m_scene, &Scene::windowDeleted);
    connect(effects, SIGNAL(screenGeometryChanged(QSize)), SLOT(addRepaintFull()));
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
    if (Workspace::self()) {
        foreach (Client * c, Workspace::self()->clientList())
            m_scene->windowClosed(c, NULL);
        foreach (Client * c, Workspace::self()->desktopList())
            m_scene->windowClosed(c, NULL);
        foreach (Unmanaged * c, Workspace::self()->unmanagedList())
            m_scene->windowClosed(c, NULL);
        foreach (Deleted * c, Workspace::self()->deletedList())
            m_scene->windowDeleted(c);
        foreach (Client * c, Workspace::self()->clientList())
        c->finishCompositing();
        foreach (Client * c, Workspace::self()->desktopList())
        c->finishCompositing();
        foreach (Unmanaged * c, Workspace::self()->unmanagedList())
        c->finishCompositing();
        foreach (Deleted * c, Workspace::self()->deletedList())
        c->finishCompositing();
        if (auto c = kwinApp()->x11Connection()) {
            xcb_composite_unredirect_subwindows(c, kwinApp()->x11RootWindow(), XCB_COMPOSITE_REDIRECT_MANUAL);
        }
    }
    if (waylandServer()) {
        foreach (ShellClient *c, waylandServer()->clients()) {
            m_scene->windowClosed(c, nullptr);
        }
        foreach (ShellClient *c, waylandServer()->internalClients()) {
            m_scene->windowClosed(c, nullptr);
        }
        foreach (ShellClient *c, waylandServer()->clients()) {
            c->finishCompositing();
        }
        foreach (ShellClient *c, waylandServer()->internalClients()) {
            c->finishCompositing();
        }
    }
    delete effects;
    effects = NULL;
    delete m_scene;
    m_scene = NULL;
    compositeTimer.stop();
    repaints_region = QRegion();
    if (Workspace::self()) {
        for (ClientList::ConstIterator it = Workspace::self()->clientList().constBegin();
                it != Workspace::self()->clientList().constEnd();
                ++it) {
            // forward all opacity values to the frame in case there'll be other CM running
            if ((*it)->opacity() != 1.0) {
                NETWinInfo i(connection(), (*it)->frameId(), rootWindow(), 0, 0);
                i.setOpacity(static_cast< unsigned long >((*it)->opacity() * 0xffffffff));
            }
        }
        // discard all Deleted windows (#152914)
        while (!Workspace::self()->deletedList().isEmpty())
            Workspace::self()->deletedList().first()->discard();
    }
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
    if (cm_selection) {
        cm_selection->owning = false;
        cm_selection->release();
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
    options->setCompositingInitialized(false);
    setup();

    if (effects) { // setup() may fail
        effects->reconfigure();
    }
}

// for the shortcut
void Compositor::slotToggleCompositing()
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
    updateCompositeBlocking(NULL);
}

void Compositor::updateCompositeBlocking(Client *c)
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
        QTimer::singleShot(0, this, SLOT(setup()));
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
        m_timeSinceStart += m_timeSinceLastVBlank;
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
    m_timeSinceStart += m_timeSinceLastVBlank;

    if (waylandServer()) {
        for (Toplevel *win : damaged) {
            if (auto surface = win->surface()) {
                surface->frameRendered(m_timeSinceStart);
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

/*****************************************************
 * Workspace
 ****************************************************/

bool Workspace::compositing() const
{
    return m_compositor && m_compositor->hasScene();
}

//****************************************
// Toplevel
//****************************************

bool Toplevel::setupCompositing()
{
    if (!compositing())
        return false;

    if (damage_handle != XCB_NONE)
        return false;

    if (kwinApp()->operationMode() == Application::OperationModeX11 && !surface()) {
        damage_handle = xcb_generate_id(connection());
        xcb_damage_create(connection(), damage_handle, frameId(), XCB_DAMAGE_REPORT_LEVEL_NON_EMPTY);
    }

    damage_region = QRegion(0, 0, width(), height());
    effect_window = new EffectWindowImpl(this);

    Compositor::self()->scene()->windowAdded(this);

    // With unmanaged windows there is a race condition between the client painting the window
    // and us setting up damage tracking.  If the client wins we won't get a damage event even
    // though the window has been painted.  To avoid this we mark the whole window as damaged
    // and schedule a repaint immediately after creating the damage object.
    if (dynamic_cast<Unmanaged*>(this))
        addDamageFull();

    return true;
}

void Toplevel::finishCompositing(ReleaseReason releaseReason)
{
    if (kwinApp()->operationMode() == Application::OperationModeX11 && damage_handle == XCB_NONE)
        return;
    if (effect_window->window() == this) { // otherwise it's already passed to Deleted, don't free data
        discardWindowPixmap();
        delete effect_window;
    }

    if (damage_handle != XCB_NONE &&
            releaseReason != ReleaseReason::Destroyed) {
        xcb_damage_destroy(connection(), damage_handle);
    }

    damage_handle = XCB_NONE;
    damage_region = QRegion();
    repaints_region = QRegion();
    effect_window = NULL;
}

void Toplevel::discardWindowPixmap()
{
    addDamageFull();
    if (effectWindow() != NULL && effectWindow()->sceneWindow() != NULL)
        effectWindow()->sceneWindow()->pixmapDiscarded();
}

void Toplevel::damageNotifyEvent()
{
    m_isDamaged = true;

    // Note: The rect is supposed to specify the damage extents,
    //       but we don't know it at this point. No one who connects
    //       to this signal uses the rect however.
    emit damaged(this, QRect());
}

bool Toplevel::compositing() const
{
    if (!Workspace::self()) {
        return false;
    }
    return Workspace::self()->compositing();
}

void Client::damageNotifyEvent()
{
    if (syncRequest.isPending && isResize()) {
        emit damaged(this, QRect());
        m_isDamaged = true;
        return;
    }

    if (!ready_for_painting) { // avoid "setReadyForPainting()" function calling overhead
        if (syncRequest.counter == XCB_NONE) {  // cannot detect complete redraw, consider done now
            setReadyForPainting();
            setupWindowManagementInterface();
        }
    }

    Toplevel::damageNotifyEvent();
}

bool Toplevel::resetAndFetchDamage()
{
    if (!m_isDamaged)
        return false;

    if (damage_handle == XCB_NONE) {
        m_isDamaged = false;
        return true;
    }

    xcb_connection_t *conn = connection();

    // Create a new region and copy the damage region to it,
    // resetting the damaged state.
    xcb_xfixes_region_t region = xcb_generate_id(conn);
    xcb_xfixes_create_region(conn, region, 0, 0);
    xcb_damage_subtract(conn, damage_handle, 0, region);

    // Send a fetch-region request and destroy the region
    m_regionCookie = xcb_xfixes_fetch_region_unchecked(conn, region);
    xcb_xfixes_destroy_region(conn, region);

    m_isDamaged = false;
    m_damageReplyPending = true;

    return m_damageReplyPending;
}

void Toplevel::getDamageRegionReply()
{
    if (!m_damageReplyPending)
        return;

    m_damageReplyPending = false;

    // Get the fetch-region reply
    xcb_xfixes_fetch_region_reply_t *reply =
            xcb_xfixes_fetch_region_reply(connection(), m_regionCookie, 0);

    if (!reply)
        return;

    // Convert the reply to a QRegion
    int count = xcb_xfixes_fetch_region_rectangles_length(reply);
    QRegion region;

    if (count > 1 && count < 16) {
        xcb_rectangle_t *rects = xcb_xfixes_fetch_region_rectangles(reply);

        QVector<QRect> qrects;
        qrects.reserve(count);

        for (int i = 0; i < count; i++)
            qrects << QRect(rects[i].x, rects[i].y, rects[i].width, rects[i].height);

        region.setRects(qrects.constData(), count);
    } else
        region += QRect(reply->extents.x, reply->extents.y,
                        reply->extents.width, reply->extents.height);

    damage_region += region;
    repaints_region += region;

    free(reply);
}

void Toplevel::addDamageFull()
{
    if (!compositing())
        return;

    damage_region = rect();
    repaints_region |= rect();

    emit damaged(this, rect());
}

void Toplevel::resetDamage()
{
    damage_region = QRegion();
}

void Toplevel::addRepaint(const QRect& r)
{
    if (!compositing()) {
        return;
    }
    repaints_region += r;
    emit needsRepaint();
}

void Toplevel::addRepaint(int x, int y, int w, int h)
{
    QRect r(x, y, w, h);
    addRepaint(r);
}

void Toplevel::addRepaint(const QRegion& r)
{
    if (!compositing()) {
        return;
    }
    repaints_region += r;
    emit needsRepaint();
}

void Toplevel::addLayerRepaint(const QRect& r)
{
    if (!compositing()) {
        return;
    }
    layer_repaints_region += r;
    emit needsRepaint();
}

void Toplevel::addLayerRepaint(int x, int y, int w, int h)
{
    QRect r(x, y, w, h);
    addLayerRepaint(r);
}

void Toplevel::addLayerRepaint(const QRegion& r)
{
    if (!compositing())
        return;
    layer_repaints_region += r;
    emit needsRepaint();
}

void Toplevel::addRepaintFull()
{
    repaints_region = visibleRect().translated(-pos());
    emit needsRepaint();
}

void Toplevel::resetRepaints()
{
    repaints_region = QRegion();
    layer_repaints_region = QRegion();
}

void Toplevel::addWorkspaceRepaint(int x, int y, int w, int h)
{
    addWorkspaceRepaint(QRect(x, y, w, h));
}

void Toplevel::addWorkspaceRepaint(const QRect& r2)
{
    if (!compositing())
        return;
    Compositor::self()->addRepaint(r2);
}

//****************************************
// Client
//****************************************

bool Client::setupCompositing()
{
    if (!Toplevel::setupCompositing()){
        return false;
    }
    if (isDecorated()) {
        decoratedClient()->destroyRenderer();
    }
    updateVisibility(); // for internalKeep()
    return true;
}

void Client::finishCompositing(ReleaseReason releaseReason)
{
    Toplevel::finishCompositing(releaseReason);
    updateVisibility();
    if (!deleting) {
        if (isDecorated()) {
            decoratedClient()->destroyRenderer();
        }
    }
    // for safety in case KWin is just resizing the window
    resetHaveResizeEffect();
}

} // namespace
