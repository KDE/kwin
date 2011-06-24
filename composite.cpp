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
#include "scene.h"
#include "scene_basic.h"
#include "scene_xrender.h"
#include "scene_opengl.h"
#include "shadow.h"
#include "compositingprefs.h"
#include "notifications.h"

#include <stdio.h>

#include <QMenu>
#include <QTimerEvent>
#include <kaction.h>
#include <kactioncollection.h>
#include <klocale.h>
#include <kxerrorhandler.h>

#include <X11/extensions/shape.h>

#ifdef HAVE_XCOMPOSITE
#include <X11/extensions/Xcomposite.h>
#if XCOMPOSITE_MAJOR > 0 || XCOMPOSITE_MINOR >= 3
#define HAVE_XCOMPOSITE_OVERLAY
#endif
#endif
#ifdef HAVE_XRANDR
#include <X11/extensions/Xrandr.h>
#endif

namespace KWin
{

extern int currentRefreshRate();

//****************************************
// Workspace
//****************************************

void Workspace::setupCompositing()
{
#ifdef KWIN_HAVE_COMPOSITING
    if (scene != NULL)
        return;
    if (compositingSuspended) {
        kDebug(1212) << "Compositing is suspended";
        return;
    } else if (!CompositingPrefs::compositingPossible()) {
        kError(1212) << "Compositing is not possible";
        return;
    }

    if (!options->compositingInitialized)
        options->reloadCompositingSettings(true);

    char selection_name[ 100 ];
    sprintf(selection_name, "_NET_WM_CM_S%d", DefaultScreen(display()));
    cm_selection = new KSelectionOwner(selection_name);
    connect(cm_selection, SIGNAL(lostOwnership()), SLOT(lostCMSelection()));
    cm_selection->claim(true);   // force claiming

    switch(options->compositingMode) {
        /*case 'B':
            kDebug( 1212 ) << "X compositing";
            scene = new SceneBasic( this );
          break; // don't fall through (this is a testing one) */
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    case OpenGLCompositing: {
        kDebug(1212) << "Initializing OpenGL compositing";

        // Some broken drivers crash on glXQuery() so to prevent constant KWin crashes:
        KSharedConfigPtr unsafeConfigPtr(KSharedConfig::openConfig("kwinrc"));
        KConfigGroup unsafeConfig(unsafeConfigPtr, "Compositing");
        if (unsafeConfig.readEntry("OpenGLIsUnsafe", false))
            kWarning(1212) << "KWin has detected that your OpenGL library is unsafe to use";
        else {
            unsafeConfig.writeEntry("OpenGLIsUnsafe", true);
            unsafeConfig.sync();

            scene = new SceneOpenGL(this);

            // TODO: Add 30 second delay to protect against screen freezes as well
            unsafeConfig.writeEntry("OpenGLIsUnsafe", false);
            unsafeConfig.sync();

            if (!scene->initFailed())
                break; // -->
            delete scene;
            scene = NULL;
        }

        // Do not Fall back to XRender - it causes problems when selfcheck fails during startup, but works later on
        break;
    }
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    case XRenderCompositing:
        kDebug(1212) << "Initializing XRender compositing";
        scene = new SceneXrender(this);
        break;
#endif
    default:
#ifndef KWIN_HAVE_COMPOSITING
        kDebug(1212) << "Compositing was not available at compile time";
#else
        kDebug(1212) << "No compositing enabled";
#endif
        delete cm_selection;
        return;
    }
    if (scene == NULL || scene->initFailed()) {
        kError(1212) << "Failed to initialize compositing, compositing disabled";
        kError(1212) << "Consult http://techbase.kde.org/Projects/KWin/4.0-release-notes#Setting_up";
        delete scene;
        scene = NULL;
        delete cm_selection;
        return;
    }
    xrrRefreshRate = KWin::currentRefreshRate();
    fpsInterval = (options->maxFpsInterval << 10);
    if (scene->waitSyncAvailable()) {  // if we do vsync, set the fps to the next multiple of the vblank rate
        vBlankInterval = (1000 << 10) / xrrRefreshRate;
        fpsInterval -= (fpsInterval % vBlankInterval);
        fpsInterval = qMax(fpsInterval, vBlankInterval);
    } else
        vBlankInterval = 1 << 10; // no sync - DO NOT set "0", would cause div-by-zero segfaults.
    vBlankPadding = 3; // vblank rounding errors... :-(
    nextPaintReference = QDateTime::currentMSecsSinceEpoch();
    checkCompositeTimer();
    XCompositeRedirectSubwindows(display(), rootWindow(), CompositeRedirectManual);
    new EffectsHandlerImpl(scene->compositingType());   // sets also the 'effects' pointer
    addRepaintFull();
    foreach (Client * c, clients)
    c->setupCompositing();
    foreach (Client * c, desktops)
    c->setupCompositing();
    foreach (Unmanaged * c, unmanaged)
    c->setupCompositing();
    foreach (Client * c, clients)
    scene->windowAdded(c);
    foreach (Client * c, desktops)
    scene->windowAdded(c);
    foreach (Unmanaged * c, unmanaged)
    scene->windowAdded(c);
    discardPopup(); // force re-creation of the Alt+F3 popup (opacity option)
#else
    kDebug(1212) << "Compositing was not available at compile time";
#endif
}

void Workspace::finishCompositing()
{
#ifdef KWIN_HAVE_COMPOSITING
    if (scene == NULL)
        return;
    m_finishingCompositing = true;
    delete cm_selection;
    foreach (Client * c, clients)
    scene->windowClosed(c, NULL);
    foreach (Client * c, desktops)
    scene->windowClosed(c, NULL);
    foreach (Unmanaged * c, unmanaged)
    scene->windowClosed(c, NULL);
    foreach (Deleted * c, deleted)
    scene->windowDeleted(c);
    foreach (Client * c, clients)
    c->finishCompositing();
    foreach (Client * c, desktops)
    c->finishCompositing();
    foreach (Unmanaged * c, unmanaged)
    c->finishCompositing();
    foreach (Deleted * c, deleted)
    c->finishCompositing();
    XCompositeUnredirectSubwindows(display(), rootWindow(), CompositeRedirectManual);
    delete effects;
    effects = NULL;
    delete scene;
    scene = NULL;
    compositeTimer.stop();
    mousePollingTimer.stop();
    repaints_region = QRegion();
    for (ClientList::ConstIterator it = clients.constBegin();
            it != clients.constEnd();
            ++it) {
        // forward all opacity values to the frame in case there'll be other CM running
        if ((*it)->opacity() != 1.0) {
            NETWinInfo2 i(display(), (*it)->frameId(), rootWindow(), 0);
            i.setOpacity(static_cast< unsigned long >((*it)->opacity() * 0xffffffff));
        }
    }
    discardPopup(); // force re-creation of the Alt+F3 popup (opacity option)
    // discard all Deleted windows (#152914)
    while (!deleted.isEmpty())
        deleted.first()->discard(Allowed);
    m_finishingCompositing = false;
#endif
}

// OpenGL self-check failed, fallback to XRender
void Workspace::fallbackToXRenderCompositing()
{
    finishCompositing();
    options->compositingMode = XRenderCompositing;
    setupCompositing();
}

void Workspace::lostCMSelection()
{
    kDebug(1212) << "Lost compositing manager selection";
    finishCompositing();
}

// for the shortcut
void Workspace::slotToggleCompositing()
{
    suspendCompositing(!compositingSuspended);
}

// for the dbus call
void Workspace::toggleCompositing()
{
    slotToggleCompositing();
    if (compositingSuspended) {
        // when disabled show a shortcut how the user can get back compositing
        QString shortcut, message;
        if (KAction* action = qobject_cast<KAction*>(keys->action("Suspend Compositing")))
            shortcut = action->globalShortcut().primary().toString(QKeySequence::NativeText);
        if (!shortcut.isEmpty()) {
            // display notification only if there is the shortcut
            message = i18n("Desktop effects have been suspended by another application.<br/>"
                           "You can resume using the '%1' shortcut.", shortcut);
            Notify::raise(Notify::CompositingSuspendedDbus, message);
        }
    }
}

void Workspace::updateCompositeBlocking(Client *c)
{
    if (c) { // if c == 0 we just check if we can resume
        if (c->isBlockingCompositing()) {
            compositingBlocked = true;
            suspendCompositing(true);
        }
    }
    else if (compositingBlocked) {  // lost a client and we're blocked - can we resume?
        // NOTICE do NOT check for "compositingSuspended" or "!compositing()"
        // only "resume" if it was really disabled for a block
        bool resume = true;
        for (ClientList::ConstIterator it = clients.constBegin(); it != clients.constEnd(); ++it) {
            if ((*it)->isBlockingCompositing()) {
                resume = false;
                break;
            }
        }
        if (resume) { // do NOT attempt to call suspendCompositing(false); from within the eventchain!
            compositingBlocked = false;
            if (compositingSuspended)
                QMetaObject::invokeMethod(this, "slotToggleCompositing", Qt::QueuedConnection);
        }
    }
}

void Workspace::suspendCompositing()
{
    suspendCompositing(true);
}

void Workspace::suspendCompositing(bool suspend)
{
    compositingSuspended = suspend;
    finishCompositing();
    setupCompositing(); // will do nothing if suspended
    // notify decorations that composition state has changed
    if (hasDecorationPlugin()) {
        KDecorationFactory* factory = mgr->factory();
        factory->reset(SettingCompositing);
    }
    emit compositingToggled(!compositingSuspended);
}

void Workspace::resetCompositing()
{
    if (compositing()) {
        finishCompositing();
        QTimer::singleShot(0, this, SLOT(setupCompositing()));
    }
}

void Workspace::addRepaint(int x, int y, int w, int h)
{
    if (!compositing())
        return;
    repaints_region += QRegion(x, y, w, h);
    checkCompositeTimer();
}

void Workspace::addRepaint(const QRect& r)
{
    if (!compositing())
        return;
    repaints_region += r;
    checkCompositeTimer();
}

void Workspace::addRepaint(const QRegion& r)
{
    if (!compositing())
        return;
    repaints_region += r;
    checkCompositeTimer();
}

void Workspace::addRepaintFull()
{
    if (!compositing())
        return;
    repaints_region = QRegion(0, 0, displayWidth(), displayHeight());
    checkCompositeTimer();
}

void Workspace::timerEvent(QTimerEvent *te)
{
    if (te->timerId() == compositeTimer.timerId()) {
        compositeTimer.stop();
        performCompositing();
    } else
        QObject::timerEvent(te);
}

void Workspace::performCompositing()
{
#ifdef KWIN_HAVE_COMPOSITING
    if (((repaints_region.isEmpty() && !windowRepaintsPending())  // no damage
            || !overlay_visible)) { // nothing is visible anyway
        vBlankPadding += 3;
        scene->idle();
        // Note: It would seem here we should undo suspended unredirect, but when scenes need
        // it for some reason, e.g. transformations or translucency, the next pass that does not
        // need this anymore and paints normally will also reset the suspended unredirect.
        // Otherwise the window would not be painted normally anyway.
        return;
    }
    // create a list of all windows in the stacking order
    ToplevelList windows = xStackingOrder();
    foreach (EffectWindow * c, static_cast< EffectsHandlerImpl* >(effects)->elevatedWindows()) {
        Toplevel* t = static_cast< EffectWindowImpl* >(c)->window();
        windows.removeAll(t);
        windows.append(t);
    }
#if 0
    // skip windows that are not yet ready for being painted
    ToplevelList tmp = windows;
    windows.clear();
    // There is a bug somewhere that prevents this from working properly (#160393), but additionally
    // this cannot be used so carelessly - needs protections against broken clients, the window
    // should not get focus before it's displayed, handle unredirected windows properly and so on.
    foreach (Toplevel * c, tmp)
    if (c->readyForPainting())
        windows.append(c);
#endif
    foreach (Toplevel * c, windows) {
        // This could be possibly optimized WRT obscuring, but that'd need being already
        // past prePaint() phase - probably not worth it.
        repaints_region |= c->repaints().translated(c->pos());
        repaints_region |= c->decorationPendingRegion();
        c->resetRepaints(c->decorationRect());
        if (c->hasShadow()) {
            c->resetRepaints(c->shadow()->shadowRegion().boundingRect());
        }
    }
    QRegion repaints = repaints_region;
    // clear all repaints, so that post-pass can add repaints for the next repaint
    repaints_region = QRegion();
    QElapsedTimer t;
    t.start();
    if (scene->waitSyncAvailable()) {
        // vsync: paint the scene, than rebase the timer and use the duration for next timeout estimation
        scene->paint(repaints, windows);
        nextPaintReference = QDateTime::currentMSecsSinceEpoch();
    } else {
        // no vsyc -> inversion: reset the timer, then paint the scene, this way we can provide a constant framerate
        nextPaintReference = QDateTime::currentMSecsSinceEpoch();
        scene->paint(repaints, windows);
    }
    // reset the roundin error corrective... :-(
    vBlankPadding = 3;
    // Trigger at least one more pass even if there would be nothing to paint, so that scene->idle()
    // is called the next time. If there would be nothing pending, it will not restart the timer and
    // checkCompositeTime() would restart it again somewhen later, called from functions that
    // would again add something pending.
    checkCompositeTimer();
#endif
}

void Workspace::performMousePoll()
{
    checkCursorPos();
}

bool Workspace::windowRepaintsPending() const
{
    foreach (Toplevel * c, clients)
    if (!c->repaints().isEmpty())
        return true;
    foreach (Toplevel * c, desktops)
    if (!c->repaints().isEmpty())
        return true;
    foreach (Toplevel * c, unmanaged)
    if (!c->repaints().isEmpty())
        return true;
    foreach (Toplevel * c, deleted)
    if (!c->repaints().isEmpty())
        return true;
    return false;
}

void Workspace::setCompositeTimer()
{
    if (!compositing())  // should not really happen, but there may be e.g. some damage events still pending
        return;

    // interval - "time since last paint completion" - "time we need to paint"
    uint passed = (QDateTime::currentMSecsSinceEpoch() - nextPaintReference) << 10;
    uint delay = fpsInterval;
    if (scene->waitSyncAvailable()) {
        if (passed > fpsInterval) {
            delay = vBlankInterval;
            passed %= vBlankInterval;
        }
        delay -= ((passed + ((scene->estimatedRenderTime() + vBlankPadding) << 10)) % vBlankInterval);
    } else
        delay = qBound(0, int(delay - passed), 250 << 10);

    compositeTimer.start(delay >> 10, this);
}

void Workspace::startMousePolling()
{
    mousePollingTimer.start(20);   // 50Hz. TODO: How often do we really need to poll?
}

void Workspace::stopMousePolling()
{
    mousePollingTimer.stop();
}

bool Workspace::createOverlay()
{
    assert(overlay == None);
    if (!Extensions::compositeOverlayAvailable())
        return false;
    if (!Extensions::shapeInputAvailable())  // needed in setupOverlay()
        return false;
#ifdef HAVE_XCOMPOSITE_OVERLAY
    overlay = XCompositeGetOverlayWindow(display(), rootWindow());
    if (overlay == None)
        return false;
    XResizeWindow(display(), overlay, displayWidth(), displayHeight());
    return true;
#else
    return false;
#endif
}

void Workspace::setupOverlay(Window w)
{
    assert(overlay != None);
    assert(Extensions::shapeInputAvailable());
    XSetWindowBackgroundPixmap(display(), overlay, None);
    overlay_shape = QRegion();
    setOverlayShape(QRect(0, 0, displayWidth(), displayHeight()));
    if (w != None) {
        XSetWindowBackgroundPixmap(display(), w, None);
        XShapeCombineRectangles(display(), w, ShapeInput, 0, 0, NULL, 0, ShapeSet, Unsorted);
    }
    XSelectInput(display(), overlay, VisibilityChangeMask);
}

void Workspace::showOverlay()
{
    assert(overlay != None);
    if (overlay_shown)
        return;
    XMapSubwindows(display(), overlay);
    XMapWindow(display(), overlay);
    overlay_shown = true;
}

void Workspace::hideOverlay()
{
    assert(overlay != None);
    XUnmapWindow(display(), overlay);
    overlay_shown = false;
    setOverlayShape(QRect(0, 0, displayWidth(), displayHeight()));
}

void Workspace::setOverlayShape(const QRegion& reg)
{
    // Avoid setting the same shape again, it causes flicker (apparently it is not a no-op
    // and triggers something).
    if (reg == overlay_shape)
        return;
    QVector< QRect > rects = reg.rects();
    XRectangle* xrects = new XRectangle[ rects.count()];
    for (int i = 0;
            i < rects.count();
            ++i) {
        xrects[ i ].x = rects[ i ].x();
        xrects[ i ].y = rects[ i ].y();
        xrects[ i ].width = rects[ i ].width();
        xrects[ i ].height = rects[ i ].height();
    }
    XShapeCombineRectangles(display(), overlay, ShapeBounding, 0, 0,
                            xrects, rects.count(), ShapeSet, Unsorted);
    delete[] xrects;
    XShapeCombineRectangles(display(), overlay, ShapeInput, 0, 0, NULL, 0, ShapeSet, Unsorted);
    overlay_shape = reg;
}

void Workspace::destroyOverlay()
{
    if (overlay == None)
        return;
    // reset the overlay shape
    XRectangle rec = { 0, 0, displayWidth(), displayHeight() };
    XShapeCombineRectangles(display(), overlay, ShapeBounding, 0, 0, &rec, 1, ShapeSet, Unsorted);
    XShapeCombineRectangles(display(), overlay, ShapeInput, 0, 0, &rec, 1, ShapeSet, Unsorted);
#ifdef HAVE_XCOMPOSITE_OVERLAY
    XCompositeReleaseOverlayWindow(display(), overlay);
#endif
    overlay = None;
    overlay_shown = false;
}

bool Workspace::compositingActive()
{
    return !m_finishingCompositing && compositing();
}

// force is needed when the list of windows changes (e.g. a window goes away)
void Workspace::checkUnredirect(bool force)
{
    if (!compositing() || overlay == None || !options->unredirectFullscreen)
        return;
    if (force)
        forceUnredirectCheck = true;
    if (!unredirectTimer.isActive())
        unredirectTimer.start(0);
}

void Workspace::delayedCheckUnredirect()
{
    if (!compositing() || overlay == None || !options->unredirectFullscreen)
        return;
    ToplevelList list;
    bool changed = forceUnredirectCheck;
    foreach (Client * c, clients)
    list.append(c);
    foreach (Unmanaged * c, unmanaged)
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
    setOverlayShape(reg);
}

//****************************************
// Toplevel
//****************************************

void Toplevel::setupCompositing()
{
#ifdef KWIN_HAVE_COMPOSITING
    if (!compositing())
        return;
    if (damage_handle != None)
        return;
    damage_handle = XDamageCreate(display(), frameId(), XDamageReportRawRectangles);
    damage_region = QRegion(0, 0, width(), height());
    effect_window = new EffectWindowImpl();
    effect_window->setWindow(this);
    unredirect = false;
    workspace()->checkUnredirect(true);
#endif
}

void Toplevel::finishCompositing()
{
#ifdef KWIN_HAVE_COMPOSITING
    if (damage_handle == None)
        return;
    workspace()->checkUnredirect(true);
    if (effect_window->window() == this) { // otherwise it's already passed to Deleted, don't free data
        discardWindowPixmap();
        delete effect_window;
    }
    XDamageDestroy(display(), damage_handle);
    damage_handle = None;
    damage_region = QRegion();
    repaints_region = QRegion();
    effect_window = NULL;
#endif
}

void Toplevel::discardWindowPixmap()
{
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
#ifdef KWIN_HAVE_COMPOSITING
    assert(compositing());
    if (unredirected())
        return None;
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
#else
    return None;
#endif
}

#ifdef HAVE_XDAMAGE
// We must specify that the two events are a union so the compiler doesn't
// complain about strict aliasing rules.
typedef union {
    XEvent e;
    XDamageNotifyEvent de;
} EventUnion;

void Toplevel::damageNotifyEvent(XDamageNotifyEvent* e)
{
    QRegion damage(e->area.x, e->area.y, e->area.width, e->area.height);
    // compress
    int cnt = 1;
    while (XPending(display())) {
        EventUnion e2;
        if (XPeekEvent(display(), &e2.e) && e2.e.type == Extensions::damageNotifyEvent()
                && e2.e.xany.window == frameId()) {
            XNextEvent(display(), &e2.e);
            if (cnt > 200) {
                // If there are way too many damage events in the queue, just discard them
                // and damage the whole window. Otherwise the X server can just overload
                // us with a flood of damage events. Should be probably optimized
                // in the X server, as this is rather lame.
                damage = rect();
                continue;
            }
            QRect r(e2.de.area.x, e2.de.area.y, e2.de.area.width, e2.de.area.height);
            ++cnt;
            // If there are too many damaged rectangles, increase them
            // to be multiples of 100x100 px grid, since QRegion get quite
            // slow with many rectangles, and there is little to gain by using
            // many small rectangles (rather the opposite, several large should
            // be often faster).
            if (cnt > 50) {
                r.setLeft(r.left() / 100 * 100);
                r.setRight((r.right() + 99) / 100 * 100);
                r.setTop(r.top() / 100 * 100);
                r.setBottom((r.bottom() + 99) / 100 * 100);
            }
            damage += r;
            continue;
        }
        break;
    }
    foreach (const QRect & r, damage.rects())
    addDamage(r);
}

void Client::damageNotifyEvent(XDamageNotifyEvent* e)
{
    Toplevel::damageNotifyEvent(e);
#ifdef HAVE_XSYNC
    if (sync_counter == None)   // cannot detect complete redraw, consider done now
        ready_for_painting = true;
#else
    ready_for_painting = true; // no sync at all, consider done now
#endif
}
#endif

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
    damage_region += r;
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
    workspace()->checkCompositeTimer();
}

void Toplevel::addDamageFull()
{
    if (!compositing())
        return;
    damage_region = rect();
    repaints_region = rect();
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
    workspace()->checkCompositeTimer();
}

void Toplevel::resetDamage(const QRect& r)
{
    damage_region -= r;
}

void Toplevel::addRepaint(const QRect& r)
{
    addRepaint(r.x(), r.y(), r.width(), r.height());
}

void Toplevel::addRepaint(int x, int y, int w, int h)
{
    if (!compositing())
        return;
    QRect r(x, y, w, h);
    r &= rect();
    repaints_region += r;
    workspace()->checkCompositeTimer();
}

void Toplevel::addRepaintFull()
{
    repaints_region = rect();
    if (hasShadow()) {
        repaints_region = repaints_region.united(shadow()->shadowRegion());
    }
    workspace()->checkCompositeTimer();
}

void Toplevel::resetRepaints(const QRect& r)
{
    repaints_region -= r;
}

void Toplevel::addWorkspaceRepaint(int x, int y, int w, int h)
{
    addWorkspaceRepaint(QRect(x, y, w, h));
}

void Toplevel::addWorkspaceRepaint(const QRect& r2)
{
    if (!compositing())
        return;
    workspace()->addRepaint(r2);
}

bool Toplevel::updateUnredirectedState()
{
    assert(compositing());
    bool should = shouldUnredirect() && !unredirectSuspend && !shape() && !hasAlpha() && opacity() == 1.0 &&
                  !static_cast<EffectsHandlerImpl*>(effects)->activeFullScreenEffect();
    if (should && !unredirect) {
        unredirect = true;
        kDebug(1212) << "Unredirecting:" << this;
#ifdef HAVE_XCOMPOSITE
        XCompositeUnredirectWindow(display(), frameId(), CompositeRedirectManual);
#endif
        return true;
    } else if (!should && unredirect) {
        unredirect = false;
        kDebug(1212) << "Redirecting:" << this;
#ifdef HAVE_XCOMPOSITE
        XCompositeRedirectWindow(display(), frameId(), CompositeRedirectManual);
#endif
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
    workspace()->checkUnredirect();
}

//****************************************
// Client
//****************************************

void Client::setupCompositing()
{
    Toplevel::setupCompositing();
    updateVisibility(); // for internalKeep()
    updateDecoration(true, true);
}

void Client::finishCompositing()
{
    Toplevel::finishCompositing();
    updateVisibility();
    updateDecoration(true, true);
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

void Client::addRepaintFull()
{
    repaints_region = decorationRect();
    if (hasShadow()) {
        repaints_region = repaints_region.united(shadow()->shadowRegion());
    }
    workspace()->checkCompositeTimer();
}

//****************************************
// Unmanaged
//****************************************

bool Unmanaged::shouldUnredirect() const
{
    // the pixmap is needed for the login effect, a nicer solution would be the login effect increasing
    // refcount for the window pixmap (which would prevent unredirect), avoiding this hack
    if (resourceClass() == "ksplashx" || resourceClass() == "ksplashsimple")
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

void Deleted::addRepaintFull()
{
    repaints_region = decorationRect();
    if (hasShadow()) {
        repaints_region = repaints_region.united(shadow()->shadowRegion());
    }
    workspace()->checkCompositeTimer();
}

} // namespace
