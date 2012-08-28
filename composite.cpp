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

/*
 Code related to compositing (redirecting windows to pixmaps and tracking
 window damage).

 Docs:

 XComposite (the protocol, but the function calls map to it):
 http://gitweb.freedesktop.org/?p=xorg/proto/compositeproto.git;a=blob_plain;hb=HEAD;f=compositeproto.txt

 XDamage (again the protocol):
 http://gitweb.freedesktop.org/?p=xorg/proto/damageproto.git;a=blob_plain;hb=HEAD;f=damageproto.txt

 Paper including basics on compositing, XGL vs AIGLX, XRender vs OpenGL, etc.:
 http://www.vis.uni-stuttgart.de/~hopf/pub/LinuxTag2007_compiz_NextGenerationDesktop_Paper.pdf

 Composite HOWTO from Fredrik:
 http://ktown.kde.org/~fredrik/composite_howto.html

*/

#include <config-X11.h>

#include "utils.h"
#include <QTextStream>
#include "workspace.h"
#include "client.h"
#include "unmanaged.h"
#include "deleted.h"
#include "effects.h"
#include "overlaywindow.h"
#include "scene.h"
#include "scene_xrender.h"
#include "scene_opengl.h"
#include "shadow.h"
#include "useractions.h"
#include "compositingprefs.h"
#include "composite.h"
#include "notifications.h"

#include <stdio.h>

#include <QtCore/QtConcurrentRun>
#include <QtCore/QFutureWatcher>
#include <QMenu>
#include <QTimerEvent>
#include <QDateTime>
#include <kaction.h>
#include <kactioncollection.h>
#include <klocale.h>
#include <kxerrorhandler.h>

#include <X11/extensions/shape.h>

#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrandr.h>

namespace KWin
{

Compositor *Compositor::s_compositor = NULL;
extern int currentRefreshRate();

//****************************************
// Workspace
//****************************************

Compositor *Compositor::createCompositor(QObject *parent)
{
    Q_ASSERT(!s_compositor);
    s_compositor = new Compositor(parent);
    return s_compositor;
}

Compositor::Compositor(QObject* workspace)
    : QObject(workspace)
    , m_suspended(!options->isUseCompositing())
    , m_blocked(false)
    , cm_selection(NULL)
    , vBlankInterval(0)
    , fpsInterval(0)
    , m_xrrRefreshRate(0)
    , forceUnredirectCheck(false)
    , m_finishing(false)
    , m_timeSinceLastVBlank(0)
    , m_nextFrameDelay(0)
    , m_scene(NULL)
{
    connect(&unredirectTimer, SIGNAL(timeout()), SLOT(delayedCheckUnredirect()));
    connect(&compositeResetTimer, SIGNAL(timeout()), SLOT(restart()));
    connect(workspace, SIGNAL(configChanged()), SLOT(slotConfigChanged()));
    connect(&mousePollingTimer, SIGNAL(timeout()), SLOT(performMousePoll()));
    unredirectTimer.setSingleShot(true);
    compositeResetTimer.setSingleShot(true);
    nextPaintReference.invalidate(); // Initialize the timer
    // delay the call to setup by one event cycle
    // The ctor of this class is invoked from the Workspace ctor, that means before
    // Workspace is completely constructed, so calling Workspace::self() would result
    // in undefined behavior. This is fixed by using a delayed invocation.
    QMetaObject::invokeMethod(this, "setup", Qt::QueuedConnection);
}

Compositor::~Compositor()
{
    finish();
}


void Compositor::setup()
{
    if (hasScene())
        return;
    if (m_suspended) {
        kDebug(1212) << "Compositing is suspended";
        return;
    } else if (!CompositingPrefs::compositingPossible()) {
        kError(1212) << "Compositing is not possible";
        return;
    }

    if (!options->isCompositingInitialized()) {
#ifndef KWIN_HAVE_OPENGLES
        // options->reloadCompositingSettings(true) initializes the CompositingPrefs which calls an
        // external program in turn
        // run this in an external thread to make startup faster.
        QFutureWatcher<void> *compositingPrefsFuture = new QFutureWatcher<void>();
        connect(compositingPrefsFuture, SIGNAL(finished()), this, SLOT(slotCompositingOptionsInitialized()));
        connect(compositingPrefsFuture, SIGNAL(finished()), compositingPrefsFuture, SLOT(deleteLater()));
        compositingPrefsFuture->setFuture(QtConcurrent::run(options, &Options::reloadCompositingSettings, true));
#else
        // OpenGL ES does not call the external program, so no need to create a thread
        options->reloadCompositingSettings(true);
        slotCompositingOptionsInitialized();
#endif
    } else {
        slotCompositingOptionsInitialized();
    }
}

void Compositor::slotCompositingOptionsInitialized()
{
    char selection_name[ 100 ];
    sprintf(selection_name, "_NET_WM_CM_S%d", DefaultScreen(display()));
    cm_selection = new KSelectionOwner(selection_name);
    connect(cm_selection, SIGNAL(lostOwnership()), SLOT(finish()));
    cm_selection->claim(true);   // force claiming

    switch(options->compositingMode()) {
    case OpenGLCompositing: {
        kDebug(1212) << "Initializing OpenGL compositing";

        // Some broken drivers crash on glXQuery() so to prevent constant KWin crashes:
        KSharedConfigPtr unsafeConfigPtr = KGlobal::config();
        KConfigGroup unsafeConfig(unsafeConfigPtr, "Compositing");
        if (unsafeConfig.readEntry("OpenGLIsUnsafe", false))
            kWarning(1212) << "KWin has detected that your OpenGL library is unsafe to use";
        else {
            unsafeConfig.writeEntry("OpenGLIsUnsafe", true);
            unsafeConfig.sync();
#ifndef KWIN_HAVE_OPENGLES
            if (!CompositingPrefs::hasGlx()) {
                unsafeConfig.writeEntry("OpenGLIsUnsafe", false);
                unsafeConfig.sync();
                kDebug(1212) << "No glx extensions available";
                break;
            }
#endif

            m_scene = new SceneOpenGL(Workspace::self());

            // TODO: Add 30 second delay to protect against screen freezes as well
            unsafeConfig.writeEntry("OpenGLIsUnsafe", false);
            unsafeConfig.sync();

            if (!m_scene->initFailed())
                break; // -->
            delete m_scene;
            m_scene = NULL;
        }

        // Do not Fall back to XRender - it causes problems when selfcheck fails during startup, but works later on
        break;
    }
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    case XRenderCompositing:
        kDebug(1212) << "Initializing XRender compositing";
        m_scene = new SceneXrender(Workspace::self());
        break;
#endif
    default:
        kDebug(1212) << "No compositing enabled";
        delete cm_selection;
        return;
    }
    if (m_scene == NULL || m_scene->initFailed()) {
        kError(1212) << "Failed to initialize compositing, compositing disabled";
        kError(1212) << "Consult http://techbase.kde.org/Projects/KWin/4.0-release-notes#Setting_up";
        delete m_scene;
        m_scene = NULL;
        delete cm_selection;
        return;
    }
    m_xrrRefreshRate = KWin::currentRefreshRate();
    fpsInterval = (options->maxFpsInterval() << 10);
    if (m_scene->waitSyncAvailable()) {  // if we do vsync, set the fps to the next multiple of the vblank rate
        vBlankInterval = (1000 << 10) / m_xrrRefreshRate;
        fpsInterval = qMax((fpsInterval / vBlankInterval) * vBlankInterval, vBlankInterval);
    } else
        vBlankInterval = 1 << 10; // no sync - DO NOT set "0", would cause div-by-zero segfaults.
    m_timeSinceLastVBlank = fpsInterval - 1; // means "start now" - we don't have even a slight idea when the first vsync will occur
    scheduleRepaint();
    XCompositeRedirectSubwindows(display(), rootWindow(), CompositeRedirectManual);
    new EffectsHandlerImpl(this, m_scene);   // sets also the 'effects' pointer
    connect(effects, SIGNAL(screenGeometryChanged(QSize)), SLOT(addRepaintFull()));
    addRepaintFull();
    foreach (Client * c, Workspace::self()->clientList())
        c->setupCompositing();
    foreach (Client * c,  Workspace::self()->desktopList())
        c->setupCompositing();
    foreach (Unmanaged * c, Workspace::self()->unmanagedList())
        c->setupCompositing();

    // render at least once
    compositeTimer.stop();
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
    delete cm_selection;
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
    XCompositeUnredirectSubwindows(display(), rootWindow(), CompositeRedirectManual);
    delete effects;
    effects = NULL;
    delete m_scene;
    m_scene = NULL;
    compositeTimer.stop();
    mousePollingTimer.stop();
    repaints_region = QRegion();
    for (ClientList::ConstIterator it = Workspace::self()->clientList().constBegin();
            it != Workspace::self()->clientList().constEnd();
            ++it) {
        // forward all opacity values to the frame in case there'll be other CM running
        if ((*it)->opacity() != 1.0) {
            NETWinInfo2 i(display(), (*it)->frameId(), rootWindow(), 0);
            i.setOpacity(static_cast< unsigned long >((*it)->opacity() * 0xffffffff));
        }
    }
    // discard all Deleted windows (#152914)
    while (!Workspace::self()->deletedList().isEmpty())
        Workspace::self()->deletedList().first()->discard(Allowed);
    m_finishing = false;
}

// OpenGL self-check failed, fallback to XRender
void Compositor::fallbackToXRenderCompositing()
{
    finish();
    KConfigGroup config(KGlobal::config(), "Compositing");
    config.writeEntry("Backend", "XRender");
    config.writeEntry("GraphicsSystem", "native");
    config.sync();
    if (Extensions::nonNativePixmaps()) { // must restart to change the graphicssystem
        restartKWin("automatic graphicssystem change for XRender backend");
        return;
    } else {
        options->setCompositingMode(XRenderCompositing);
        setup();
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
    KGlobal::config()->reparseConfiguration();
    const QString graphicsSystem = KConfigGroup(KGlobal::config(), "Compositing").readEntry("GraphicsSystem", "");
    if ((Extensions::nonNativePixmaps() && graphicsSystem == "native") ||
        (!Extensions::nonNativePixmaps() && (graphicsSystem == "raster" || graphicsSystem == "opengl")) ) {
        restartKWin("explicitly reconfigured graphicsSystem change");
        return;
    }

    // Update any settings that can be set in the compositing kcm.
#ifdef KWIN_BUILD_SCREENEDGES
    Workspace::self()->screenEdge()->update();
#endif
    // Restart compositing
    finish();
    // resume compositing if suspended
    m_suspended = false;
    options->setCompositingInitialized(false);
    setup();

    if (effects) { // setup() may fail
        effects->reconfigure();
    }
    emit compositingToggled(!m_suspended);
}

// for the shortcut
void Compositor::slotToggleCompositing()
{
    suspendResume(!m_suspended);
}

// for the dbus call
void Compositor::toggleCompositing()
{
    slotToggleCompositing();
    if (m_suspended) {
        // when disabled show a shortcut how the user can get back compositing
        QString shortcut, message;
        if (KAction* action = qobject_cast<KAction*>(Workspace::self()->actionCollection()->action("Suspend Compositing")))
            shortcut = action->globalShortcut().primary().toString(QKeySequence::NativeText);
        if (!shortcut.isEmpty()) {
            // display notification only if there is the shortcut
            message = i18n("Desktop effects have been suspended by another application.<br/>"
                           "You can resume using the '%1' shortcut.", shortcut);
            Notify::raise(Notify::CompositingSuspendedDbus, message);
        }
    }
}

QStringList Workspace::activeEffects() const
{
    if (effects)
        return static_cast< EffectsHandlerImpl* >(effects)->activeEffects();
    return QStringList();
}

void Compositor::updateCompositeBlocking()
{
    updateCompositeBlocking(NULL);
}

void Compositor::updateCompositeBlocking(Client *c)
{
    if (c) { // if c == 0 we just check if we can resume
        if (c->isBlockingCompositing()) {
            if (!m_blocked) // do NOT attempt to call suspend(true); from within the eventchain!
                QMetaObject::invokeMethod(this, "slotToggleCompositing", Qt::QueuedConnection);
            m_blocked = true;
        }
    }
    else if (m_blocked) {  // lost a client and we're blocked - can we resume?
        // NOTICE do NOT check for "m_Suspended" or "!compositing()"
        // only "resume" if it was really disabled for a block
        bool resume = true;
        for (ClientList::ConstIterator it = Workspace::self()->clientList().constBegin(); it != Workspace::self()->clientList().constEnd(); ++it) {
            if ((*it)->isBlockingCompositing()) {
                resume = false;
                break;
            }
        }
        if (resume) { // do NOT attempt to call suspend(false); from within the eventchain!
            m_blocked = false;
            if (m_suspended)
                QMetaObject::invokeMethod(this, "slotToggleCompositing", Qt::QueuedConnection);
        }
    }
}

void Compositor::suspendResume(bool suspend)
{
    m_suspended = suspend;
    finish();
    setup(); // will do nothing if suspended
    emit compositingToggled(!m_suspended);
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
    repaints_region = QRegion(0, 0, displayWidth(), displayHeight());
    scheduleRepaint();
}

void Compositor::timerEvent(QTimerEvent *te)
{
    if (te->timerId() == compositeTimer.timerId()) {
        compositeTimer.stop();
        performCompositing();
    } else
        QObject::timerEvent(te);
}

static int s_pendingFlushes = 0;
void Compositor::performCompositing()
{
    if (!isOverlayWindowVisible())
        return; // nothing is visible anyway

    bool pending = !repaints_region.isEmpty() || windowRepaintsPending();
    if (pending)
        s_pendingFlushes = 3;
    else if (m_scene->hasPendingFlush())
        --s_pendingFlushes;
    else
        s_pendingFlushes = 0;
    if (s_pendingFlushes < 1) {
        s_pendingFlushes = 0;
        m_scene->idle();
        // Note: It would seem here we should undo suspended unredirect, but when scenes need
        // it for some reason, e.g. transformations or translucency, the next pass that does not
        // need this anymore and paints normally will also reset the suspended unredirect.
        // Otherwise the window would not be painted normally anyway.
        return;
    }

    // create a list of all windows in the stacking order
    ToplevelList windows = Workspace::self()->xStackingOrder();
    foreach (EffectWindow * c, static_cast< EffectsHandlerImpl* >(effects)->elevatedWindows()) {
        Toplevel* t = static_cast< EffectWindowImpl* >(c)->window();
        windows.removeAll(t);
        windows.append(t);
    }

    // skip windows that are not yet ready for being painted
    // TODO ?
    // this cannot be used so carelessly - needs protections against broken clients, the window
    // should not get focus before it's displayed, handle unredirected windows properly and so on.
    foreach (Toplevel *t, windows)
        if (!t->readyForPainting())
            windows.removeAll(t);

    QRegion repaints = repaints_region;
    // clear all repaints, so that post-pass can add repaints for the next repaint
    repaints_region = QRegion();

    m_timeSinceLastVBlank = m_scene->paint(repaints, windows);
    // Trigger at least one more pass even if there would be nothing to paint, so that scene->idle()
    // is called the next time. If there would be nothing pending, it will not restart the timer and
    // checkCompositeTime() would restart it again somewhen later, called from functions that
    // would again add something pending.
    scheduleRepaint();
}

void Compositor::performMousePoll()
{
    Workspace::self()->checkCursorPos();
}

bool Compositor::windowRepaintsPending() const
{
    foreach (Toplevel * c, Workspace::self()->clientList())
    if (!c->repaints().isEmpty())
        return true;
    foreach (Toplevel * c, Workspace::self()->desktopList())
    if (!c->repaints().isEmpty())
        return true;
    foreach (Toplevel * c, Workspace::self()->unmanagedList())
    if (!c->repaints().isEmpty())
        return true;
    foreach (Toplevel * c, Workspace::self()->deletedList())
    if (!c->repaints().isEmpty())
        return true;
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

    uint padding = m_timeSinceLastVBlank << 10;

    if (m_scene->waitSyncAvailable()) {

        // TODO: make vBlankTime dynamic?!
        // It's required because glXWaitVideoSync will *likely* block a full frame if one enters
        // a retrace pass which can last a variable amount of time, depending on the actual screen
        // Now, my ooold 19" CRT can do such retrace so that 2ms are entirely sufficient,
        // while another ooold 15" TFT requires about 6ms

        if (padding > fpsInterval) {
            // we're at low repaints or spent more time in painting than the user wanted to wait for that frame
            padding = vBlankInterval - (padding%vBlankInterval); // -> align to next vblank
        } else {  // -> align to the next maxFps tick
            padding = ((vBlankInterval - padding%vBlankInterval) + (fpsInterval/vBlankInterval-1)*vBlankInterval);
            //               "remaining time of the first vsync" + "time for the other vsyncs of the frame"
        }

        if (padding < options->vBlankTime()) { // we'll likely miss this frame
            m_nextFrameDelay = (padding + vBlankInterval) >> 10;
            padding = (padding + vBlankInterval - options->vBlankTime()) >> 10; // so we add one
//             qDebug() << "WE LOST A FRAME";
        } else {
            m_nextFrameDelay = padding >> 10;
            padding = (padding - options->vBlankTime()) >> 10;
        }
    }
    else // w/o vsync we just jump to the next demanded tick
        // the "1" will ensure we don't block out the eventloop - the system's just not faster
        // "0" would be sufficient, but the compositor isn't the WMs only task
        m_nextFrameDelay = padding = (padding > (int)fpsInterval) ? 1 : ((fpsInterval - padding) >> 10);
    compositeTimer.start(qMin(padding, 250u), this); // force 4fps minimum
}

void Compositor::startMousePolling()
{
    mousePollingTimer.start(20);   // 50Hz. TODO: How often do we really need to poll?
}

void Compositor::stopMousePolling()
{
    mousePollingTimer.stop();
}

bool Compositor::isActive()
{
    return !m_finishing && hasScene();
}

void Compositor::checkUnredirect()
{
    checkUnredirect(false);
}

// force is needed when the list of windows changes (e.g. a window goes away)
void Compositor::checkUnredirect(bool force)
{
    if (!hasScene() || m_scene->overlayWindow()->window() == None || !options->isUnredirectFullscreen())
        return;
    if (force)
        forceUnredirectCheck = true;
    if (!unredirectTimer.isActive())
        unredirectTimer.start(0);
}

void Compositor::delayedCheckUnredirect()
{
    if (!hasScene() || m_scene->overlayWindow()->window() == None || !options->isUnredirectFullscreen())
        return;
    ToplevelList list;
    bool changed = forceUnredirectCheck;
    foreach (Client * c, Workspace::self()->clientList())
    list.append(c);
    foreach (Unmanaged * c, Workspace::self()->unmanagedList())
    list.append(c);
    foreach (Toplevel * c, list) {
        if (c->updateUnredirectedState())
            changed = true;
    }
    // no desktops, no Deleted ones
    if (!changed)
        return;
    forceUnredirectCheck = false;
    // Cut out parts from the overlay window where unredirected windows are,
    // so that they are actually visible.
    QRegion reg(0, 0, displayWidth(), displayHeight());
    foreach (Toplevel * c, list) {
        if (c->unredirected())
            reg -= c->geometry();
    }
    m_scene->overlayWindow()->setShape(reg);
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

WId Compositor::overlayWindow() const
{
    if (!hasScene()) {
        return None;
    }
    if (!m_scene->overlayWindow()) {
        return None;
    }
    return m_scene->overlayWindow()->window();
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

void Compositor::setOverlayWindowVisibility(bool visible)
{
    if (hasScene() && m_scene->overlayWindow()) {
        m_scene->overlayWindow()->setVisibility(visible);
    }
}

void Compositor::restartKWin(const QString &reason)
{
    kDebug(1212) << "restarting kwin for:" << reason;
    char cmd[1024]; // copied from crashhandler - maybe not the best way to do?
    sprintf(cmd, "%s --replace &", QFile::encodeName(QCoreApplication::applicationFilePath()).constData());
    system(cmd);
}

/*****************************************************
 * Compositing related D-Bus interface from Workspace
 ****************************************************/
bool Workspace::compositingPossible() const
{
    return CompositingPrefs::compositingPossible();
}

QString Workspace::compositingNotPossibleReason() const
{
    return CompositingPrefs::compositingNotPossibleReason();
}

bool Workspace::openGLIsBroken() const
{
    return CompositingPrefs::openGlIsBroken();
}

QString Workspace::compositingType()
{
    // the returned strings are considered as identifiers and may not be translated
    if (!effects) {
        return "none";
    }
    if (effects->compositingType() == XRenderCompositing) {
        return "xrender";
    } else if (effects->compositingType() == OpenGLCompositing) {
#ifdef KWIN_HAVE_OPENGLES
        return "gles";
#else
        if (ShaderManager::instance()->isValid()) {
            return "gl2";
        } else {
            return "gl1";
        }
#endif
    }
    return "none";
}

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
    damageRatio = 0.0;
    if (damage_handle != None)
        return false;
    damage_handle = XDamageCreate(display(), frameId(), XDamageReportRawRectangles);
    damage_region = QRegion(0, 0, width(), height());
    effect_window = new EffectWindowImpl(this);
    unredirect = false;
    Compositor::self()->checkUnredirect(true);
    Compositor::self()->scene()->windowAdded(this);
    return true;
}

void Toplevel::finishCompositing()
{
    damageRatio = 0.0;
    if (damage_handle == None)
        return;
    Compositor::self()->checkUnredirect(true);
    if (effect_window->window() == this) { // otherwise it's already passed to Deleted, don't free data
        discardWindowPixmap();
        delete effect_window;
    }
    XDamageDestroy(display(), damage_handle);
    damage_handle = None;
    damage_region = QRegion();
    repaints_region = QRegion();
    effect_window = NULL;
}

void Toplevel::discardWindowPixmap()
{
    damageRatio = 0.0;
    addDamageFull();
    if (window_pix == None)
        return;
    XFreePixmap(display(), window_pix);
    window_pix = None;
    if (effectWindow() != NULL && effectWindow()->sceneWindow() != NULL)
        effectWindow()->sceneWindow()->pixmapDiscarded();
}

Pixmap Toplevel::createWindowPixmap()
{
    assert(compositing());
    if (unredirected())
        return None;
    damageRatio = 0.0;
    grabXServer();
    KXErrorHandler err;
    Pixmap pix = XCompositeNameWindowPixmap(display(), frameId());
    // check that the received pixmap is valid and actually matches what we
    // know about the window (i.e. size)
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(display(), frameId(), &attrs)
            || err.error(false)
            || attrs.width != width() || attrs.height != height() || attrs.map_state != IsViewable) {
        kDebug(1212) << "Creating window pixmap failed: " << this;
        XFreePixmap(display(), pix);
        pix = None;
    }
    ungrabXServer();
    return pix;
}

// We must specify that the two events are a union so the compiler doesn't
// complain about strict aliasing rules.
typedef union {
    XEvent e;
    XDamageNotifyEvent de;
} EventUnion;

static QVector<QRect> damageRects;

void Toplevel::damageNotifyEvent(XDamageNotifyEvent* e)
{
    if (damageRatio == 1.0) { // we know that we're completely damaged, no need to tell us again
        while (XPending(display())) { // drop events
            EventUnion e2;
            if (XPeekEvent(display(), &e2.e) && e2.e.type == Extensions::damageNotifyEvent() &&
                    e2.e.xany.window == frameId()) {
                XNextEvent(display(), &e2.e);
                continue;
            }
            break;
        }

        return;
    }

    const float area = rect().width()*rect().height();
    damageRects.reserve(16);
    damageRects.erase(damageRects.begin(), damageRects.end());
    damageRects << QRect(e->area.x, e->area.y, e->area.width, e->area.height);

    // we can not easily say anything about the overall ratio since the new rects may intersect the present
    float newDamageRatio = damageRects.last().width()*damageRects.last().height() / area;

    // compress
    while (XPending(display())) {
        EventUnion e2;
        if (XPeekEvent(display(), &e2.e) && e2.e.type == Extensions::damageNotifyEvent()
                && e2.e.xany.window == frameId()) {
            XNextEvent(display(), &e2.e);
            if (damageRatio >= 0.8 || newDamageRatio > 0.8 || damageRects.count() > 15) {
                // If there are too many damage events in the queue, just discard them
                // and damage the whole window. Otherwise the X server can just overload
                // us with a flood of damage events. Should be probably optimized
                // in the X server, as this is rather lame.
                newDamageRatio = 1.0;
                damageRects.clear();
                continue;
            }
            damageRects << QRect(e2.de.area.x, e2.de.area.y, e2.de.area.width, e2.de.area.height);
            newDamageRatio += damageRects.last().width()*damageRects.last().height() / area;
            continue;
        }
        break;
    }


    if ((damageRects.count() == 1 && damageRects.last() == rect()) ||
        (damageRects.isEmpty() && newDamageRatio == 1.0)) {
        addDamageFull();
    } else {
        foreach (const QRect &r, damageRects)
            addDamage(r);
    }
}

bool Toplevel::compositing() const
{
    Compositor *c = Compositor::self();
    return c && c->hasScene();
}

void Client::damageNotifyEvent(XDamageNotifyEvent* e)
{
#ifdef HAVE_XSYNC
    if (syncRequest.isPending && isResize())
        return;
    if (!ready_for_painting) { // avoid "setReadyForPainting()" function calling overhead
        if (syncRequest.counter == None)   // cannot detect complete redraw, consider done now
            setReadyForPainting();
    }
#else
    if (!ready_for_painting) {
        setReadyForPainting();
    }
#endif

    Toplevel::damageNotifyEvent(e);
}

void Unmanaged::damageNotifyEvent(XDamageNotifyEvent* e)
{
    if (!ready_for_painting) { // avoid "setReadyForPainting()" function calling overhead
        setReadyForPainting();
    }
    Toplevel::damageNotifyEvent(e);
}

void Toplevel::addDamage(const QRect& r)
{
    addDamage(r.x(), r.y(), r.width(), r.height());
}

void Toplevel::addDamage(int x, int y, int w, int h)
{
    if (!compositing())
        return;
    QRect r(x, y, w, h);
    // resizing the decoration may lag behind a bit and when shrinking there
    // may be a damage event coming with size larger than the current window size
    r &= rect();
    if (r.isEmpty())
        return;
    damage_region += r;
    int damageArea = 0;
    foreach (const QRect &r2, damage_region.rects())
        damageArea += r2.width()*r2.height();
    damageRatio = float(damageArea) / float(rect().width()*rect().height());
    repaints_region += r;
    emit damaged(this, r);
    // discard lanczos texture
    if (effect_window) {
        QVariant cachedTextureVariant = effect_window->data(LanczosCacheRole);
        if (cachedTextureVariant.isValid()) {
            GLTexture *cachedTexture = static_cast< GLTexture*>(cachedTextureVariant.value<void*>());
            delete cachedTexture;
            cachedTexture = 0;
            effect_window->setData(LanczosCacheRole, QVariant());
        }
    }
}

void Toplevel::addDamageFull()
{
    if (!compositing())
        return;
    damage_region = rect();
    repaints_region = rect();
    damageRatio = 1.0;
    emit damaged(this, rect());
    // discard lanczos texture
    if (effect_window) {
        QVariant cachedTextureVariant = effect_window->data(LanczosCacheRole);
        if (cachedTextureVariant.isValid()) {
            GLTexture *cachedTexture = static_cast< GLTexture*>(cachedTextureVariant.value<void*>());
            delete cachedTexture;
            cachedTexture = 0;
            effect_window->setData(LanczosCacheRole, QVariant());
        }
    }
}

void Toplevel::resetDamage(const QRect& r)
{
    damage_region -= r;
    int damageArea = 0;
    foreach (const QRect &r2, damage_region.rects())
        damageArea += r2.width()*r2.height();
    damageRatio = float(damageArea) / float(rect().width()*rect().height());
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

bool Toplevel::updateUnredirectedState()
{
    assert(compositing());
    bool should = shouldUnredirect() && !unredirectSuspend && !shape() && !hasAlpha() && opacity() == 1.0 &&
                  !static_cast<EffectsHandlerImpl*>(effects)->activeFullScreenEffect();
    if (should && !unredirect) {
        unredirect = true;
        kDebug(1212) << "Unredirecting:" << this;
        XCompositeUnredirectWindow(display(), frameId(), CompositeRedirectManual);
        return true;
    } else if (!should && unredirect) {
        unredirect = false;
        kDebug(1212) << "Redirecting:" << this;
        XCompositeRedirectWindow(display(), frameId(), CompositeRedirectManual);
        discardWindowPixmap();
        return true;
    }
    return false;
}

void Toplevel::suspendUnredirect(bool suspend)
{
    if (unredirectSuspend == suspend)
        return;
    unredirectSuspend = suspend;
    Compositor::self()->checkUnredirect();
}

//****************************************
// Client
//****************************************

bool Client::setupCompositing()
{
    if (!Toplevel::setupCompositing()){
        return false;
    }
    updateVisibility(); // for internalKeep()
    if (isManaged()) {
        // only create the decoration when a client is managed
        updateDecoration(true, true);
    }
    return true;
}

void Client::finishCompositing()
{
    Toplevel::finishCompositing();
    updateVisibility();
    if (!deleting) {
        // only recreate the decoration if we are not shutting down completely
        updateDecoration(true, true);
    }
    // for safety in case KWin is just resizing the window
    s_haveResizeEffect = false;
}

bool Client::shouldUnredirect() const
{
    if (isActiveFullScreen()) {
        ToplevelList stacking = workspace()->xStackingOrder();
        for (int pos = stacking.count() - 1;
                pos >= 0;
                --pos) {
            Toplevel* c = stacking.at(pos);
            if (c == this)   // is not covered by any other window, ok to unredirect
                return true;
            if (c->geometry().intersects(geometry()))
                return false;
        }
        abort();
    }
    return false;
}


//****************************************
// Unmanaged
//****************************************

bool Unmanaged::shouldUnredirect() const
{
    // the pixmap is needed for the login effect, a nicer solution would be the login effect increasing
    // refcount for the window pixmap (which would prevent unredirect), avoiding this hack
    if (resourceClass() == "ksplashx"
            || resourceClass() == "ksplashsimple"
            || resourceClass() == "ksplashqml"
            )
        return false;
// it must cover whole display or one xinerama screen, and be the topmost there
    if (geometry() == workspace()->clientArea(FullArea, geometry().center(), workspace()->currentDesktop())
            || geometry() == workspace()->clientArea(ScreenArea, geometry().center(), workspace()->currentDesktop())) {
        ToplevelList stacking = workspace()->xStackingOrder();
        for (int pos = stacking.count() - 1;
                pos >= 0;
                --pos) {
            Toplevel* c = stacking.at(pos);
            if (c == this)   // is not covered by any other window, ok to unredirect
                return true;
            if (c->geometry().intersects(geometry()))
                return false;
        }
        abort();
    }
    return false;
}

//****************************************
// Deleted
//****************************************

bool Deleted::shouldUnredirect() const
{
    return false;
}


} // namespace
