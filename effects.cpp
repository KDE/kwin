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

#include "effects.h"

#include "deleted.h"
#include "client.h"
#include "group.h"
#include "scene_xrender.h"
#include "scene_opengl.h"
#include "unmanaged.h"
#include "workspace.h"
#include "kwinglutils.h"

#include <QFile>

#include "kdebug.h"
#include "klibrary.h"
#include "kdesktopfile.h"
#include "kconfiggroup.h"
#include "kstandarddirs.h"
#include <kservice.h>
#include <kservicetypetrader.h>
#include <kplugininfo.h>
#include <Plasma/Theme>

#include <assert.h>


namespace KWin
{

//---------------------
// Static

static QByteArray readWindowProperty(Window win, long atom, long type, int format)
{
    int len = 32768;
    for (;;) {
        unsigned char* data;
        Atom rtype;
        int rformat;
        unsigned long nitems, after;
        if (XGetWindowProperty(QX11Info::display(), win,
                              atom, 0, len, False, AnyPropertyType,
                              &rtype, &rformat, &nitems, &after, &data) == Success) {
            if (after > 0) {
                XFree(data);
                len *= 2;
                continue;
            }
            if (long(rtype) == type && rformat == format) {
                int bytelen = format == 8 ? nitems : format == 16 ? nitems * sizeof(short) : nitems * sizeof(long);
                QByteArray ret(reinterpret_cast< const char* >(data), bytelen);
                XFree(data);
                return ret;
            } else { // wrong format, type or something
                XFree(data);
                return QByteArray();
            }
        } else // XGetWindowProperty() failed
            return QByteArray();
    }
}

static void deleteWindowProperty(Window win, long int atom)
{
    XDeleteProperty(QX11Info::display(), win, atom);
}

//---------------------

EffectsHandlerImpl::EffectsHandlerImpl(CompositingType type)
    : EffectsHandler(type)
    , keyboard_grab_effect(NULL)
    , fullscreen_effect(0)
    , next_window_quad_type(EFFECT_QUAD_TYPE_START)
    , mouse_poll_ref_count(0)
    , current_paint_effectframe(0)
{
    Workspace *ws = Workspace::self();
    connect(ws, SIGNAL(currentDesktopChanged(int)), this, SLOT(slotDesktopChanged(int)));
    connect(ws, SIGNAL(clientAdded(KWin::Client*)), this, SLOT(slotClientAdded(KWin::Client*)));
    connect(ws, SIGNAL(unmanagedAdded(KWin::Unmanaged*)), this, SLOT(slotUnmanagedAdded(KWin::Unmanaged*)));
    connect(ws, SIGNAL(clientActivated(KWin::Client*)), this, SLOT(slotClientActivated(KWin::Client*)));
    connect(ws, SIGNAL(deletedRemoved(KWin::Deleted*)), this, SLOT(slotDeletedRemoved(KWin::Deleted*)));
    // connect all clients
    foreach (Client *c, ws->clientList()) {
        connect(c, SIGNAL(clientClosed(KWin::Client*)), this, SLOT(slotClientClosed(KWin::Client*)));
    }
    foreach (Unmanaged *u, ws->unmanagedList()) {
        connect(u, SIGNAL(unmanagedClosed(KWin::Unmanaged*)), this, SLOT(slotUnmanagedClosed(KWin::Unmanaged*)));
    }
    reconfigure();
}

EffectsHandlerImpl::~EffectsHandlerImpl()
{
    if (keyboard_grab_effect != NULL)
        ungrabKeyboard();
    foreach (const EffectPair & ep, loaded_effects)
    unloadEffect(ep.first);
    foreach (const InputWindowPair & pos, input_windows)
    XDestroyWindow(display(), pos.second);
}

void EffectsHandlerImpl::reconfigure()
{
    KSharedConfig::Ptr _config = KGlobal::config();
    KConfigGroup conf(_config, "Plugins");

    KService::List offers = KServiceTypeTrader::self()->query("KWin/Effect");
    QStringList effectsToBeLoaded;
    // First unload necessary effects
    foreach (const KService::Ptr & service, offers) {
        KPluginInfo plugininfo(service);
        plugininfo.load(conf);

        bool isloaded = isEffectLoaded(plugininfo.pluginName());
        bool shouldbeloaded = plugininfo.isPluginEnabled();
        if (!shouldbeloaded && isloaded)
            unloadEffect(plugininfo.pluginName());
        if (shouldbeloaded)
            effectsToBeLoaded.append(plugininfo.pluginName());
    }
    QStringList newLoaded;
    // Then load those that should be loaded
    foreach (const QString & effectName, effectsToBeLoaded) {
        if (!isEffectLoaded(effectName)) {
            loadEffect(effectName);
            newLoaded.append(effectName);
        }
    }
    foreach (const EffectPair & ep, loaded_effects) {
        if (!newLoaded.contains(ep.first))    // don't reconfigure newly loaded effects
            ep.second->reconfigure(Effect::ReconfigureAll);
    }
}

// the idea is that effects call this function again which calls the next one
void EffectsHandlerImpl::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (current_paint_screen < loaded_effects.size()) {
        loaded_effects[current_paint_screen++].second->prePaintScreen(data, time);
        --current_paint_screen;
    }
    // no special final code
}

void EffectsHandlerImpl::paintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    if (current_paint_screen < loaded_effects.size()) {
        loaded_effects[current_paint_screen++].second->paintScreen(mask, region, data);
        --current_paint_screen;
    } else
        scene->finalPaintScreen(mask, region, data);
}

void EffectsHandlerImpl::postPaintScreen()
{
    if (current_paint_screen < loaded_effects.size()) {
        loaded_effects[current_paint_screen++].second->postPaintScreen();
        --current_paint_screen;
    }
    // no special final code
}

void EffectsHandlerImpl::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    if (current_paint_window < loaded_effects.size()) {
        loaded_effects[current_paint_window++].second->prePaintWindow(w, data, time);
        --current_paint_window;
    }
    // no special final code
}

void EffectsHandlerImpl::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (current_paint_window < loaded_effects.size()) {
        loaded_effects[current_paint_window++].second->paintWindow(w, mask, region, data);
        --current_paint_window;
    } else
        scene->finalPaintWindow(static_cast<EffectWindowImpl*>(w), mask, region, data);
}

void EffectsHandlerImpl::paintEffectFrame(EffectFrame* frame, QRegion region, double opacity, double frameOpacity)
{
    if (current_paint_effectframe < loaded_effects.size()) {
        loaded_effects[current_paint_effectframe++].second->paintEffectFrame(frame, region, opacity, frameOpacity);
        --current_paint_effectframe;
    } else {
        const EffectFrameImpl* frameImpl = static_cast<const EffectFrameImpl*>(frame);
        frameImpl->finalRender(region, opacity, frameOpacity);
    }
}

void EffectsHandlerImpl::postPaintWindow(EffectWindow* w)
{
    if (current_paint_window < loaded_effects.size()) {
        loaded_effects[current_paint_window++].second->postPaintWindow(w);
        --current_paint_window;
    }
    // no special final code
}

bool EffectsHandlerImpl::provides(Effect::Feature ef)
{
    for (int i = 0; i < loaded_effects.size(); ++i)
        if (loaded_effects.at(i).second->provides(ef))
            return true;
    return false;
}

void EffectsHandlerImpl::drawWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (current_draw_window < loaded_effects.size()) {
        loaded_effects[current_draw_window++].second->drawWindow(w, mask, region, data);
        --current_draw_window;
    } else
        scene->finalDrawWindow(static_cast<EffectWindowImpl*>(w), mask, region, data);
}

void EffectsHandlerImpl::buildQuads(EffectWindow* w, WindowQuadList& quadList)
{
    if (current_build_quads < loaded_effects.size()) {
        loaded_effects[current_build_quads++].second->buildQuads(w, quadList);
        --current_build_quads;
    }
}

bool EffectsHandlerImpl::hasDecorationShadows() const
{
    return Workspace::self()->hasDecorationShadows();
}

bool EffectsHandlerImpl::decorationsHaveAlpha() const
{
    return Workspace::self()->decorationHasAlpha();
}

bool EffectsHandlerImpl::decorationSupportsBlurBehind() const
{
    return Workspace::self()->decorationSupportsBlurBehind();
}

// start another painting pass
void EffectsHandlerImpl::startPaint()
{
    assert(current_paint_screen == 0);
    assert(current_paint_window == 0);
    assert(current_draw_window == 0);
    assert(current_build_quads == 0);
    assert(current_transform == 0);
}

void EffectsHandlerImpl::windowUserMovedResized(EffectWindow* c, bool first, bool last)
{
    foreach (const EffectPair & ep, loaded_effects)
    ep.second->windowUserMovedResized(c, first, last);
}

void EffectsHandlerImpl::windowMoveResizeGeometryUpdate(EffectWindow* c, const QRect& geometry)
{
    foreach (const EffectPair & ep, loaded_effects)
    ep.second->windowMoveResizeGeometryUpdate(c, geometry);
}

void EffectsHandlerImpl::windowOpacityChanged(EffectWindow* c, double old_opacity)
{
    if (!c)
        return;
    if (static_cast<EffectWindowImpl*>(c)->window()->opacity() == old_opacity)
        return;
    foreach (const EffectPair & ep, loaded_effects)
    ep.second->windowOpacityChanged(c, old_opacity);
}

void EffectsHandlerImpl::slotClientAdded(Client *c)
{
    connect(c, SIGNAL(clientClosed(KWin::Client*)), this, SLOT(slotClientClosed(KWin::Client*)));
    emit windowAdded(c->effectWindow());
}

void EffectsHandlerImpl::slotUnmanagedAdded(Unmanaged *u)
{
    connect(u, SIGNAL(unmanagedClosed(KWin::Unmanaged*)), this, SLOT(slotUnmanagedClosed(KWin::Unmanaged*)));
    emit windowAdded(u->effectWindow());
}

void EffectsHandlerImpl::slotDeletedRemoved(KWin::Deleted *d)
{
    emit windowDeleted(d->effectWindow());
    elevated_windows.removeAll(d->effectWindow());
}

void EffectsHandlerImpl::slotClientClosed(Client *c)
{
    emit windowClosed(c->effectWindow());
}

void EffectsHandlerImpl::slotUnmanagedClosed(Unmanaged* u)
{
    emit windowClosed(u->effectWindow());
}

void EffectsHandlerImpl::slotClientActivated(KWin::Client *c)
{
    emit windowActivated(c ? c->effectWindow() : NULL);
}

void EffectsHandlerImpl::windowMinimized(EffectWindow* c)
{
    foreach (const EffectPair & ep, loaded_effects)
    ep.second->windowMinimized(c);
}

void EffectsHandlerImpl::windowUnminimized(EffectWindow* c)
{
    foreach (const EffectPair & ep, loaded_effects)
    ep.second->windowUnminimized(c);
}

void EffectsHandlerImpl::clientGroupItemSwitched(EffectWindow* from, EffectWindow* to)
{
    foreach (const EffectPair & ep, loaded_effects)
    ep.second->clientGroupItemSwitched(from, to);
}

void EffectsHandlerImpl::clientGroupItemAdded(EffectWindow* from, EffectWindow* to)
{
    foreach (const EffectPair & ep, loaded_effects)
    ep.second->clientGroupItemAdded(from, to);
}

void EffectsHandlerImpl::clientGroupItemRemoved(EffectWindow* c, EffectWindow* group)
{
    foreach (const EffectPair & ep, loaded_effects)
    ep.second->clientGroupItemRemoved(c, group);
}

void EffectsHandlerImpl::slotDesktopChanged(int old)
{
    const int newDesktop = Workspace::self()->currentDesktop();
    if (old != 0 && newDesktop != old) {
        emit desktopChanged(old, newDesktop);
    }
}

void EffectsHandlerImpl::windowDamaged(EffectWindow* w, const QRect& r)
{
    if (w == NULL)
        return;
    foreach (const EffectPair & ep, loaded_effects)
    ep.second->windowDamaged(w, r);
}

void EffectsHandlerImpl::windowGeometryShapeChanged(EffectWindow* w, const QRect& old)
{
    if (w == NULL)   // during late cleanup effectWindow() may be already NULL
        return;     // in some functions that may still call this
    foreach (const EffectPair & ep, loaded_effects)
    ep.second->windowGeometryShapeChanged(w, old);
}

void EffectsHandlerImpl::tabBoxAdded(int mode)
{
    foreach (const EffectPair & ep, loaded_effects)
    ep.second->tabBoxAdded(mode);
}

void EffectsHandlerImpl::tabBoxClosed()
{
    foreach (const EffectPair & ep, loaded_effects)
    ep.second->tabBoxClosed();
}

void EffectsHandlerImpl::tabBoxUpdated()
{
    foreach (const EffectPair & ep, loaded_effects)
    ep.second->tabBoxUpdated();
}

void EffectsHandlerImpl::tabBoxKeyEvent(QKeyEvent* event)
{
    foreach (const EffectPair & ep, loaded_effects)
    ep.second->tabBoxKeyEvent(event);
}

void EffectsHandlerImpl::setActiveFullScreenEffect(Effect* e)
{
    fullscreen_effect = e;
    Workspace::self()->checkUnredirect();
}

Effect* EffectsHandlerImpl::activeFullScreenEffect() const
{
    return fullscreen_effect;
}

bool EffectsHandlerImpl::borderActivated(ElectricBorder border)
{
    bool ret = false;
    foreach (const EffectPair & ep, loaded_effects)
    if (ep.second->borderActivated(border))
        ret = true; // bail out or tell all?
    return ret;
}

void EffectsHandlerImpl::mouseChanged(const QPoint& pos, const QPoint& oldpos,
                                      Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                                      Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers)
{
    foreach (const EffectPair & ep, loaded_effects)
    ep.second->mouseChanged(pos, oldpos, buttons, oldbuttons, modifiers, oldmodifiers);
}

bool EffectsHandlerImpl::grabKeyboard(Effect* effect)
{
    if (keyboard_grab_effect != NULL)
        return false;
    bool ret = grabXKeyboard();
    if (!ret)
        return false;
    keyboard_grab_effect = effect;
    return true;
}

void EffectsHandlerImpl::ungrabKeyboard()
{
    assert(keyboard_grab_effect != NULL);
    ungrabXKeyboard();
    keyboard_grab_effect = NULL;
}

void EffectsHandlerImpl::grabbedKeyboardEvent(QKeyEvent* e)
{
    if (keyboard_grab_effect != NULL)
        keyboard_grab_effect->grabbedKeyboardEvent(e);
}

void* EffectsHandlerImpl::getProxy(QString name)
{
    // All effects start with "kwin4_effect_", prepend it to the name
    name.prepend("kwin4_effect_");

    for (QVector< EffectPair >::iterator it = loaded_effects.begin(); it != loaded_effects.end(); ++it)
        if ((*it).first == name)
            return (*it).second->proxy();

    return NULL;
}

void EffectsHandlerImpl::startMousePolling()
{
    if (!mouse_poll_ref_count)   // Start timer if required
        Workspace::self()->startMousePolling();
    mouse_poll_ref_count++;
}

void EffectsHandlerImpl::stopMousePolling()
{
    assert(mouse_poll_ref_count);
    mouse_poll_ref_count--;
    if (!mouse_poll_ref_count)   // Stop timer if required
        Workspace::self()->stopMousePolling();
}

bool EffectsHandlerImpl::hasKeyboardGrab() const
{
    return keyboard_grab_effect != NULL;
}

void EffectsHandlerImpl::propertyNotify(EffectWindow* c, long atom)
{
    if (!registered_atoms.contains(atom))
        return;
    foreach (const EffectPair & ep, loaded_effects)
    ep.second->propertyNotify(c, atom);
}

void EffectsHandlerImpl::numberDesktopsChanged(int old)
{
    foreach (const EffectPair & ep, loaded_effects)
    ep.second->numberDesktopsChanged(old);
}

void EffectsHandlerImpl::registerPropertyType(long atom, bool reg)
{
    if (reg)
        ++registered_atoms[ atom ]; // initialized to 0 if not present yet
    else {
        if (--registered_atoms[ atom ] == 0)
            registered_atoms.remove(atom);
    }
}

QByteArray EffectsHandlerImpl::readRootProperty(long atom, long type, int format) const
{
    return readWindowProperty(rootWindow(), atom, type, format);
}

void EffectsHandlerImpl::deleteRootProperty(long atom) const
{
    deleteWindowProperty(rootWindow(), atom);
}

void EffectsHandlerImpl::activateWindow(EffectWindow* c)
{
    if (Client* cl = dynamic_cast< Client* >(static_cast<EffectWindowImpl*>(c)->window()))
        Workspace::self()->activateClient(cl, true);
}

EffectWindow* EffectsHandlerImpl::activeWindow() const
{
    return Workspace::self()->activeClient() ? Workspace::self()->activeClient()->effectWindow() : NULL;
}

void EffectsHandlerImpl::moveWindow(EffectWindow* w, const QPoint& pos, bool snap, double snapAdjust)
{
    Client* cl = dynamic_cast< Client* >(static_cast<EffectWindowImpl*>(w)->window());
    if (!cl || !cl->isMovable())
        return;

    if (snap)
        cl->move(Workspace::self()->adjustClientPosition(cl, pos, true, snapAdjust));
    else
        cl->move(pos);
}

void EffectsHandlerImpl::windowToDesktop(EffectWindow* w, int desktop)
{
    Client* cl = dynamic_cast< Client* >(static_cast<EffectWindowImpl*>(w)->window());
    if (cl && !cl->isDesktop() && !cl->isDock() && !cl->isTopMenu())
        Workspace::self()->sendClientToDesktop(cl, desktop, true);
}

void EffectsHandlerImpl::windowToScreen(EffectWindow* w, int screen)
{
    Client* cl = dynamic_cast< Client* >(static_cast<EffectWindowImpl*>(w)->window());
    if (cl && !cl->isDesktop() && !cl->isDock() && !cl->isTopMenu())
        Workspace::self()->sendClientToScreen(cl, screen);
}

void EffectsHandlerImpl::setShowingDesktop(bool showing)
{
    Workspace::self()->setShowingDesktop(showing);
}


int EffectsHandlerImpl::currentDesktop() const
{
    return Workspace::self()->currentDesktop();
}

int EffectsHandlerImpl::numberOfDesktops() const
{
    return Workspace::self()->numberOfDesktops();
}

void EffectsHandlerImpl::setCurrentDesktop(int desktop)
{
    Workspace::self()->setCurrentDesktop(desktop);
}

void EffectsHandlerImpl::setNumberOfDesktops(int desktops)
{
    Workspace::self()->setNumberOfDesktops(desktops);
}

QSize EffectsHandlerImpl::desktopGridSize() const
{
    return Workspace::self()->desktopGridSize();
}

int EffectsHandlerImpl::desktopGridWidth() const
{
    return Workspace::self()->desktopGridWidth();
}

int EffectsHandlerImpl::desktopGridHeight() const
{
    return Workspace::self()->desktopGridHeight();
}

int EffectsHandlerImpl::workspaceWidth() const
{
    return Workspace::self()->workspaceWidth();
}

int EffectsHandlerImpl::workspaceHeight() const
{
    return Workspace::self()->workspaceHeight();
}

int EffectsHandlerImpl::desktopAtCoords(QPoint coords) const
{
    return Workspace::self()->desktopAtCoords(coords);
}

QPoint EffectsHandlerImpl::desktopGridCoords(int id) const
{
    return Workspace::self()->desktopGridCoords(id);
}

QPoint EffectsHandlerImpl::desktopCoords(int id) const
{
    return Workspace::self()->desktopCoords(id);
}

int EffectsHandlerImpl::desktopAbove(int desktop, bool wrap) const
{
    return Workspace::self()->desktopAbove(desktop, wrap);
}

int EffectsHandlerImpl::desktopToRight(int desktop, bool wrap) const
{
    return Workspace::self()->desktopToRight(desktop, wrap);
}

int EffectsHandlerImpl::desktopBelow(int desktop, bool wrap) const
{
    return Workspace::self()->desktopBelow(desktop, wrap);
}

int EffectsHandlerImpl::desktopToLeft(int desktop, bool wrap) const
{
    return Workspace::self()->desktopToLeft(desktop, wrap);
}

bool EffectsHandlerImpl::isDesktopLayoutDynamic() const
{
    return Workspace::self()->isDesktopLayoutDynamic();
}

int EffectsHandlerImpl::addDesktop(QPoint coords)
{
    return Workspace::self()->addDesktop(coords);
}

void EffectsHandlerImpl::deleteDesktop(int id)
{
    Workspace::self()->deleteDesktop(id);
}

QString EffectsHandlerImpl::desktopName(int desktop) const
{
    return Workspace::self()->desktopName(desktop);
}

bool EffectsHandlerImpl::optionRollOverDesktops() const
{
    return options->rollOverDesktops;
}

double EffectsHandlerImpl::animationTimeFactor() const
{
    return options->animationTimeFactor();
}

WindowQuadType EffectsHandlerImpl::newWindowQuadType()
{
    return WindowQuadType(next_window_quad_type++);
}

int EffectsHandlerImpl::displayWidth() const
{
    return KWin::displayWidth();
}

int EffectsHandlerImpl::displayHeight() const
{
    return KWin::displayWidth();
}

EffectWindow* EffectsHandlerImpl::findWindow(WId id) const
{
    if (Client* w = Workspace::self()->findClient(WindowMatchPredicate(id)))
        return w->effectWindow();
    if (Unmanaged* w = Workspace::self()->findUnmanaged(WindowMatchPredicate(id)))
        return w->effectWindow();
    return NULL;
}

EffectWindowList EffectsHandlerImpl::stackingOrder() const
{
    ClientList list = Workspace::self()->stackingOrder();
    EffectWindowList ret;
    foreach (Client * c, list)
    ret.append(effectWindow(c));
    return ret;
}

void EffectsHandlerImpl::setElevatedWindow(EffectWindow* w, bool set)
{
    elevated_windows.removeAll(w);
    if (set)
        elevated_windows.append(w);
}

void EffectsHandlerImpl::setTabBoxWindow(EffectWindow* w)
{
    if (Client* c = dynamic_cast< Client* >(static_cast< EffectWindowImpl* >(w)->window()))
        Workspace::self()->setTabBoxClient(c);
}

void EffectsHandlerImpl::setTabBoxDesktop(int desktop)
{
    Workspace::self()->setTabBoxDesktop(desktop);
}

EffectWindowList EffectsHandlerImpl::currentTabBoxWindowList() const
{
    EffectWindowList ret;
    ClientList clients = Workspace::self()->currentTabBoxClientList();
    foreach (Client * c, clients)
    ret.append(c->effectWindow());
    return ret;
}

void EffectsHandlerImpl::refTabBox()
{
    Workspace::self()->refTabBox();
}

void EffectsHandlerImpl::unrefTabBox()
{
    Workspace::self()->unrefTabBox();
}

void EffectsHandlerImpl::closeTabBox()
{
    Workspace::self()->closeTabBox();
}

QList< int > EffectsHandlerImpl::currentTabBoxDesktopList() const
{
    return Workspace::self()->currentTabBoxDesktopList();
}

int EffectsHandlerImpl::currentTabBoxDesktop() const
{
    return Workspace::self()->currentTabBoxDesktop();
}

EffectWindow* EffectsHandlerImpl::currentTabBoxWindow() const
{
    if (Client* c = Workspace::self()->currentTabBoxClient())
        return c->effectWindow();
    return NULL;
}

void EffectsHandlerImpl::pushRenderTarget(GLRenderTarget* target)
{
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    target->enable();
    render_targets.push(target);
#endif
}

GLRenderTarget* EffectsHandlerImpl::popRenderTarget()
{
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    GLRenderTarget* ret = render_targets.pop();
    ret->disable();
    if (!render_targets.isEmpty())
        render_targets.top()->enable();
    return ret;
#else
    return 0;
#endif
}

bool EffectsHandlerImpl::isRenderTargetBound()
{
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    return !render_targets.isEmpty();
#else
    return false;
#endif
}

void EffectsHandlerImpl::addRepaintFull()
{
    Workspace::self()->addRepaintFull();
}

void EffectsHandlerImpl::addRepaint(const QRect& r)
{
    Workspace::self()->addRepaint(r);
}

void EffectsHandlerImpl::addRepaint(const QRegion& r)
{
    Workspace::self()->addRepaint(r);
}

void EffectsHandlerImpl::addRepaint(int x, int y, int w, int h)
{
    Workspace::self()->addRepaint(x, y, w, h);
}

int EffectsHandlerImpl::activeScreen() const
{
    return Workspace::self()->activeScreen();
}

int EffectsHandlerImpl::numScreens() const
{
    return Workspace::self()->numScreens();
}

int EffectsHandlerImpl::screenNumber(const QPoint& pos) const
{
    return Workspace::self()->screenNumber(pos);
}

QRect EffectsHandlerImpl::clientArea(clientAreaOption opt, int screen, int desktop) const
{
    return Workspace::self()->clientArea(opt, screen, desktop);
}

QRect EffectsHandlerImpl::clientArea(clientAreaOption opt, const EffectWindow* c) const
{
    const Toplevel* t = static_cast< const EffectWindowImpl* >(c)->window();
    if (const Client* cl = dynamic_cast< const Client* >(t))
        return Workspace::self()->clientArea(opt, cl);
    else
        return Workspace::self()->clientArea(opt, t->geometry().center(), Workspace::self()->currentDesktop());
}

QRect EffectsHandlerImpl::clientArea(clientAreaOption opt, const QPoint& p, int desktop) const
{
    return Workspace::self()->clientArea(opt, p, desktop);
}

Window EffectsHandlerImpl::createInputWindow(Effect* e, int x, int y, int w, int h, const QCursor& cursor)
{
    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    Window win = XCreateWindow(display(), rootWindow(), x, y, w, h, 0, 0, InputOnly, CopyFromParent,
                               CWOverrideRedirect, &attrs);
    // TODO keeping on top?
    // TODO enter/leave notify?
    XSelectInput(display(), win, ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    XDefineCursor(display(), win, cursor.handle());
    XMapWindow(display(), win);
    input_windows.append(qMakePair(e, win));

    // Raise electric border windows above the input windows
    // so they can still be triggered.
    Workspace::self()->raiseElectricBorderWindows();

    return win;
}

void EffectsHandlerImpl::destroyInputWindow(Window w)
{
    foreach (const InputWindowPair & pos, input_windows) {
        if (pos.second == w) {
            input_windows.removeAll(pos);
            XDestroyWindow(display(), w);
            return;
        }
    }
    abort();
}

bool EffectsHandlerImpl::checkInputWindowEvent(XEvent* e)
{
    if (e->type != ButtonPress && e->type != ButtonRelease && e->type != MotionNotify)
        return false;
    foreach (const InputWindowPair & pos, input_windows) {
        if (pos.second == e->xany.window) {
            switch(e->type) {
            case ButtonPress: {
                XButtonEvent* e2 = &e->xbutton;
                Qt::MouseButton button = x11ToQtMouseButton(e2->button);
                Qt::MouseButtons buttons = x11ToQtMouseButtons(e2->state) | button;
                QMouseEvent ev(QEvent::MouseButtonPress,
                               QPoint(e2->x, e2->y), QPoint(e2->x_root, e2->y_root),
                               button, buttons, x11ToQtKeyboardModifiers(e2->state));
                pos.first->windowInputMouseEvent(pos.second, &ev);
                break; // --->
            }
            case ButtonRelease: {
                XButtonEvent* e2 = &e->xbutton;
                Qt::MouseButton button = x11ToQtMouseButton(e2->button);
                Qt::MouseButtons buttons = x11ToQtMouseButtons(e2->state) & ~button;
                QMouseEvent ev(QEvent::MouseButtonRelease,
                               QPoint(e2->x, e2->y), QPoint(e2->x_root, e2->y_root),
                               button, buttons, x11ToQtKeyboardModifiers(e2->state));
                pos.first->windowInputMouseEvent(pos.second, &ev);
                break; // --->
            }
            case MotionNotify: {
                XMotionEvent* e2 = &e->xmotion;
                QMouseEvent ev(QEvent::MouseMove, QPoint(e2->x, e2->y), QPoint(e2->x_root, e2->y_root),
                               Qt::NoButton, x11ToQtMouseButtons(e2->state), x11ToQtKeyboardModifiers(e2->state));
                pos.first->windowInputMouseEvent(pos.second, &ev);
                break; // --->
            }
            }
            return true; // eat event
        }
    }
    return false;
}

void EffectsHandlerImpl::checkInputWindowStacking()
{
    if (input_windows.count() == 0)
        return;
    Window* wins = new Window[ input_windows.count()];
    int pos = 0;
    foreach (const InputWindowPair & it, input_windows)
    wins[ pos++ ] = it.second;
    XRaiseWindow(display(), wins[ 0 ]);
    XRestackWindows(display(), wins, pos);
    delete[] wins;
    // Raise electric border windows above the input windows
    // so they can still be triggered. TODO: Do both at once.
    Workspace::self()->raiseElectricBorderWindows();
}

QPoint EffectsHandlerImpl::cursorPos() const
{
    return Workspace::self()->cursorPos();
}

void EffectsHandlerImpl::checkElectricBorder(const QPoint &pos, Time time)
{
    Workspace::self()->checkElectricBorder(pos, time);
}

void EffectsHandlerImpl::reserveElectricBorder(ElectricBorder border)
{
    Workspace::self()->reserveElectricBorder(border);
}

void EffectsHandlerImpl::unreserveElectricBorder(ElectricBorder border)
{
    Workspace::self()->unreserveElectricBorder(border);
}

void EffectsHandlerImpl::reserveElectricBorderSwitching(bool reserve)
{
    Workspace::self()->reserveElectricBorderSwitching(reserve);
}

unsigned long EffectsHandlerImpl::xrenderBufferPicture()
{
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (SceneXrender* s = dynamic_cast< SceneXrender* >(scene))
        return s->bufferPicture();
#endif
    return None;
}

KLibrary* EffectsHandlerImpl::findEffectLibrary(KService* service)
{
    QString libname = service->library();
    KLibrary* library = new KLibrary(libname);
    if (!library) {
        kError(1212) << "couldn't open library for effect '" <<
                     service->name() << "'" << endl;
        return 0;
    }

    return library;
}

void EffectsHandlerImpl::toggleEffect(const QString& name)
{
    if (isEffectLoaded(name))
        unloadEffect(name);
    else
        loadEffect(name);
}

QStringList EffectsHandlerImpl::loadedEffects() const
{
    QStringList listModules;
    for (QVector< EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        listModules << (*it).first;
    }
    return listModules;
}

QStringList EffectsHandlerImpl::listOfEffects() const
{
    KService::List offers = KServiceTypeTrader::self()->query("KWin/Effect");
    QStringList listOfModules;
    // First unload necessary effects
    foreach (const KService::Ptr & service, offers) {
        KPluginInfo plugininfo(service);
        listOfModules << plugininfo.pluginName();
    }
    return listOfModules;
}

bool EffectsHandlerImpl::loadEffect(const QString& name)
{
    Workspace::self()->addRepaintFull();
    assert(current_paint_screen == 0);
    assert(current_paint_window == 0);
    assert(current_draw_window == 0);
    assert(current_build_quads == 0);
    assert(current_transform == 0);

    if (!name.startsWith(QLatin1String("kwin4_effect_")))
        kWarning(1212) << "Effect names usually have kwin4_effect_ prefix" ;

    // Make sure a single effect won't be loaded multiple times
    for (QVector< EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if ((*it).first == name) {
            kDebug(1212) << "EffectsHandler::loadEffect : Effect already loaded : " << name;
            return true;
        }
    }


    kDebug(1212) << "Trying to load " << name;
    QString internalname = name.toLower();

    QString constraint = QString("[X-KDE-PluginInfo-Name] == '%1'").arg(internalname);
    KService::List offers = KServiceTypeTrader::self()->query("KWin/Effect", constraint);
    if (offers.isEmpty()) {
        kError(1212) << "Couldn't find effect " << name << endl;
        return false;
    }
    KService::Ptr service = offers.first();

    KLibrary* library = findEffectLibrary(service.data());
    if (!library) {
        return false;
    }

    QString version_symbol = "effect_version_" + name;
    KLibrary::void_function_ptr version_func = library->resolveFunction(version_symbol.toAscii());
    if (version_func == NULL) {
        kWarning(1212) << "Effect " << name << " does not provide required API version, ignoring.";
        return false;
    }
    typedef int (*t_versionfunc)();
    int version = reinterpret_cast< t_versionfunc >(version_func)();   // call it
    // Version must be the same or less, but major must be the same.
    // With major 0 minor must match exactly.
    if (version > KWIN_EFFECT_API_VERSION
            || (version >> 8) != KWIN_EFFECT_API_VERSION_MAJOR
            || (KWIN_EFFECT_API_VERSION_MAJOR == 0 && version != KWIN_EFFECT_API_VERSION)) {
        kWarning(1212) << "Effect " << name << " requires unsupported API version " << version;
        return false;
    }
    QString supported_symbol = "effect_supported_" + name;
    KLibrary::void_function_ptr supported_func = library->resolveFunction(supported_symbol.toAscii().data());
    QString create_symbol = "effect_create_" + name;
    KLibrary::void_function_ptr create_func = library->resolveFunction(create_symbol.toAscii().data());
    if (supported_func) {
        typedef bool (*t_supportedfunc)();
        t_supportedfunc supported = reinterpret_cast<t_supportedfunc>(supported_func);
        if (!supported()) {
            kWarning(1212) << "EffectsHandler::loadEffect : Effect " << name << " is not supported" ;
            library->unload();
            return false;
        }
    }
    if (!create_func) {
        kError(1212) << "EffectsHandler::loadEffect : effect_create function not found" << endl;
        library->unload();
        return false;
    }
    typedef Effect*(*t_createfunc)();
    t_createfunc create = reinterpret_cast<t_createfunc>(create_func);

    // Make sure all depenedencies have been loaded
    // TODO: detect circular deps
    KPluginInfo plugininfo(service);
    QStringList dependencies = plugininfo.dependencies();
    foreach (const QString & depName, dependencies) {
        if (!loadEffect(depName)) {
            kError(1212) << "EffectsHandler::loadEffect : Couldn't load dependencies for effect " << name << endl;
            library->unload();
            return false;
        }
    }

    Effect* e = create();

    effect_order.insert(service->property("X-KDE-Ordering").toInt(), EffectPair(name, e));
    effectsChanged();
    effect_libraries[ name ] = library;

    return true;
}

void EffectsHandlerImpl::unloadEffect(const QString& name)
{
    Workspace::self()->addRepaintFull();
    assert(current_paint_screen == 0);
    assert(current_paint_window == 0);
    assert(current_draw_window == 0);
    assert(current_build_quads == 0);
    assert(current_transform == 0);

    for (QMap< int, EffectPair >::iterator it = effect_order.begin(); it != effect_order.end(); ++it) {
        if (it.value().first == name) {
            kDebug(1212) << "EffectsHandler::unloadEffect : Unloading Effect : " << name;
            if (activeFullScreenEffect() == it.value().second) {
                setActiveFullScreenEffect(0);
            }
            delete it.value().second;
            effect_order.erase(it);
            effectsChanged();
            effect_libraries[ name ]->unload();
            return;
        }
    }

    kDebug(1212) << "EffectsHandler::unloadEffect : Effect not loaded : " << name;
}

void EffectsHandlerImpl::reconfigureEffect(const QString& name)
{
    for (QVector< EffectPair >::iterator it = loaded_effects.begin(); it != loaded_effects.end(); ++it)
        if ((*it).first == name) {
            (*it).second->reconfigure(Effect::ReconfigureAll);
            return;
        }
}

bool EffectsHandlerImpl::isEffectLoaded(const QString& name)
{
    for (QVector< EffectPair >::iterator it = loaded_effects.begin(); it != loaded_effects.end(); ++it)
        if ((*it).first == name)
            return true;

    return false;
}

void EffectsHandlerImpl::effectsChanged()
{
    loaded_effects.clear();
//    kDebug(1212) << "Recreating effects' list:";
    foreach (const EffectPair & effect, effect_order) {
//        kDebug(1212) << effect.first;
        loaded_effects.append(effect);
    }
}

EffectFrame* EffectsHandlerImpl::effectFrame(EffectFrameStyle style, bool staticSize, const QPoint& position, Qt::Alignment alignment) const
{
    return new EffectFrameImpl(style, staticSize, position, alignment);
}

//****************************************
// EffectWindowImpl
//****************************************

EffectWindowImpl::EffectWindowImpl() : EffectWindow()
    , toplevel(NULL)
    , sw(NULL)
{
}

EffectWindowImpl::~EffectWindowImpl()
{
    QVariant cachedTextureVariant = data(LanczosCacheRole);
    if (cachedTextureVariant.isValid()) {
        GLTexture *cachedTexture = static_cast< GLTexture*>(cachedTextureVariant.value<void*>());
        delete cachedTexture;
    }
}

bool EffectWindowImpl::isPaintingEnabled()
{
    return sceneWindow()->isPaintingEnabled();
}

void EffectWindowImpl::enablePainting(int reason)
{
    sceneWindow()->enablePainting(reason);
}

void EffectWindowImpl::disablePainting(int reason)
{
    sceneWindow()->disablePainting(reason);
}

void EffectWindowImpl::addRepaint(const QRect& r)
{
    toplevel->addRepaint(r);
}

void EffectWindowImpl::addRepaint(int x, int y, int w, int h)
{
    toplevel->addRepaint(x, y, w, h);
}

void EffectWindowImpl::addRepaintFull()
{
    toplevel->addRepaintFull();
}

int EffectWindowImpl::desktop() const
{
    return toplevel->desktop();
}

bool EffectWindowImpl::isOnAllDesktops() const
{
    return desktop() == NET::OnAllDesktops;
}

QString EffectWindowImpl::caption() const
{
    if (Client* c = dynamic_cast<Client*>(toplevel))
        return c->caption();
    else
        return "";
}

QString EffectWindowImpl::windowClass() const
{
    return toplevel->resourceName() + ' ' + toplevel->resourceClass();
}

QString EffectWindowImpl::windowRole() const
{
    return toplevel->windowRole();
}

QPixmap EffectWindowImpl::icon() const
{
    if (Client* c = dynamic_cast<Client*>(toplevel))
        return c->icon();
    return QPixmap(); // TODO
}

const EffectWindowGroup* EffectWindowImpl::group() const
{
    if (Client* c = dynamic_cast< Client* >(toplevel))
        return c->group()->effectGroup();
    return NULL; // TODO
}

bool EffectWindowImpl::isMinimized() const
{
    if (Client* c = dynamic_cast<Client*>(toplevel))
        return c->isMinimized();
    else
        return false;
}

double EffectWindowImpl::opacity() const
{
    return toplevel->opacity();
}

bool EffectWindowImpl::hasAlpha() const
{
    return toplevel->hasAlpha();
}


bool EffectWindowImpl::isDeleted() const
{
    return (dynamic_cast<Deleted*>(toplevel) != 0);
}

void EffectWindowImpl::refWindow()
{
    if (Deleted* d = dynamic_cast< Deleted* >(toplevel))
        return d->refWindow();
    abort(); // TODO
}

void EffectWindowImpl::unrefWindow()
{
    if (Deleted* d = dynamic_cast< Deleted* >(toplevel))
        return d->unrefWindow(true);   // delayed
    abort(); // TODO
}

void EffectWindowImpl::setWindow(Toplevel* w)
{
    toplevel = w;
}

void EffectWindowImpl::setSceneWindow(Scene::Window* w)
{
    sw = w;
}

int EffectWindowImpl::x() const
{
    return toplevel->x();
}

int EffectWindowImpl::y() const
{
    return toplevel->y();
}

int EffectWindowImpl::width() const
{
    return toplevel->width();
}

int EffectWindowImpl::height() const
{
    return toplevel->height();
}

QRect EffectWindowImpl::geometry() const
{
    return toplevel->geometry();
}

QRegion EffectWindowImpl::shape() const
{
    return sw ? sw->shape() : geometry();
}

int EffectWindowImpl::screen() const
{
    return toplevel->screen();
}

bool EffectWindowImpl::hasOwnShape() const
{
    return toplevel->shape();
}

QSize EffectWindowImpl::size() const
{
    return toplevel->size();
}

QPoint EffectWindowImpl::pos() const
{
    return toplevel->pos();
}

QRect EffectWindowImpl::rect() const
{
    return toplevel->rect();
}

QRect EffectWindowImpl::contentsRect() const
{
    return QRect(toplevel->clientPos(), toplevel->clientSize());
}

QRect EffectWindowImpl::decorationInnerRect() const
{
    Client *client = dynamic_cast<Client*>(toplevel);
    return client ? client->transparentRect() : contentsRect();
}

QByteArray EffectWindowImpl::readProperty(long atom, long type, int format) const
{
    return readWindowProperty(window()->window(), atom, type, format);
}

void EffectWindowImpl::deleteProperty(long int atom) const
{
    deleteWindowProperty(window()->window(), atom);
}

bool EffectWindowImpl::isMovable() const
{
    if (Client* c = dynamic_cast< Client* >(toplevel))
        return c->isMovable();
    return false;
}

bool EffectWindowImpl::isMovableAcrossScreens() const
{
    if (Client* c = dynamic_cast< Client* >(toplevel))
        return c->isMovableAcrossScreens();
    return false;
}

bool EffectWindowImpl::isUserMove() const
{
    if (Client* c = dynamic_cast< Client* >(toplevel))
        return c->isMove();
    return false;
}

bool EffectWindowImpl::isUserResize() const
{
    if (Client* c = dynamic_cast< Client* >(toplevel))
        return c->isResize();
    return false;
}

QRect EffectWindowImpl::iconGeometry() const
{
    if (Client* c = dynamic_cast< Client* >(toplevel))
        return c->iconGeometry();
    return QRect();
}

bool EffectWindowImpl::isDesktop() const
{
    return toplevel->isDesktop();
}

bool EffectWindowImpl::isDock() const
{
    return toplevel->isDock();
}

bool EffectWindowImpl::isToolbar() const
{
    return toplevel->isToolbar();
}

bool EffectWindowImpl::isTopMenu() const
{
    return toplevel->isTopMenu();
}

bool EffectWindowImpl::isMenu() const
{
    return toplevel->isMenu();
}

bool EffectWindowImpl::isNormalWindow() const
{
    return toplevel->isNormalWindow();
}

bool EffectWindowImpl::isSpecialWindow() const
{
    if (Client* c = dynamic_cast<Client*>(toplevel))
        return c->isSpecialWindow();
    else
        return true;
}

bool EffectWindowImpl::isDialog() const
{
    return toplevel->isDialog();
}

bool EffectWindowImpl::isSplash() const
{
    return toplevel->isSplash();
}

bool EffectWindowImpl::isUtility() const
{
    return toplevel->isUtility();
}

bool EffectWindowImpl::isDropdownMenu() const
{
    return toplevel->isDropdownMenu();
}

bool EffectWindowImpl::isPopupMenu() const
{
    return toplevel->isPopupMenu();
}

bool EffectWindowImpl::isTooltip() const
{
    return toplevel->isTooltip();
}

bool EffectWindowImpl::isNotification() const
{
    return toplevel->isNotification();
}

bool EffectWindowImpl::isComboBox() const
{
    return toplevel->isComboBox();
}

bool EffectWindowImpl::isDNDIcon() const
{
    return toplevel->isDNDIcon();
}

bool EffectWindowImpl::isManaged() const
{
    return dynamic_cast< const Client* >(toplevel) != NULL;
}

bool EffectWindowImpl::acceptsFocus() const
{
    const Client* client = dynamic_cast< const Client* >(toplevel);
    if (!client)
        return true; // We don't actually know...
    return client->wantsInput();
}

bool EffectWindowImpl::keepAbove() const
{
    const Client* client = dynamic_cast< const Client* >(toplevel);
    if (!client)
        return true;
    return client->keepAbove();
}

bool EffectWindowImpl::isModal() const
{
    if (Client* c = dynamic_cast< Client* >(toplevel))
        return c->isModal();
    return false;
}

EffectWindow* EffectWindowImpl::findModal()
{
    if (Client* c = dynamic_cast< Client* >(toplevel)) {
        if (Client* c2 = c->findModal())
            return c2->effectWindow();
    }
    return NULL;
}

EffectWindowList EffectWindowImpl::mainWindows() const
{
    if (Client* c = dynamic_cast< Client* >(toplevel)) {
        EffectWindowList ret;
        ClientList mainclients = c->mainClients();
        foreach (Client * tmp, mainclients)
        ret.append(tmp->effectWindow());
        return ret;
    }
    return EffectWindowList();
}

bool EffectWindowImpl::isSkipSwitcher() const
{
    if (Client* c = dynamic_cast< Client* >(toplevel))
        return c->skipSwitcher();
    return false;
}

WindowQuadList EffectWindowImpl::buildQuads(bool force) const
{
    return sceneWindow()->buildQuads(force);
}

void EffectWindowImpl::minimize() const
{
    if (Client* c = dynamic_cast< Client* >(toplevel)) {
        c->minimize();
    }
}

void EffectWindowImpl::unminimize() const
{
    if (Client* c = dynamic_cast< Client* >(toplevel)) {
        c->unminimize();
    }
}

void EffectWindowImpl::closeWindow() const
{
    if (Client* c = dynamic_cast< Client* >(toplevel)) {
        c->closeWindow();
    }
}

bool EffectWindowImpl::visibleInClientGroup() const
{
    if (Client* c = dynamic_cast< Client* >(toplevel)) {
        if (!c->clientGroup())
            return true;
        return c == c->clientGroup()->visible();
    }
    return false;
}

void EffectWindowImpl::setData(int role, const QVariant &data)
{
    if (!data.isNull())
        dataMap[ role ] = data;
    else
        dataMap.remove(role);
}

QVariant EffectWindowImpl::data(int role) const
{
    if (!dataMap.contains(role))
        return QVariant();
    return dataMap[ role ];
}

EffectWindow* effectWindow(Toplevel* w)
{
    EffectWindowImpl* ret = w->effectWindow();
    return ret;
}

EffectWindow* effectWindow(Scene::Window* w)
{
    EffectWindowImpl* ret = w->window()->effectWindow();
    ret->setSceneWindow(w);
    return ret;
}

//****************************************
// EffectWindowGroupImpl
//****************************************


EffectWindowList EffectWindowGroupImpl::members() const
{
    EffectWindowList ret;
    foreach (Toplevel * c, group->members())
    ret.append(c->effectWindow());
    return ret;
}

//****************************************
// EffectFrameImpl
//****************************************

EffectFrameImpl::EffectFrameImpl(EffectFrameStyle style, bool staticSize, QPoint position, Qt::Alignment alignment)
    : QObject(0)
    , EffectFrame()
    , m_style(style)
    , m_static(staticSize)
    , m_point(position)
    , m_alignment(alignment)
    , m_shader(NULL)
{
    if (m_style == EffectFrameStyled) {
        m_frame.setImagePath("widgets/background");
        m_frame.setCacheAllRenderedFrames(true);
        m_selection.setImagePath("widgets/viewitem");
        m_selection.setElementPrefix("hover");
        m_selection.setCacheAllRenderedFrames(true);
        m_selection.setEnabledBorders(Plasma::FrameSvg::AllBorders);
        connect(Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()), this, SLOT(plasmaThemeChanged()));
    }

    if (effects->compositingType() == OpenGLCompositing) {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
        m_sceneFrame = new SceneOpenGL::EffectFrame(this);
#endif
    } else if (effects->compositingType() == XRenderCompositing) {
        m_sceneFrame = new SceneXrender::EffectFrame(this);
    } else {
        // that should not happen and will definitely crash!
        m_sceneFrame = NULL;
    }
}

EffectFrameImpl::~EffectFrameImpl()
{
    delete m_sceneFrame;
}

const QFont& EffectFrameImpl::font() const
{
    return m_font;
}

void EffectFrameImpl::setFont(const QFont& font)
{
    if (m_font == font) {
        return;
    }
    m_font = font;
    QRect oldGeom = m_geometry;
    if (!m_text.isEmpty()) {
        autoResize();
    }
    if (oldGeom == m_geometry) {
        // Wasn't updated in autoResize()
        m_sceneFrame->freeTextFrame();
    }
}

void EffectFrameImpl::free()
{
    m_sceneFrame->free();
}

const QRect& EffectFrameImpl::geometry() const
{
    return m_geometry;
}

void EffectFrameImpl::setGeometry(const QRect& geometry, bool force)
{
    QRect oldGeom = m_geometry;
    m_geometry = geometry;
    if (m_geometry == oldGeom && !force) {
        return;
    }
    effects->addRepaint(oldGeom);
    effects->addRepaint(m_geometry);
    if (m_geometry.size() == oldGeom.size() && !force) {
        return;
    }

    if (m_style == EffectFrameStyled) {
        qreal left, top, right, bottom;
        m_frame.getMargins(left, top, right, bottom);   // m_geometry is the inner geometry
        m_frame.resizeFrame(m_geometry.adjusted(-left, -top, right, bottom).size());
    }

    free();
}

const QPixmap& EffectFrameImpl::icon() const
{
    return m_icon;
}

void EffectFrameImpl::setIcon(const QPixmap& icon)
{
    m_icon = icon;
    if (isCrossFade()) {
        m_sceneFrame->crossFadeIcon();
    }
    if (m_iconSize.isEmpty()) { // Set a size if we don't already have one
        setIconSize(m_icon.size());
    }
    m_sceneFrame->freeIconFrame();
}

const QSize& EffectFrameImpl::iconSize() const
{
    return m_iconSize;
}

void EffectFrameImpl::setIconSize(const QSize& size)
{
    if (m_iconSize == size) {
        return;
    }
    m_iconSize = size;
    autoResize();
    m_sceneFrame->freeIconFrame();
}

void EffectFrameImpl::plasmaThemeChanged()
{
    free();
}

void EffectFrameImpl::render(QRegion region, double opacity, double frameOpacity)
{
    if (m_geometry.isEmpty()) {
        return; // Nothing to display
    }
    m_shader = NULL;
    effects->paintEffectFrame(this, region, opacity, frameOpacity);
}

void EffectFrameImpl::finalRender(QRegion region, double opacity, double frameOpacity) const
{
    region = infiniteRegion(); // TODO: Old region doesn't seem to work with OpenGL

    m_sceneFrame->render(region, opacity, frameOpacity);
}

Qt::Alignment EffectFrameImpl::alignment() const
{
    return m_alignment;
}


void
EffectFrameImpl::align(QRect &geometry)
{
    if (m_alignment & Qt::AlignLeft)
        geometry.moveLeft(m_point.x());
    else if (m_alignment & Qt::AlignRight)
        geometry.moveLeft(m_point.x() - geometry.width());
    else
        geometry.moveLeft(m_point.x() - geometry.width() / 2);
    if (m_alignment & Qt::AlignTop)
        geometry.moveTop(m_point.y());
    else if (m_alignment & Qt::AlignBottom)
        geometry.moveTop(m_point.y() - geometry.height());
    else
        geometry.moveTop(m_point.y() - geometry.height() / 2);
}


void EffectFrameImpl::setAlignment(Qt::Alignment alignment)
{
    m_alignment = alignment;
    align(m_geometry);
    setGeometry(m_geometry);
}

void EffectFrameImpl::setPosition(const QPoint& point)
{
    m_point = point;
    QRect geometry = m_geometry; // this is important, setGeometry need call repaint for old & new geometry
    align(geometry);
    setGeometry(geometry);
}

const QString& EffectFrameImpl::text() const
{
    return m_text;
}

void EffectFrameImpl::setText(const QString& text)
{
    if (m_text == text) {
        return;
    }
    if (isCrossFade()) {
        m_sceneFrame->crossFadeText();
    }
    m_text = text;
    QRect oldGeom = m_geometry;
    autoResize();
    if (oldGeom == m_geometry) {
        // Wasn't updated in autoResize()
        m_sceneFrame->freeTextFrame();
    }
}

void EffectFrameImpl::setSelection(const QRect& selection)
{
    if (selection == m_selectionGeometry) {
        return;
    }
    m_selectionGeometry = selection;
    if (m_selectionGeometry.size() != m_selection.frameSize().toSize()) {
        m_selection.resizeFrame(m_selectionGeometry.size());
    }
    // TODO; optimize to only recreate when resizing
    m_sceneFrame->freeSelection();
}

void EffectFrameImpl::autoResize()
{
    if (m_static)
        return; // Not automatically resizing

    QRect geometry;
    // Set size
    if (!m_text.isEmpty()) {
        QFontMetrics metrics(m_font);
        geometry.setSize(metrics.size(0, m_text));
    }
    if (!m_icon.isNull() && !m_iconSize.isEmpty()) {
        geometry.setLeft(-m_iconSize.width());
        if (m_iconSize.height() > geometry.height())
            geometry.setHeight(m_iconSize.height());
    }

    align(geometry);
    setGeometry(geometry);
}

QColor EffectFrameImpl::styledTextColor()
{
    return Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor);
}

} // namespace
