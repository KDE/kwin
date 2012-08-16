/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

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
#ifdef KWIN_BUILD_TABBOX
#include "tabbox.h"
#endif
#include "scripting/scriptedeffect.h"
#include "thumbnailitem.h"
#include "workspace.h"
#include "kwinglutils.h"

#include <QFile>
#include <QtCore/QFutureWatcher>
#include <QtCore/QtConcurrentRun>

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
#include "composite.h"


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

EffectsHandlerImpl::EffectsHandlerImpl(Scene *scene)
    : EffectsHandler(scene->compositingType())
    , keyboard_grab_effect(NULL)
    , fullscreen_effect(0)
    , next_window_quad_type(EFFECT_QUAD_TYPE_START)
    , mouse_poll_ref_count(0)
    , m_scene(scene)
{
    // init is important, otherwise causes crashes when quads are build before the first painting pass start
    m_currentBuildQuadsIterator = m_activeEffects.end();

    Workspace *ws = Workspace::self();
    connect(ws, SIGNAL(currentDesktopChanged(int, KWin::Client*)), SLOT(slotDesktopChanged(int, KWin::Client*)));
    connect(ws, SIGNAL(clientAdded(KWin::Client*)), this, SLOT(slotClientAdded(KWin::Client*)));
    connect(ws, SIGNAL(unmanagedAdded(KWin::Unmanaged*)), this, SLOT(slotUnmanagedAdded(KWin::Unmanaged*)));
    connect(ws, SIGNAL(clientActivated(KWin::Client*)), this, SLOT(slotClientActivated(KWin::Client*)));
    connect(ws, SIGNAL(deletedRemoved(KWin::Deleted*)), this, SLOT(slotDeletedRemoved(KWin::Deleted*)));
    connect(ws, SIGNAL(numberDesktopsChanged(int)), SIGNAL(numberDesktopsChanged(int)));
    connect(ws, SIGNAL(mouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)),
            SIGNAL(mouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)));
    connect(ws, SIGNAL(propertyNotify(long)), this, SLOT(slotPropertyNotify(long)));
    connect(ws, SIGNAL(activityAdded(QString)), SIGNAL(activityAdded(QString)));
    connect(ws, SIGNAL(activityRemoved(QString)), SIGNAL(activityRemoved(QString)));
    connect(ws, SIGNAL(currentActivityChanged(QString)), SIGNAL(currentActivityChanged(QString)));
#ifdef KWIN_BUILD_TABBOX
    connect(ws->tabBox(), SIGNAL(tabBoxAdded(int)), SIGNAL(tabBoxAdded(int)));
    connect(ws->tabBox(), SIGNAL(tabBoxUpdated()), SIGNAL(tabBoxUpdated()));
    connect(ws->tabBox(), SIGNAL(tabBoxClosed()), SIGNAL(tabBoxClosed()));
    connect(ws->tabBox(), SIGNAL(tabBoxKeyEvent(QKeyEvent*)), SIGNAL(tabBoxKeyEvent(QKeyEvent*)));
#endif
    // connect all clients
    foreach (Client *c, ws->clientList()) {
        setupClientConnections(c);
    }
    foreach (Unmanaged *u, ws->unmanagedList()) {
        setupUnmanagedConnections(u);
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

void EffectsHandlerImpl::setupClientConnections(Client* c)
{
    connect(c, SIGNAL(windowClosed(KWin::Toplevel*,KWin::Deleted*)), this, SLOT(slotWindowClosed(KWin::Toplevel*)));
    connect(c, SIGNAL(clientMaximizedStateChanged(KWin::Client*,KDecorationDefines::MaximizeMode)), this, SLOT(slotClientMaximized(KWin::Client*,KDecorationDefines::MaximizeMode)));
    connect(c, SIGNAL(clientStartUserMovedResized(KWin::Client*)), this, SLOT(slotClientStartUserMovedResized(KWin::Client*)));
    connect(c, SIGNAL(clientStepUserMovedResized(KWin::Client*,QRect)), this, SLOT(slotClientStepUserMovedResized(KWin::Client*,QRect)));
    connect(c, SIGNAL(clientFinishUserMovedResized(KWin::Client*)), this, SLOT(slotClientFinishUserMovedResized(KWin::Client*)));
    connect(c, SIGNAL(opacityChanged(KWin::Toplevel*,qreal)), this, SLOT(slotOpacityChanged(KWin::Toplevel*,qreal)));
    connect(c, SIGNAL(clientMinimized(KWin::Client*,bool)), this, SLOT(slotClientMinimized(KWin::Client*,bool)));
    connect(c, SIGNAL(clientUnminimized(KWin::Client*,bool)), this, SLOT(slotClientUnminimized(KWin::Client*,bool)));
    connect(c, SIGNAL(geometryShapeChanged(KWin::Toplevel*,QRect)), this, SLOT(slotGeometryShapeChanged(KWin::Toplevel*,QRect)));
    connect(c, SIGNAL(paddingChanged(KWin::Toplevel*,QRect)), this, SLOT(slotPaddingChanged(KWin::Toplevel*,QRect)));
    connect(c, SIGNAL(damaged(KWin::Toplevel*,QRect)), this, SLOT(slotWindowDamaged(KWin::Toplevel*,QRect)));
    connect(c, SIGNAL(propertyNotify(KWin::Toplevel*,long)), this, SLOT(slotPropertyNotify(KWin::Toplevel*,long)));
}

void EffectsHandlerImpl::setupUnmanagedConnections(Unmanaged* u)
{
    connect(u, SIGNAL(windowClosed(KWin::Toplevel*,KWin::Deleted*)), this, SLOT(slotWindowClosed(KWin::Toplevel*)));
    connect(u, SIGNAL(opacityChanged(KWin::Toplevel*,qreal)), this, SLOT(slotOpacityChanged(KWin::Toplevel*,qreal)));
    connect(u, SIGNAL(geometryShapeChanged(KWin::Toplevel*,QRect)), this, SLOT(slotGeometryShapeChanged(KWin::Toplevel*,QRect)));
    connect(u, SIGNAL(paddingChanged(KWin::Toplevel*,QRect)), this, SLOT(slotPaddingChanged(KWin::Toplevel*,QRect)));
    connect(u, SIGNAL(damaged(KWin::Toplevel*,QRect)), this, SLOT(slotWindowDamaged(KWin::Toplevel*,QRect)));
    connect(u, SIGNAL(propertyNotify(KWin::Toplevel*,long)), this, SLOT(slotPropertyNotify(KWin::Toplevel*,long)));
}

void EffectsHandlerImpl::reconfigure()
{
    // perform querying for the services in a thread
    QFutureWatcher<KService::List> *watcher = new QFutureWatcher<KService::List>(this);
    connect(watcher, SIGNAL(finished()), this, SLOT(slotEffectsQueried()));
    watcher->setFuture(QtConcurrent::run(KServiceTypeTrader::self(), &KServiceTypeTrader::query, QString("KWin/Effect"), QString()));
}

void EffectsHandlerImpl::slotEffectsQueried()
{
    QFutureWatcher<KService::List> *watcher = dynamic_cast< QFutureWatcher<KService::List>* >(sender());
    if (!watcher) {
        // slot invoked not from a FutureWatcher
        return;
    }

    KService::List offers = watcher->result();
    QStringList effectsToBeLoaded;
    QStringList checkDefault;
    KSharedConfig::Ptr _config = KGlobal::config();
    KConfigGroup conf(_config, "Plugins");

    // First unload necessary effects
    foreach (const KService::Ptr & service, offers) {
        KPluginInfo plugininfo(service);
        plugininfo.load(conf);

        if (plugininfo.isPluginEnabledByDefault()) {
            const QString key = plugininfo.pluginName() + QString::fromLatin1("Enabled");
            if (!conf.hasKey(key))
                checkDefault.append(plugininfo.pluginName());
        }

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
            if (loadEffect(effectName, checkDefault.contains(effectName)))
                newLoaded.append(effectName);
        }
    }
    foreach (const EffectPair & ep, loaded_effects) {
        if (!newLoaded.contains(ep.first))    // don't reconfigure newly loaded effects
            ep.second->reconfigure(Effect::ReconfigureAll);
    }
    watcher->deleteLater();
}

// the idea is that effects call this function again which calls the next one
void EffectsHandlerImpl::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (m_currentPaintScreenIterator != m_activeEffects.end()) {
        (*m_currentPaintScreenIterator++)->prePaintScreen(data, time);
        --m_currentPaintScreenIterator;
    }
    // no special final code
}

void EffectsHandlerImpl::paintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    if (m_currentPaintScreenIterator != m_activeEffects.end()) {
        (*m_currentPaintScreenIterator++)->paintScreen(mask, region, data);
        --m_currentPaintScreenIterator;
    } else
        m_scene->finalPaintScreen(mask, region, data);
}

void EffectsHandlerImpl::postPaintScreen()
{
    if (m_currentPaintScreenIterator != m_activeEffects.end()) {
        (*m_currentPaintScreenIterator++)->postPaintScreen();
        --m_currentPaintScreenIterator;
    }
    // no special final code
}

void EffectsHandlerImpl::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    if (m_currentPaintWindowIterator != m_activeEffects.end()) {
        (*m_currentPaintWindowIterator++)->prePaintWindow(w, data, time);
        --m_currentPaintWindowIterator;
    }
    // no special final code
}

void EffectsHandlerImpl::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (m_currentPaintWindowIterator != m_activeEffects.end()) {
        (*m_currentPaintWindowIterator++)->paintWindow(w, mask, region, data);
        --m_currentPaintWindowIterator;
    } else
        m_scene->finalPaintWindow(static_cast<EffectWindowImpl*>(w), mask, region, data);
}

void EffectsHandlerImpl::paintEffectFrame(EffectFrame* frame, QRegion region, double opacity, double frameOpacity)
{
    if (m_currentPaintEffectFrameIterator != m_activeEffects.end()) {
        (*m_currentPaintEffectFrameIterator++)->paintEffectFrame(frame, region, opacity, frameOpacity);
        --m_currentPaintEffectFrameIterator;
    } else {
        const EffectFrameImpl* frameImpl = static_cast<const EffectFrameImpl*>(frame);
        frameImpl->finalRender(region, opacity, frameOpacity);
    }
}

void EffectsHandlerImpl::postPaintWindow(EffectWindow* w)
{
    if (m_currentPaintWindowIterator != m_activeEffects.end()) {
        (*m_currentPaintWindowIterator++)->postPaintWindow(w);
        --m_currentPaintWindowIterator;
    }
    // no special final code
}

Effect *EffectsHandlerImpl::provides(Effect::Feature ef)
{
    for (int i = 0; i < loaded_effects.size(); ++i)
        if (loaded_effects.at(i).second->provides(ef))
            return loaded_effects.at(i).second;
    return NULL;
}

void EffectsHandlerImpl::drawWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (m_currentDrawWindowIterator != m_activeEffects.end()) {
        (*m_currentDrawWindowIterator++)->drawWindow(w, mask, region, data);
        --m_currentDrawWindowIterator;
    } else
        m_scene->finalDrawWindow(static_cast<EffectWindowImpl*>(w), mask, region, data);
}

void EffectsHandlerImpl::buildQuads(EffectWindow* w, WindowQuadList& quadList)
{
    static bool initIterator = true;
    if (initIterator) {
        m_currentBuildQuadsIterator = m_activeEffects.begin();
        initIterator = false;
    }
    if (m_currentBuildQuadsIterator != m_activeEffects.end()) {
        (*m_currentBuildQuadsIterator++)->buildQuads(w, quadList);
        --m_currentBuildQuadsIterator;
    }
    if (m_currentBuildQuadsIterator == m_activeEffects.begin())
        initIterator = true;
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
    m_activeEffects.clear();
    for(QVector< KWin::EffectPair >::iterator it = loaded_effects.begin(); it != loaded_effects.end(); ++it) {
        if (it->second->isActive()) {
            m_activeEffects << it->second;
        }
    }
    m_currentDrawWindowIterator = m_activeEffects.begin();
    m_currentPaintWindowIterator = m_activeEffects.begin();
    m_currentPaintScreenIterator = m_activeEffects.begin();
    m_currentPaintEffectFrameIterator = m_activeEffects.begin();
}

void EffectsHandlerImpl::slotClientMaximized(KWin::Client *c, KDecorationDefines::MaximizeMode maxMode)
{
    bool horizontal = false;
    bool vertical = false;
    switch (maxMode) {
    case KDecorationDefines::MaximizeHorizontal:
        horizontal = true;
        break;
    case KDecorationDefines::MaximizeVertical:
        vertical = true;
        break;
    case KDecorationDefines::MaximizeFull:
        horizontal = true;
        vertical = true;
        break;
    case KDecorationDefines::MaximizeRestore: // fall through
    default:
        // default - nothing to do
        break;
    }
    emit windowMaximizedStateChanged(c->effectWindow(), horizontal, vertical);
}

void EffectsHandlerImpl::slotClientStartUserMovedResized(Client *c)
{
    emit windowStartUserMovedResized(c->effectWindow());
}

void EffectsHandlerImpl::slotClientFinishUserMovedResized(Client *c)
{
    emit windowFinishUserMovedResized(c->effectWindow());
}

void EffectsHandlerImpl::slotClientStepUserMovedResized(Client* c, const QRect& geometry)
{
    emit windowStepUserMovedResized(c->effectWindow(), geometry);
}

void EffectsHandlerImpl::slotOpacityChanged(Toplevel *t, qreal oldOpacity)
{
    if (t->opacity() == oldOpacity || !t->effectWindow()) {
        return;
    }
    emit windowOpacityChanged(t->effectWindow(), oldOpacity, (qreal)t->opacity());
}

void EffectsHandlerImpl::slotClientAdded(Client *c)
{
    if (c->readyForPainting())
        slotClientShown(c);
    else
        connect(c, SIGNAL(windowShown(KWin::Toplevel*)), SLOT(slotClientShown(KWin::Toplevel*)));
}

void EffectsHandlerImpl::slotUnmanagedAdded(Unmanaged *u)
{   // regardless, unmanaged windows are -yet?- not synced anyway
    setupUnmanagedConnections(u);
    emit windowAdded(u->effectWindow());
}

void EffectsHandlerImpl::slotClientShown(KWin::Toplevel *t)
{
    Q_ASSERT(dynamic_cast<Client*>(t));
    Client *c = static_cast<Client*>(t);
    setupClientConnections(c);
    if (!c->tabGroup()) // the "window" has already been there
        emit windowAdded(c->effectWindow());
}

void EffectsHandlerImpl::slotDeletedRemoved(KWin::Deleted *d)
{
    emit windowDeleted(d->effectWindow());
    elevated_windows.removeAll(d->effectWindow());
}

void EffectsHandlerImpl::slotWindowClosed(KWin::Toplevel *c)
{
    emit windowClosed(c->effectWindow());
}

void EffectsHandlerImpl::slotClientActivated(KWin::Client *c)
{
    emit windowActivated(c ? c->effectWindow() : NULL);
}

void EffectsHandlerImpl::slotClientMinimized(Client *c, bool animate)
{
    // TODO: notify effects even if it should not animate?
    if (animate) {
        emit windowMinimized(c->effectWindow());
    }
}

void EffectsHandlerImpl::slotClientUnminimized(Client* c, bool animate)
{
    // TODO: notify effects even if it should not animate?
    if (animate) {
        emit windowUnminimized(c->effectWindow());
    }
}

void EffectsHandlerImpl::slotCurrentTabAboutToChange(EffectWindow *from, EffectWindow *to)
{
    emit currentTabAboutToChange(from, to);
}

void EffectsHandlerImpl::slotTabAdded(EffectWindow* w, EffectWindow* to)
{
    emit tabAdded(w, to);
}

void EffectsHandlerImpl::slotTabRemoved(EffectWindow *w, EffectWindow* leaderOfFormerGroup)
{
    emit tabRemoved(w, leaderOfFormerGroup);
}

void EffectsHandlerImpl::slotDesktopChanged(int old, Client *c)
{
    const int newDesktop = Workspace::self()->currentDesktop();
    if (old != 0 && newDesktop != old) {
        emit desktopChanged(old, newDesktop, c ? c->effectWindow() : 0);
        // TODO: remove in 4.10
        emit desktopChanged(old, newDesktop);
    }
}

void EffectsHandlerImpl::slotWindowDamaged(Toplevel* t, const QRect& r)
{
    if (!t->effectWindow()) {
        // can happen during tear down of window
        return;
    }
    emit windowDamaged(t->effectWindow(), r);
}

void EffectsHandlerImpl::slotGeometryShapeChanged(Toplevel* t, const QRect& old)
{
    // during late cleanup effectWindow() may be already NULL
    // in some functions that may still call this
    if (t == NULL || t->effectWindow() == NULL)
        return;
    emit windowGeometryShapeChanged(t->effectWindow(), old);
}

void EffectsHandlerImpl::slotPaddingChanged(Toplevel* t, const QRect& old)
{
    // during late cleanup effectWindow() may be already NULL
    // in some functions that may still call this
    if (t == NULL || t->effectWindow() == NULL)
        return;
    emit windowPaddingChanged(t->effectWindow(), old);
}

void EffectsHandlerImpl::setActiveFullScreenEffect(Effect* e)
{
    fullscreen_effect = e;
    Workspace::self()->compositor()->checkUnredirect();
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
        Workspace::self()->compositor()->startMousePolling();
    mouse_poll_ref_count++;
}

void EffectsHandlerImpl::stopMousePolling()
{
    assert(mouse_poll_ref_count);
    mouse_poll_ref_count--;
    if (!mouse_poll_ref_count)   // Stop timer if required
        Workspace::self()->compositor()->stopMousePolling();
}

bool EffectsHandlerImpl::hasKeyboardGrab() const
{
    return keyboard_grab_effect != NULL;
}

void EffectsHandlerImpl::desktopResized(const QSize &size)
{
    m_scene->screenGeometryChanged(size);
    emit screenGeometryChanged(size);
    Workspace::self()->compositor()->addRepaintFull();
}

void EffectsHandlerImpl::slotPropertyNotify(Toplevel* t, long int atom)
{
    if (!registered_atoms.contains(atom))
        return;
    emit propertyNotify(t->effectWindow(), atom);
}

void EffectsHandlerImpl::slotPropertyNotify(long int atom)
{
    if (!registered_atoms.contains(atom))
        return;
    emit propertyNotify(NULL, atom);
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
    if (cl && !cl->isDesktop() && !cl->isDock())
        Workspace::self()->sendClientToDesktop(cl, desktop, true);
}

void EffectsHandlerImpl::windowToScreen(EffectWindow* w, int screen)
{
    Client* cl = dynamic_cast< Client* >(static_cast<EffectWindowImpl*>(w)->window());
    if (cl && !cl->isDesktop() && !cl->isDock())
        Workspace::self()->sendClientToScreen(cl, screen);
}

void EffectsHandlerImpl::setShowingDesktop(bool showing)
{
    Workspace::self()->setShowingDesktop(showing);
}

QString EffectsHandlerImpl::currentActivity() const
{
    return Workspace::self()->currentActivity();
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

QString EffectsHandlerImpl::desktopName(int desktop) const
{
    return Workspace::self()->desktopName(desktop);
}

bool EffectsHandlerImpl::optionRollOverDesktops() const
{
    return options->isRollOverDesktops();
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
    ToplevelList list = Workspace::self()->xStackingOrder();
    EffectWindowList ret;
    foreach (Toplevel *w, list)
        ret.append(effectWindow(w));
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
#ifdef KWIN_BUILD_TABBOX
    if (Client* c = dynamic_cast< Client* >(static_cast< EffectWindowImpl* >(w)->window())) {

        if (Workspace::self()->hasTabBox()) {
            Workspace::self()->tabBox()->setCurrentClient(c);
        }
    }
#else
    Q_UNUSED(w)
#endif
}

void EffectsHandlerImpl::setTabBoxDesktop(int desktop)
{
#ifdef KWIN_BUILD_TABBOX
    if (Workspace::self()->hasTabBox()) {
        Workspace::self()->tabBox()->setCurrentDesktop(desktop);
    }
#else
    Q_UNUSED(desktop)
#endif
}

EffectWindowList EffectsHandlerImpl::currentTabBoxWindowList() const
{
#ifdef KWIN_BUILD_TABBOX
    EffectWindowList ret;
    ClientList clients;
    if (Workspace::self()->hasTabBox()) {
        clients = Workspace::self()->tabBox()->currentClientList();
    } else {
        clients = ClientList();
    }
    foreach (Client * c, clients)
    ret.append(c->effectWindow());
    return ret;
#else
    return EffectWindowList();
#endif
}

void EffectsHandlerImpl::refTabBox()
{
#ifdef KWIN_BUILD_TABBOX
    if (Workspace::self()->hasTabBox()) {
        Workspace::self()->tabBox()->reference();
    }
#endif
}

void EffectsHandlerImpl::unrefTabBox()
{
#ifdef KWIN_BUILD_TABBOX
    if (Workspace::self()->hasTabBox()) {
        Workspace::self()->tabBox()->unreference();
    }
#endif
}

void EffectsHandlerImpl::closeTabBox()
{
#ifdef KWIN_BUILD_TABBOX
    if (Workspace::self()->hasTabBox()) {
        Workspace::self()->tabBox()->close();
    }
#endif
}

QList< int > EffectsHandlerImpl::currentTabBoxDesktopList() const
{
#ifdef KWIN_BUILD_TABBOX
    if (Workspace::self()->hasTabBox()) {
        return Workspace::self()->tabBox()->currentDesktopList();
    }
#endif
    return QList< int >();
}

int EffectsHandlerImpl::currentTabBoxDesktop() const
{
#ifdef KWIN_BUILD_TABBOX
    if (Workspace::self()->hasTabBox()) {
        return Workspace::self()->tabBox()->currentDesktop();
    }
#endif
    return -1;
}

EffectWindow* EffectsHandlerImpl::currentTabBoxWindow() const
{
#ifdef KWIN_BUILD_TABBOX
    if (Workspace::self()->hasTabBox()) {
        if (Client* c = Workspace::self()->tabBox()->currentClient())
        return c->effectWindow();
    }
#endif
    return NULL;
}

void EffectsHandlerImpl::addRepaintFull()
{
    Workspace::self()->compositor()->addRepaintFull();
}

void EffectsHandlerImpl::addRepaint(const QRect& r)
{
    Workspace::self()->compositor()->addRepaint(r);
}

void EffectsHandlerImpl::addRepaint(const QRegion& r)
{
    Workspace::self()->compositor()->addRepaint(r);
}

void EffectsHandlerImpl::addRepaint(int x, int y, int w, int h)
{
    Workspace::self()->compositor()->addRepaint(x, y, w, h);
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
    Window win = 0;
    QList<InputWindowPair>::iterator it = input_windows.begin();
    while (it != input_windows.end()) {
        if (it->first != e) {
            ++it;
            continue;
        }
        XWindowAttributes attr;
        if (!XGetWindowAttributes(display(), it->second, &attr)) {
            // this is some random junk that certainly should no be here
            kDebug(1212) << "found input window that is NOT on the server, something is VERY broken here";
            Q_ASSERT(false); // exit in debug mode - for releases we'll be a bit more graceful
            it = input_windows.erase(it);
            continue;
        }
        if (attr.x == x && attr.y == y && attr.width == w && attr.height == h) {
            win = it->second; // re-use
            break;
        } else if (attr.map_state == IsUnmapped) {
            // probably old one, likely no longer of interest
            XDestroyWindow(display(), it->second);
            it = input_windows.erase(it);
            continue;
        }
        ++it;
    }
    if (!win) {
        XSetWindowAttributes attrs;
        attrs.override_redirect = True;
        win = XCreateWindow(display(), rootWindow(), x, y, w, h, 0, 0, InputOnly, CopyFromParent,
                                CWOverrideRedirect, &attrs);
        // TODO keeping on top?
        // TODO enter/leave notify?
        XSelectInput(display(), win, ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
        XDefineCursor(display(), win, cursor.handle());
        input_windows.append(qMakePair(e, win));
    }
    XMapRaised(display(), win);
    // Raise electric border windows above the input windows
    // so they can still be triggered.
#ifdef KWIN_BUILD_SCREENEDGES
    Workspace::self()->screenEdge()->ensureOnTop();
#endif
    if (input_windows.count() > 10) // that sounds like some leak - could still be correct, thoug - so NO ABORT HERE!
        kDebug() << "** warning ** there are now " << input_windows.count() <<
                    "input windows what's a bit much - please have a look and if this counts up, better report a bug";
    return win;
}

void EffectsHandlerImpl::destroyInputWindow(Window w)
{
    foreach (const InputWindowPair & pos, input_windows) {
        if (pos.second == w) {
            XUnmapWindow(display(), w);
#ifdef KWIN_BUILD_SCREENEDGES
            Workspace::self()->screenEdge()->raisePanelProxies();
#endif
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
    Window* wins = new Window[input_windows.count()];
    int pos = 0;
    foreach (const InputWindowPair &it, input_windows) {
        XWindowAttributes attr;
        if (XGetWindowAttributes(display(), it.second, &attr) && attr.map_state != IsUnmapped)
            wins[pos++] = it.second;
    }
    if (pos) {
        XRaiseWindow(display(), wins[0]);
        XRestackWindows(display(), wins, pos);
    }
    delete[] wins;
    // Raise electric border windows above the input windows
    // so they can still be triggered. TODO: Do both at once.
#ifdef KWIN_BUILD_SCREENEDGES
    if (pos)
        Workspace::self()->screenEdge()->ensureOnTop();
#endif
}

QPoint EffectsHandlerImpl::cursorPos() const
{
    return Workspace::self()->cursorPos();
}

void EffectsHandlerImpl::checkElectricBorder(const QPoint &pos, Time time)
{
#ifdef KWIN_BUILD_SCREENEDGES
    Workspace::self()->screenEdge()->check(pos, time);
#else
    Q_UNUSED(pos)
    Q_UNUSED(time)
#endif
}

void EffectsHandlerImpl::reserveElectricBorder(ElectricBorder border)
{
#ifdef KWIN_BUILD_SCREENEDGES
    Workspace::self()->screenEdge()->reserve(border);
#else
    Q_UNUSED(border)
#endif
}

void EffectsHandlerImpl::unreserveElectricBorder(ElectricBorder border)
{
#ifdef KWIN_BUILD_SCREENEDGES
    Workspace::self()->screenEdge()->unreserve(border);
#else
    Q_UNUSED(border)
#endif
}

void EffectsHandlerImpl::reserveElectricBorderSwitching(bool reserve, Qt::Orientations o)
{
#ifdef KWIN_BUILD_SCREENEDGES
    Workspace::self()->screenEdge()->reserveDesktopSwitching(reserve, o);
#else
    Q_UNUSED(reserve)
#endif
}

unsigned long EffectsHandlerImpl::xrenderBufferPicture()
{
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (SceneXrender* s = dynamic_cast< SceneXrender* >(m_scene))
        return s->bufferPicture();
#endif
    return None;
}

KLibrary* EffectsHandlerImpl::findEffectLibrary(KService* service)
{
    QString libname = service->library();
#ifdef KWIN_HAVE_OPENGLES
    if (libname.startsWith(QLatin1String("kwin4_effect_"))) {
        libname.replace("kwin4_effect_", "kwin4_effect_gles_");
    }
#endif
    libname.replace("kwin", KWIN_NAME);
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

bool EffectsHandlerImpl::loadEffect(const QString& name, bool checkDefault)
{
    Workspace::self()->compositor()->addRepaintFull();

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

    if (service->property("X-Plasma-API").toString() == "javascript") {
        // this is a scripted effect - use different loader
        return loadScriptedEffect(name, service.data());
    }

    KLibrary* library = findEffectLibrary(service.data());
    if (!library) {
        return false;
    }

    QString version_symbol = "effect_version_" + name;
    KLibrary::void_function_ptr version_func = library->resolveFunction(version_symbol.toAscii());
    if (version_func == NULL) {
        kWarning(1212) << "Effect " << name << " does not provide required API version, ignoring.";
	delete library;
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
        delete library;
        return false;
    }

    const QString enabledByDefault_symbol = "effect_enabledbydefault_" + name;
    KLibrary::void_function_ptr enabledByDefault_func = library->resolveFunction(enabledByDefault_symbol.toAscii().data());

    const QString supported_symbol = "effect_supported_" + name;
    KLibrary::void_function_ptr supported_func = library->resolveFunction(supported_symbol.toAscii().data());

    const QString create_symbol = "effect_create_" + name;
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

    if (checkDefault && enabledByDefault_func) {
        typedef bool (*t_enabledByDefaultfunc)();
        t_enabledByDefaultfunc enabledByDefault = reinterpret_cast<t_enabledByDefaultfunc>(enabledByDefault_func);

        if (!enabledByDefault()) {
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

bool EffectsHandlerImpl::loadScriptedEffect(const QString& name, KService *service)
{
    const KDesktopFile df("services", service->entryPath());
    const QString scriptName = df.desktopGroup().readEntry<QString>("X-Plasma-MainScript", "");
    if (scriptName.isEmpty()) {
        kDebug(1212) << "X-Plasma-MainScript not set";
        return false;
    }
    const QString scriptFile = KStandardDirs::locate("data", QLatin1String(KWIN_NAME) + "/effects/" + name + "/contents/" + scriptName);
    if (scriptFile.isNull()) {
        kDebug(1212) << "Could not locate the effect script";
        return false;
    }
    ScriptedEffect *effect = ScriptedEffect::create(name, scriptFile);
    if (!effect) {
        kDebug(1212) << "Could not initialize scripted effect: " << name;
        return false;
    }
    effect_order.insert(service->property("X-KDE-Ordering").toInt(), EffectPair(name, effect));
    effectsChanged();
    return true;
}

void EffectsHandlerImpl::unloadEffect(const QString& name)
{
    Workspace::self()->compositor()->addRepaintFull();

    for (QMap< int, EffectPair >::iterator it = effect_order.begin(); it != effect_order.end(); ++it) {
        if (it.value().first == name) {
            kDebug(1212) << "EffectsHandler::unloadEffect : Unloading Effect : " << name;
            if (activeFullScreenEffect() == it.value().second) {
                setActiveFullScreenEffect(0);
            }
            delete it.value().second;
            effect_order.erase(it);
            effectsChanged();
            if (effect_libraries.contains(name)) {
                effect_libraries[ name ]->unload();
            }
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

bool EffectsHandlerImpl::isEffectLoaded(const QString& name) const
{
    for (QVector< EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it)
        if ((*it).first == name)
            return true;

    return false;
}

void EffectsHandlerImpl::reloadEffect(Effect *effect)
{
    QString effectName;
    for (QVector< EffectPair >::iterator it = loaded_effects.begin(); it != loaded_effects.end(); ++it) {
        if ((*it).second == effect) {
            effectName = (*it).first;
            break;
        }
    }
    if (!effectName.isNull()) {
        unloadEffect(effectName);
        loadEffect(effectName);
    }
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

QStringList EffectsHandlerImpl::activeEffects() const
{
    QStringList ret;
    for(QVector< KWin::EffectPair >::const_iterator it = loaded_effects.constBegin(),
                                                    end = loaded_effects.constEnd(); it != end; ++it) {
            if (it->second->isActive()) {
                ret << it->first;
            }
        }
    return ret;
}

EffectFrame* EffectsHandlerImpl::effectFrame(EffectFrameStyle style, bool staticSize, const QPoint& position, Qt::Alignment alignment) const
{
    return new EffectFrameImpl(style, staticSize, position, alignment);
}


QVariant EffectsHandlerImpl::kwinOption(KWinOption kwopt)
{
    switch (kwopt) {
    case CloseButtonCorner:
        return Workspace::self()->decorationCloseButtonCorner();
    }
    return QVariant(); // an invalid one
}

void EffectsHandlerImpl::slotShowOutline(const QRect& geometry)
{
    emit showOutline(geometry);
}

void EffectsHandlerImpl::slotHideOutline()
{
    emit hideOutline();
}

QString EffectsHandlerImpl::supportInformation(const QString &name) const
{
    if (!isEffectLoaded(name)) {
        return QString();
    }
    for (QVector< EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if ((*it).first == name) {
            QString support((*it).first + ":\n");
            const QMetaObject *metaOptions = (*it).second->metaObject();
            for (int i=0; i<metaOptions->propertyCount(); ++i) {
                const QMetaProperty property = metaOptions->property(i);
                if (QLatin1String(property.name()) == "objectName") {
                    continue;
                }
                support.append(QLatin1String(property.name()) % ": " % (*it).second->property(property.name()).toString() % '\n');
            }
            return support;
        }
    }
    return QString();
}

//****************************************
// EffectWindowImpl
//****************************************

EffectWindowImpl::EffectWindowImpl(Toplevel *toplevel)
    : EffectWindow(toplevel)
    , toplevel(toplevel)
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

const EffectWindowGroup* EffectWindowImpl::group() const
{
    if (Client* c = dynamic_cast< Client* >(toplevel))
        return c->group()->effectGroup();
    return NULL; // TODO
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
    setParent(w);
}

void EffectWindowImpl::setSceneWindow(Scene::Window* w)
{
    sw = w;
}

QRegion EffectWindowImpl::shape() const
{
    return sw ? sw->shape() : geometry();
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

WindowQuadList EffectWindowImpl::buildQuads(bool force) const
{
    return sceneWindow()->buildQuads(force);
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

void EffectWindowImpl::registerThumbnail(ThumbnailItem *item)
{
    insertThumbnail(item);
    connect(item, SIGNAL(destroyed(QObject*)), SLOT(thumbnailDestroyed(QObject*)));
    connect(item, SIGNAL(wIdChanged(qulonglong)), SLOT(thumbnailTargetChanged()));
}

void EffectWindowImpl::thumbnailDestroyed(QObject *object)
{
    // we know it is a ThumbnailItem
    m_thumbnails.remove(static_cast<ThumbnailItem*>(object));
}

void EffectWindowImpl::thumbnailTargetChanged()
{
    if (ThumbnailItem *item = qobject_cast<ThumbnailItem*>(sender())) {
        insertThumbnail(item);
    }
}

void EffectWindowImpl::insertThumbnail(ThumbnailItem *item)
{
    EffectWindow *w = effects->findWindow(item->wId());
    if (w) {
        m_thumbnails.insert(item, QWeakPointer<EffectWindowImpl>(static_cast<EffectWindowImpl*>(w)));
    } else {
        m_thumbnails.insert(item, QWeakPointer<EffectWindowImpl>());
    }
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
        connect(Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()), this, SLOT(plasmaThemeChanged()));
    }
    m_selection.setImagePath("widgets/viewitem");
    m_selection.setElementPrefix("hover");
    m_selection.setCacheAllRenderedFrames(true);
    m_selection.setEnabledBorders(Plasma::FrameSvg::AllBorders);

    if (effects->compositingType() == OpenGLCompositing) {
        m_sceneFrame = new SceneOpenGL::EffectFrame(this);
    } else if (effects->compositingType() == XRenderCompositing) {
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
        m_sceneFrame = new SceneXrender::EffectFrame(this);
#endif
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
