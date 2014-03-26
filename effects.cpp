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

#include "effectsadaptor.h"
#ifdef KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#include "decorations.h"
#include "deleted.h"
#include "client.h"
#include "cursor.h"
#include "group.h"
#include "scene_xrender.h"
#include "scene_qpainter.h"
#include "unmanaged.h"
#ifdef KWIN_BUILD_TABBOX
#include "tabbox.h"
#endif
#ifdef KWIN_BUILD_SCREENEDGES
#include "screenedge.h"
#endif
#include "scripting/scriptedeffect.h"
#include "screens.h"
#include "thumbnailitem.h"
#include "virtualdesktops.h"
#include "workspace.h"
#include "kwinglutils.h"
#include "effects/effect_builtins.h"

#include <QDebug>
#include <QFile>
#include <QFutureWatcher>
#include <QtConcurrentRun>
#include <QDBusServiceWatcher>
#include <QDBusPendingCallWatcher>
#include <QStandardPaths>

#include <KLibrary>
#include <KDesktopFile>
#include <KConfigGroup>
#include <KService>
#include <KServiceTypeTrader>
#include <KPluginInfo>
#include <Plasma/Theme>

#include <assert.h>
#include "composite.h"
#include "xcbutils.h"
#if HAVE_WAYLAND
#include "wayland_backend.h"
#endif

// dbus generated
#include "screenlocker_interface.h"


namespace KWin
{

static const QString SCREEN_LOCKER_SERVICE_NAME = QStringLiteral("org.freedesktop.ScreenSaver");

ScreenLockerWatcher::ScreenLockerWatcher(QObject *parent)
    : QObject(parent)
    , m_interface(NULL)
    , m_serviceWatcher(new QDBusServiceWatcher(this))
    , m_locked(false)
{
    connect(m_serviceWatcher, SIGNAL(serviceOwnerChanged(QString,QString,QString)), SLOT(serviceOwnerChanged(QString,QString,QString)));
    m_serviceWatcher->setWatchMode(QDBusServiceWatcher::WatchForOwnerChange);
    m_serviceWatcher->addWatchedService(SCREEN_LOCKER_SERVICE_NAME);
    // check whether service is registered
    QFutureWatcher<QDBusReply<bool> > *watcher = new QFutureWatcher<QDBusReply<bool> >(this);
    connect(watcher, SIGNAL(finished()), SLOT(serviceRegisteredQueried()));
    connect(watcher, SIGNAL(canceled()), watcher, SLOT(deleteLater()));
    watcher->setFuture(QtConcurrent::run(QDBusConnection::sessionBus().interface(),
                                         &QDBusConnectionInterface::isServiceRegistered,
                                         SCREEN_LOCKER_SERVICE_NAME));
}

ScreenLockerWatcher::~ScreenLockerWatcher()
{
}

void ScreenLockerWatcher::serviceOwnerChanged(const QString &serviceName, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    if (serviceName != SCREEN_LOCKER_SERVICE_NAME) {
        return;
    }
    delete m_interface;
    m_interface = NULL;
    m_locked = false;
    if (!newOwner.isEmpty()) {
        m_interface = new OrgFreedesktopScreenSaverInterface(newOwner, QString(), QDBusConnection::sessionBus(), this);
        connect(m_interface, SIGNAL(ActiveChanged(bool)), SLOT(setLocked(bool)));
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(m_interface->GetActive(), this);
        connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), SLOT(activeQueried(QDBusPendingCallWatcher*)));
    }
}

void ScreenLockerWatcher::serviceRegisteredQueried()
{
    QFutureWatcher<QDBusReply<bool> > *watcher = dynamic_cast<QFutureWatcher<QDBusReply<bool> > *>(sender());
    if (!watcher) {
        return;
    }
    const QDBusReply<bool> &reply = watcher->result();
    if (reply.isValid() && reply.value()) {
        QFutureWatcher<QDBusReply<QString> > *ownerWatcher = new QFutureWatcher<QDBusReply<QString> >(this);
        connect(ownerWatcher, SIGNAL(finished()), SLOT(serviceOwnerQueried()));
        connect(ownerWatcher, SIGNAL(canceled()), ownerWatcher, SLOT(deleteLater()));
        ownerWatcher->setFuture(QtConcurrent::run(QDBusConnection::sessionBus().interface(),
                                                  &QDBusConnectionInterface::serviceOwner,
                                                  SCREEN_LOCKER_SERVICE_NAME));
    }
    watcher->deleteLater();
}

void ScreenLockerWatcher::serviceOwnerQueried()
{
    QFutureWatcher<QDBusReply<QString> > *watcher = dynamic_cast<QFutureWatcher<QDBusReply<QString> > *>(sender());
    if (!watcher) {
        return;
    }
    const QDBusReply<QString> reply = watcher->result();
    if (reply.isValid()) {
        serviceOwnerChanged(SCREEN_LOCKER_SERVICE_NAME, QString(), reply.value());
    }

    watcher->deleteLater();
}

void ScreenLockerWatcher::activeQueried(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<bool> reply = *watcher;
    if (!reply.isError()) {
        setLocked(reply.value());
    }
    watcher->deleteLater();
}

void ScreenLockerWatcher::setLocked(bool activated)
{
    if (m_locked == activated) {
        return;
    }
    m_locked = activated;
    emit locked(m_locked);
}

EffectLoader::EffectLoader(EffectsHandlerImpl *parent)
    : QObject(parent)
    , m_effects(parent)
    , m_dequeueScheduled(false)
{
}

EffectLoader::~EffectLoader()
{
}

void EffectLoader::queue(const QString &effect, bool checkDefault)
{
    m_queue.enqueue(qMakePair(effect, checkDefault));
    scheduleDequeue();
}

void EffectLoader::dequeue()
{
    Q_ASSERT(!m_queue.isEmpty());
    m_dequeueScheduled = false;
    const auto pair = m_queue.dequeue();
    m_effects->loadEffect(pair.first, pair.second);
    scheduleDequeue();
}

void EffectLoader::scheduleDequeue()
{
    if (m_queue.isEmpty() || m_dequeueScheduled) {
        return;
    }
    m_dequeueScheduled = true;
    QMetaObject::invokeMethod(this, "dequeue", Qt::QueuedConnection);
}

//---------------------
// Static

static QByteArray readWindowProperty(xcb_window_t win, xcb_atom_t atom, xcb_atom_t type, int format)
{
    uint32_t len = 32768;
    xcb_connection_t *c = connection();
    for (;;) {
        const auto cookie = xcb_get_property_unchecked(c, false, win, atom, XCB_ATOM_ANY, 0, len);
        ScopedCPointer<xcb_get_property_reply_t> prop(xcb_get_property_reply(c, cookie, nullptr));
        if (prop.isNull()) {
            // get property failed
            return QByteArray();
        }
        if (prop->bytes_after > 0) {
            len *= 2;
            continue;
        }
        if (prop->type == type && prop->format == format) {
            return QByteArray(reinterpret_cast< const char* >(xcb_get_property_value(prop.data())),
                              xcb_get_property_value_length(prop.data()));
        } else {
            return QByteArray();
        }
    }
}

static void deleteWindowProperty(Window win, long int atom)
{
    xcb_delete_property(connection(), win, atom);
}

//---------------------

EffectsHandlerImpl::EffectsHandlerImpl(Compositor *compositor, Scene *scene)
    : EffectsHandler(scene->compositingType())
    , keyboard_grab_effect(NULL)
    , fullscreen_effect(0)
    , next_window_quad_type(EFFECT_QUAD_TYPE_START)
    , m_compositor(compositor)
    , m_scene(scene)
    , m_screenLockerWatcher(new ScreenLockerWatcher(this))
    , m_desktopRendering(false)
    , m_currentRenderedDesktop(0)
    , m_effectLoader(new EffectLoader(this))
{
    new EffectsAdaptor(this);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject(QStringLiteral("/Effects"), this);
    dbus.registerService(QStringLiteral("org.kde.kwin.Effects"));
    // init is important, otherwise causes crashes when quads are build before the first painting pass start
    m_currentBuildQuadsIterator = m_activeEffects.constEnd();

    Workspace *ws = Workspace::self();
    VirtualDesktopManager *vds = VirtualDesktopManager::self();
    connect(ws, &Workspace::currentDesktopChanged, this,
        [this](int old, Client *c) {
            const int newDesktop = VirtualDesktopManager::self()->current();
            if (old != 0 && newDesktop != old) {
                emit desktopChanged(old, newDesktop, c ? c->effectWindow() : 0);
                // TODO: remove in 4.10
                emit desktopChanged(old, newDesktop);
            }
        }
    );
    connect(ws, &Workspace::desktopPresenceChanged, this,
        [this](Client *c, int old) {
            if (!c->effectWindow()) {
                return;
            }
            emit desktopPresenceChanged(c->effectWindow(), old, c->desktop());
        }
    );
    connect(ws, &Workspace::clientAdded, this,
        [this](Client *c) {
            if (c->readyForPainting())
                slotClientShown(c);
            else
                connect(c, &Toplevel::windowShown, this, &EffectsHandlerImpl::slotClientShown);
        }
    );
    connect(ws, &Workspace::unmanagedAdded, this,
        [this](Unmanaged *u) {
            // it's never initially ready but has synthetic 50ms delay
            connect(u, &Toplevel::windowShown, this, &EffectsHandlerImpl::slotUnmanagedShown);
        }
    );
    connect(ws, &Workspace::clientActivated, this,
        [this](KWin::Client *c) {
            emit windowActivated(c ? c->effectWindow() : nullptr);
        }
    );
    connect(ws, &Workspace::deletedRemoved, this,
        [this](KWin::Deleted *d) {
            emit windowDeleted(d->effectWindow());
            elevated_windows.removeAll(d->effectWindow());
        }
    );
    connect(vds, &VirtualDesktopManager::countChanged, this, &EffectsHandler::numberDesktopsChanged);
    connect(Cursor::self(), &Cursor::mouseChanged, this, &EffectsHandler::mouseChanged);
    connect(ws, &Workspace::propertyNotify, this,
        [this](long int atom) {
            if (!registered_atoms.contains(atom))
                return;
            emit propertyNotify(nullptr, atom);
        }
    );
    connect(screens(), &Screens::countChanged,    this, &EffectsHandler::numberScreensChanged);
    connect(screens(), &Screens::sizeChanged,     this, &EffectsHandler::virtualScreenSizeChanged);
    connect(screens(), &Screens::geometryChanged, this, &EffectsHandler::virtualScreenGeometryChanged);
#ifdef KWIN_BUILD_ACTIVITIES
    Activities *activities = Activities::self();
    connect(activities, &Activities::added,          this, &EffectsHandler::activityAdded);
    connect(activities, &Activities::removed,        this, &EffectsHandler::activityRemoved);
    connect(activities, &Activities::currentChanged, this, &EffectsHandler::currentActivityChanged);
#endif
    connect(ws, &Workspace::stackingOrderChanged, this, &EffectsHandler::stackingOrderChanged);
#ifdef KWIN_BUILD_TABBOX
    TabBox::TabBox *tabBox = TabBox::TabBox::self();
    connect(tabBox, &TabBox::TabBox::tabBoxAdded,    this, &EffectsHandler::tabBoxAdded);
    connect(tabBox, &TabBox::TabBox::tabBoxUpdated,  this, &EffectsHandler::tabBoxUpdated);
    connect(tabBox, &TabBox::TabBox::tabBoxClosed,   this, &EffectsHandler::tabBoxClosed);
    connect(tabBox, &TabBox::TabBox::tabBoxKeyEvent, this, &EffectsHandler::tabBoxKeyEvent);
#endif
#ifdef KWIN_BUILD_SCREENEDGES
    connect(ScreenEdges::self(), &ScreenEdges::approaching, this, &EffectsHandler::screenEdgeApproaching);
#endif
    connect(m_screenLockerWatcher, &ScreenLockerWatcher::locked, this, &EffectsHandler::screenLockingChanged);
    // connect all clients
    for (Client *c : ws->clientList()) {
        setupClientConnections(c);
    }
    for (Unmanaged *u : ws->unmanagedList()) {
        setupUnmanagedConnections(u);
    }
    reconfigure();
}

EffectsHandlerImpl::~EffectsHandlerImpl()
{
    makeOpenGLContextCurrent();
    if (keyboard_grab_effect != NULL)
        ungrabKeyboard();
    setActiveFullScreenEffect(nullptr);
    for (auto it = loaded_effects.begin(); it != loaded_effects.end(); ++it) {
        const QString &name = (*it).first;
        Effect *effect = (*it).second;
        stopMouseInterception(effect);
        // remove support properties for the effect
        const QList<QByteArray> properties = m_propertiesForEffects.keys();
        for (const QByteArray &property : properties) {
            removeSupportProperty(property, effect);
        }
        delete effect;
        if (effect_libraries.contains(name)) {
            effect_libraries[ name ]->unload();
        }
    }
    loaded_effects.clear();
}

void EffectsHandlerImpl::setupClientConnections(Client* c)
{
    connect(c, &Client::windowClosed, this, &EffectsHandlerImpl::slotWindowClosed);
    connect(c, static_cast<void (Client::*)(KWin::Client*, KDecorationDefines::MaximizeMode)>(&Client::clientMaximizedStateChanged),
            this, &EffectsHandlerImpl::slotClientMaximized);
    connect(c, &Client::clientStartUserMovedResized, this,
        [this](Client *c) {
            emit windowStartUserMovedResized(c->effectWindow());
        }
    );
    connect(c, &Client::clientStepUserMovedResized, this,
        [this](Client *c, const QRect &geometry) {
            emit windowStepUserMovedResized(c->effectWindow(), geometry);
        }
    );
    connect(c, &Client::clientFinishUserMovedResized, this,
        [this](Client *c) {
            emit windowFinishUserMovedResized(c->effectWindow());
        }
    );
    connect(c, &Client::opacityChanged, this, &EffectsHandlerImpl::slotOpacityChanged);
    connect(c, &Client::clientMinimized, this,
        [this](Client *c, bool animate) {
            // TODO: notify effects even if it should not animate?
            if (animate) {
                emit windowMinimized(c->effectWindow());
            }
        }
    );
    connect(c, &Client::clientUnminimized, this,
        [this](Client* c, bool animate) {
            // TODO: notify effects even if it should not animate?
            if (animate) {
                emit windowUnminimized(c->effectWindow());
            }
        }
    );
    connect(c, &Client::modalChanged,         this, &EffectsHandlerImpl::slotClientModalityChanged);
    connect(c, &Client::geometryShapeChanged, this, &EffectsHandlerImpl::slotGeometryShapeChanged);
    connect(c, &Client::paddingChanged,       this, &EffectsHandlerImpl::slotPaddingChanged);
    connect(c, &Client::damaged,              this, &EffectsHandlerImpl::slotWindowDamaged);
    connect(c, &Client::propertyNotify,       this, &EffectsHandlerImpl::slotPropertyNotify);
}

void EffectsHandlerImpl::setupUnmanagedConnections(Unmanaged* u)
{
    connect(u, &Unmanaged::windowClosed,         this, &EffectsHandlerImpl::slotWindowClosed);
    connect(u, &Unmanaged::opacityChanged,       this, &EffectsHandlerImpl::slotOpacityChanged);
    connect(u, &Unmanaged::geometryShapeChanged, this, &EffectsHandlerImpl::slotGeometryShapeChanged);
    connect(u, &Unmanaged::paddingChanged,       this, &EffectsHandlerImpl::slotPaddingChanged);
    connect(u, &Unmanaged::damaged,              this, &EffectsHandlerImpl::slotWindowDamaged);
    connect(u, &Unmanaged::propertyNotify,       this, &EffectsHandlerImpl::slotPropertyNotify);
}

void EffectsHandlerImpl::reconfigure()
{
    // perform querying for the services in a thread
    QFutureWatcher<KService::List> *watcher = new QFutureWatcher<KService::List>(this);
    connect(watcher, SIGNAL(finished()), this, SLOT(slotEffectsQueried()));
    watcher->setFuture(QtConcurrent::run(KServiceTypeTrader::self(), &KServiceTypeTrader::query, QStringLiteral("KWin/Effect"), QString()));
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
    KConfigGroup conf(KSharedConfig::openConfig(), "Plugins");

    makeOpenGLContextCurrent();
    // First unload necessary effects
    for (const KService::Ptr & service : offers) {
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
    for (const QString & effectName : effectsToBeLoaded) {
        if (!isEffectLoaded(effectName)) {
            m_effectLoader->queue(effectName, checkDefault.contains(effectName));
            newLoaded.append(effectName);
        }
    }
    for (const EffectPair & ep : loaded_effects) {
        if (!newLoaded.contains(ep.first))    // don't reconfigure newly loaded effects
            ep.second->reconfigure(Effect::ReconfigureAll);
    }
    watcher->deleteLater();
}

// the idea is that effects call this function again which calls the next one
void EffectsHandlerImpl::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (m_currentPaintScreenIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintScreenIterator++)->prePaintScreen(data, time);
        --m_currentPaintScreenIterator;
    }
    // no special final code
}

void EffectsHandlerImpl::paintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    if (m_currentPaintScreenIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintScreenIterator++)->paintScreen(mask, region, data);
        --m_currentPaintScreenIterator;
    } else
        m_scene->finalPaintScreen(mask, region, data);
}

void EffectsHandlerImpl::paintDesktop(int desktop, int mask, QRegion region, ScreenPaintData &data)
{
    if (desktop < 1 || desktop > numberOfDesktops()) {
        return;
    }
    m_currentRenderedDesktop = desktop;
    m_desktopRendering = true;
    // save the paint screen iterator
    EffectsIterator savedIterator = m_currentPaintScreenIterator;
    m_currentPaintScreenIterator = m_activeEffects.constBegin();
    effects->paintScreen(mask, region, data);
    // restore the saved iterator
    m_currentPaintScreenIterator = savedIterator;
    m_desktopRendering = false;
}

void EffectsHandlerImpl::postPaintScreen()
{
    if (m_currentPaintScreenIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintScreenIterator++)->postPaintScreen();
        --m_currentPaintScreenIterator;
    }
    // no special final code
}

void EffectsHandlerImpl::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    if (m_currentPaintWindowIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintWindowIterator++)->prePaintWindow(w, data, time);
        --m_currentPaintWindowIterator;
    }
    // no special final code
}

void EffectsHandlerImpl::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (m_currentPaintWindowIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintWindowIterator++)->paintWindow(w, mask, region, data);
        --m_currentPaintWindowIterator;
    } else
        m_scene->finalPaintWindow(static_cast<EffectWindowImpl*>(w), mask, region, data);
}

void EffectsHandlerImpl::paintEffectFrame(EffectFrame* frame, QRegion region, double opacity, double frameOpacity)
{
    if (m_currentPaintEffectFrameIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintEffectFrameIterator++)->paintEffectFrame(frame, region, opacity, frameOpacity);
        --m_currentPaintEffectFrameIterator;
    } else {
        const EffectFrameImpl* frameImpl = static_cast<const EffectFrameImpl*>(frame);
        frameImpl->finalRender(region, opacity, frameOpacity);
    }
}

void EffectsHandlerImpl::postPaintWindow(EffectWindow* w)
{
    if (m_currentPaintWindowIterator != m_activeEffects.constEnd()) {
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
    if (m_currentDrawWindowIterator != m_activeEffects.constEnd()) {
        (*m_currentDrawWindowIterator++)->drawWindow(w, mask, region, data);
        --m_currentDrawWindowIterator;
    } else
        m_scene->finalDrawWindow(static_cast<EffectWindowImpl*>(w), mask, region, data);
}

void EffectsHandlerImpl::buildQuads(EffectWindow* w, WindowQuadList& quadList)
{
    static bool initIterator = true;
    if (initIterator) {
        m_currentBuildQuadsIterator = m_activeEffects.constBegin();
        initIterator = false;
    }
    if (m_currentBuildQuadsIterator != m_activeEffects.constEnd()) {
        (*m_currentBuildQuadsIterator++)->buildQuads(w, quadList);
        --m_currentBuildQuadsIterator;
    }
    if (m_currentBuildQuadsIterator == m_activeEffects.constBegin())
        initIterator = true;
}

bool EffectsHandlerImpl::hasDecorationShadows() const
{
    return decorationPlugin()->hasShadows();
}

bool EffectsHandlerImpl::decorationsHaveAlpha() const
{
    return decorationPlugin()->hasAlpha();
}

bool EffectsHandlerImpl::decorationSupportsBlurBehind() const
{
    return decorationPlugin()->supportsBlurBehind();
}

// start another painting pass
void EffectsHandlerImpl::startPaint()
{
    m_activeEffects.clear();
    m_activeEffects.reserve(loaded_effects.count());
    for(QVector< KWin::EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->isActive()) {
            m_activeEffects << it->second;
        }
    }
    m_currentDrawWindowIterator = m_activeEffects.constBegin();
    m_currentPaintWindowIterator = m_activeEffects.constBegin();
    m_currentPaintScreenIterator = m_activeEffects.constBegin();
    m_currentPaintEffectFrameIterator = m_activeEffects.constBegin();
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
    if (EffectWindowImpl *w = c->effectWindow()) {
        emit windowMaximizedStateChanged(w, horizontal, vertical);
    }
}

void EffectsHandlerImpl::slotOpacityChanged(Toplevel *t, qreal oldOpacity)
{
    if (t->opacity() == oldOpacity || !t->effectWindow()) {
        return;
    }
    emit windowOpacityChanged(t->effectWindow(), oldOpacity, (qreal)t->opacity());
}

void EffectsHandlerImpl::slotClientShown(KWin::Toplevel *t)
{
    Q_ASSERT(dynamic_cast<Client*>(t));
    Client *c = static_cast<Client*>(t);
    setupClientConnections(c);
    if (!c->tabGroup()) // the "window" has already been there
        emit windowAdded(c->effectWindow());
}

void EffectsHandlerImpl::slotUnmanagedShown(KWin::Toplevel *t)
{   // regardless, unmanaged windows are -yet?- not synced anyway
    Q_ASSERT(dynamic_cast<Unmanaged*>(t));
    Unmanaged *u = static_cast<Unmanaged*>(t);
    setupUnmanagedConnections(u);
    emit windowAdded(u->effectWindow());
}

void EffectsHandlerImpl::slotWindowClosed(KWin::Toplevel *c)
{
    c->disconnect(this);
    emit windowClosed(c->effectWindow());
}

void EffectsHandlerImpl::slotClientModalityChanged()
{
    emit windowModalityChanged(static_cast<Client*>(sender())->effectWindow());
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
    m_compositor->checkUnredirect();
}

Effect* EffectsHandlerImpl::activeFullScreenEffect() const
{
    return fullscreen_effect;
}

bool EffectsHandlerImpl::grabKeyboard(Effect* effect)
{
    if (keyboard_grab_effect != NULL)
        return false;
    if (kwinApp()->operationMode() == Application::OperationModeX11) {
        bool ret = grabXKeyboard();
        if (!ret)
            return false;
    }
    keyboard_grab_effect = effect;
    return true;
}

void EffectsHandlerImpl::ungrabKeyboard()
{
    assert(keyboard_grab_effect != NULL);
    if (kwinApp()->operationMode() == Application::OperationModeX11) {
        ungrabXKeyboard();
    }
    keyboard_grab_effect = NULL;
}

void EffectsHandlerImpl::grabbedKeyboardEvent(QKeyEvent* e)
{
    if (keyboard_grab_effect != NULL)
        keyboard_grab_effect->grabbedKeyboardEvent(e);
}

void EffectsHandlerImpl::startMouseInterception(Effect *effect, Qt::CursorShape shape)
{
    if (m_grabbedMouseEffects.contains(effect)) {
        return;
    }
    m_grabbedMouseEffects.append(effect);
    if (m_grabbedMouseEffects.size() != 1) {
        return;
    }
    if (kwinApp()->operationMode() != Application::OperationModeX11) {
#if HAVE_WAYLAND
        if (Wayland::WaylandBackend *w = Wayland::WaylandBackend::self()) {
            w->installCursorImage(shape);
        }
#endif
        return;
    }
    // NOTE: it is intended to not perform an XPointerGrab on X11. See documentation in kwineffects.h
    // The mouse grab is implemented by using a full screen input only window
    if (!m_mouseInterceptionWindow.isValid()) {
        const QRect geo(0, 0, displayWidth(), displayHeight());
        const uint32_t mask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK | XCB_CW_CURSOR;
        const uint32_t values[] = {
            true,
            XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION,
            Cursor::x11Cursor(shape)
        };
        m_mouseInterceptionWindow.reset(Xcb::createInputWindow(geo, mask, values));
    }
    m_mouseInterceptionWindow.map();
    m_mouseInterceptionWindow.raise();
    // Raise electric border windows above the input windows
    // so they can still be triggered.
#ifdef KWIN_BUILD_SCREENEDGES
    ScreenEdges::self()->ensureOnTop();
#endif
}

void EffectsHandlerImpl::stopMouseInterception(Effect *effect)
{
    if (!m_grabbedMouseEffects.contains(effect)) {
        return;
    }
    m_grabbedMouseEffects.removeAll(effect);
    if (kwinApp()->operationMode() != Application::OperationModeX11) {
        return;
    }
    if (m_grabbedMouseEffects.isEmpty()) {
        m_mouseInterceptionWindow.unmap();
#ifdef KWIN_BUILD_SCREENEDGES
        Workspace::self()->stackScreenEdgesUnderOverrideRedirect();
#endif
    }
}

void EffectsHandlerImpl::registerGlobalShortcut(const QKeySequence &shortcut, QAction *action)
{
    input()->registerShortcut(shortcut, action);
}

void EffectsHandlerImpl::registerPointerShortcut(Qt::KeyboardModifiers modifiers, Qt::MouseButton pointerButtons, QAction *action)
{
    input()->registerPointerShortcut(modifiers, pointerButtons, action);
}

void EffectsHandlerImpl::registerAxisShortcut(Qt::KeyboardModifiers modifiers, PointerAxisDirection axis, QAction *action)
{
    input()->registerAxisShortcut(modifiers, axis, action);
}

void* EffectsHandlerImpl::getProxy(QString name)
{
    // All effects start with "kwin4_effect_", prepend it to the name
    name.prepend(QStringLiteral("kwin4_effect_"));

    for (QVector< EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it)
        if ((*it).first == name)
            return (*it).second->proxy();

    return NULL;
}

void EffectsHandlerImpl::startMousePolling()
{
    Cursor::self()->startMousePolling();
}

void EffectsHandlerImpl::stopMousePolling()
{
    Cursor::self()->stopMousePolling();
}

bool EffectsHandlerImpl::hasKeyboardGrab() const
{
    return keyboard_grab_effect != NULL;
}

void EffectsHandlerImpl::desktopResized(const QSize &size)
{
    m_scene->screenGeometryChanged(size);
    if (m_mouseInterceptionWindow.isValid()) {
        m_mouseInterceptionWindow.setGeometry(QRect(0, 0, size.width(), size.height()));
    }
    emit screenGeometryChanged(size);
}

void EffectsHandlerImpl::slotPropertyNotify(Toplevel* t, long int atom)
{
    if (!registered_atoms.contains(atom))
        return;
    emit propertyNotify(t->effectWindow(), atom);
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

xcb_atom_t EffectsHandlerImpl::announceSupportProperty(const QByteArray &propertyName, Effect *effect)
{
    PropertyEffectMap::iterator it = m_propertiesForEffects.find(propertyName);
    if (it != m_propertiesForEffects.end()) {
        // property has already been registered for an effect
        // just append Effect and return the atom stored in m_managedProperties
        if (!it.value().contains(effect)) {
            it.value().append(effect);
        }
        return m_managedProperties.value(propertyName);
    }
    // get the atom for the propertyName
    ScopedCPointer<xcb_intern_atom_reply_t> atomReply(xcb_intern_atom_reply(connection(),
        xcb_intern_atom_unchecked(connection(), false, propertyName.size(), propertyName.constData()),
        NULL));
    if (atomReply.isNull()) {
        return XCB_ATOM_NONE;
    }
    m_compositor->keepSupportProperty(atomReply->atom);
    // announce property on root window
    unsigned char dummy = 0;
    xcb_change_property(connection(), XCB_PROP_MODE_REPLACE, rootWindow(), atomReply->atom, atomReply->atom, 8, 1, &dummy);
    // TODO: add to _NET_SUPPORTED
    m_managedProperties.insert(propertyName, atomReply->atom);
    m_propertiesForEffects.insert(propertyName, QList<Effect*>() << effect);
    registerPropertyType(atomReply->atom, true);
    return atomReply->atom;
}

void EffectsHandlerImpl::removeSupportProperty(const QByteArray &propertyName, Effect *effect)
{
    PropertyEffectMap::iterator it = m_propertiesForEffects.find(propertyName);
    if (it == m_propertiesForEffects.end()) {
        // property is not registered - nothing to do
        return;
    }
    if (!it.value().contains(effect)) {
        // property is not registered for given effect - nothing to do
        return;
    }
    it.value().removeAll(effect);
    if (!it.value().isEmpty()) {
        // property still registered for another effect - nothing further to do
        return;
    }
    const xcb_atom_t atom = m_managedProperties.take(propertyName);
    registerPropertyType(atom, false);
    m_propertiesForEffects.remove(propertyName);
    m_compositor->removeSupportProperty(atom); // delayed removal
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
#ifdef KWIN_BUILD_ACTIVITIES
    return Activities::self()->current();
#else
    return QString();
#endif
}

int EffectsHandlerImpl::currentDesktop() const
{
    return VirtualDesktopManager::self()->current();
}

int EffectsHandlerImpl::numberOfDesktops() const
{
    return VirtualDesktopManager::self()->count();
}

void EffectsHandlerImpl::setCurrentDesktop(int desktop)
{
    VirtualDesktopManager::self()->setCurrent(desktop);
}

void EffectsHandlerImpl::setNumberOfDesktops(int desktops)
{
    VirtualDesktopManager::self()->setCount(desktops);
}

QSize EffectsHandlerImpl::desktopGridSize() const
{
    return VirtualDesktopManager::self()->grid().size();
}

int EffectsHandlerImpl::desktopGridWidth() const
{
    return desktopGridSize().width();
}

int EffectsHandlerImpl::desktopGridHeight() const
{
    return desktopGridSize().height();
}

int EffectsHandlerImpl::workspaceWidth() const
{
    return desktopGridWidth() * displayWidth();
}

int EffectsHandlerImpl::workspaceHeight() const
{
    return desktopGridHeight() * displayHeight();
}

int EffectsHandlerImpl::desktopAtCoords(QPoint coords) const
{
    return VirtualDesktopManager::self()->grid().at(coords);
}

QPoint EffectsHandlerImpl::desktopGridCoords(int id) const
{
    return VirtualDesktopManager::self()->grid().gridCoords(id);
}

QPoint EffectsHandlerImpl::desktopCoords(int id) const
{
    QPoint coords = VirtualDesktopManager::self()->grid().gridCoords(id);
    if (coords.x() == -1)
        return QPoint(-1, -1);
    return QPoint(coords.x() * displayWidth(), coords.y() * displayHeight());
}

int EffectsHandlerImpl::desktopAbove(int desktop, bool wrap) const
{
    return getDesktop<DesktopAbove>(desktop, wrap);
}

int EffectsHandlerImpl::desktopToRight(int desktop, bool wrap) const
{
    return getDesktop<DesktopRight>(desktop, wrap);
}

int EffectsHandlerImpl::desktopBelow(int desktop, bool wrap) const
{
    return getDesktop<DesktopBelow>(desktop, wrap);
}

int EffectsHandlerImpl::desktopToLeft(int desktop, bool wrap) const
{
    return getDesktop<DesktopLeft>(desktop, wrap);
}

QString EffectsHandlerImpl::desktopName(int desktop) const
{
    return VirtualDesktopManager::self()->name(desktop);
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

EffectWindow* EffectsHandlerImpl::findWindow(WId id) const
{
    if (Client* w = Workspace::self()->findClient(Predicate::WindowMatch, id))
        return w->effectWindow();
    if (Unmanaged* w = Workspace::self()->findUnmanaged(id))
        return w->effectWindow();
    return NULL;
}

EffectWindowList EffectsHandlerImpl::stackingOrder() const
{
    ToplevelList list = Workspace::self()->xStackingOrder();
    EffectWindowList ret;
    for (Toplevel *t : list) {
        if (EffectWindow *w = effectWindow(t))
            ret.append(w);
    }
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
        TabBox::TabBox::self()->setCurrentClient(c);
    }
#else
    Q_UNUSED(w)
#endif
}

void EffectsHandlerImpl::setTabBoxDesktop(int desktop)
{
#ifdef KWIN_BUILD_TABBOX
    TabBox::TabBox::self()->setCurrentDesktop(desktop);
#else
    Q_UNUSED(desktop)
#endif
}

EffectWindowList EffectsHandlerImpl::currentTabBoxWindowList() const
{
#ifdef KWIN_BUILD_TABBOX
    EffectWindowList ret;
    ClientList clients;
    clients = TabBox::TabBox::self()->currentClientList();
    for (Client * c : clients)
    ret.append(c->effectWindow());
    return ret;
#else
    return EffectWindowList();
#endif
}

void EffectsHandlerImpl::refTabBox()
{
#ifdef KWIN_BUILD_TABBOX
    TabBox::TabBox::self()->reference();
#endif
}

void EffectsHandlerImpl::unrefTabBox()
{
#ifdef KWIN_BUILD_TABBOX
    TabBox::TabBox::self()->unreference();
#endif
}

void EffectsHandlerImpl::closeTabBox()
{
#ifdef KWIN_BUILD_TABBOX
    TabBox::TabBox::self()->close();
#endif
}

QList< int > EffectsHandlerImpl::currentTabBoxDesktopList() const
{
#ifdef KWIN_BUILD_TABBOX
    return TabBox::TabBox::self()->currentDesktopList();
#endif
    return QList< int >();
}

int EffectsHandlerImpl::currentTabBoxDesktop() const
{
#ifdef KWIN_BUILD_TABBOX
    return TabBox::TabBox::self()->currentDesktop();
#endif
    return -1;
}

EffectWindow* EffectsHandlerImpl::currentTabBoxWindow() const
{
#ifdef KWIN_BUILD_TABBOX
    if (Client* c = TabBox::TabBox::self()->currentClient())
        return c->effectWindow();
#endif
    return NULL;
}

void EffectsHandlerImpl::addRepaintFull()
{
    m_compositor->addRepaintFull();
}

void EffectsHandlerImpl::addRepaint(const QRect& r)
{
    m_compositor->addRepaint(r);
}

void EffectsHandlerImpl::addRepaint(const QRegion& r)
{
    m_compositor->addRepaint(r);
}

void EffectsHandlerImpl::addRepaint(int x, int y, int w, int h)
{
    m_compositor->addRepaint(x, y, w, h);
}

int EffectsHandlerImpl::activeScreen() const
{
    return screens()->current();
}

int EffectsHandlerImpl::numScreens() const
{
    return screens()->count();
}

int EffectsHandlerImpl::screenNumber(const QPoint& pos) const
{
    return screens()->number(pos);
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
        return Workspace::self()->clientArea(opt, t->geometry().center(), VirtualDesktopManager::self()->current());
}

QRect EffectsHandlerImpl::clientArea(clientAreaOption opt, const QPoint& p, int desktop) const
{
    return Workspace::self()->clientArea(opt, p, desktop);
}

QRect EffectsHandlerImpl::virtualScreenGeometry() const
{
    return screens()->geometry();
}

QSize EffectsHandlerImpl::virtualScreenSize() const
{
    return screens()->size();
}

void EffectsHandlerImpl::defineCursor(Qt::CursorShape shape)
{
    if (!m_mouseInterceptionWindow.isValid()) {
#if HAVE_WAYLAND
        if (Wayland::WaylandBackend *w = Wayland::WaylandBackend::self()) {
            w->installCursorImage(shape);
        }
#endif
        return;
    }
    m_mouseInterceptionWindow.defineCursor(Cursor::x11Cursor(shape));
}

bool EffectsHandlerImpl::checkInputWindowEvent(xcb_button_press_event_t *e)
{
    if (m_grabbedMouseEffects.isEmpty() || m_mouseInterceptionWindow != e->event) {
        return false;
    }
    for (Effect *effect : m_grabbedMouseEffects) {
        Qt::MouseButton button = x11ToQtMouseButton(e->detail);
        Qt::MouseButtons buttons = x11ToQtMouseButtons(e->state);
        const QEvent::Type type = ((e->response_type & ~0x80) == XCB_BUTTON_PRESS) ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease;
        if (type == QEvent::MouseButtonPress) {
            buttons |= button;
        } else {
            buttons &= ~button;
        }
        QMouseEvent ev(type,
                        QPoint(e->event_x, e->event_y), QPoint(e->root_x, e->root_y),
                        button, buttons, x11ToQtKeyboardModifiers(e->state));
        effect->windowInputMouseEvent(&ev);
    }
    return true; // eat event
}

bool EffectsHandlerImpl::checkInputWindowEvent(xcb_motion_notify_event_t *e)
{
    if (m_grabbedMouseEffects.isEmpty() || m_mouseInterceptionWindow != e->event) {
        return false;
    }
    for (Effect *effect : m_grabbedMouseEffects) {
        QMouseEvent ev(QEvent::MouseMove, QPoint(e->event_x, e->event_y), QPoint(e->root_x, e->root_y),
                        Qt::NoButton, x11ToQtMouseButtons(e->state), x11ToQtKeyboardModifiers(e->state));
        effect->windowInputMouseEvent(&ev);
    }
    return true; // eat event
}

bool EffectsHandlerImpl::checkInputWindowEvent(QMouseEvent *e)
{
    if (m_grabbedMouseEffects.isEmpty()) {
        return false;
    }
    foreach (Effect *effect, m_grabbedMouseEffects) {
        effect->windowInputMouseEvent(e);
    }
    return true;
}

void EffectsHandlerImpl::checkInputWindowStacking()
{
    if (m_grabbedMouseEffects.isEmpty()) {
        return;
    }
    if (kwinApp()->operationMode() != Application::OperationModeX11) {
        return;
    }
    m_mouseInterceptionWindow.raise();
    // Raise electric border windows above the input windows
    // so they can still be triggered. TODO: Do both at once.
#ifdef KWIN_BUILD_SCREENEDGES
    ScreenEdges::self()->ensureOnTop();
#endif
}

QPoint EffectsHandlerImpl::cursorPos() const
{
    return Cursor::pos();
}

void EffectsHandlerImpl::reserveElectricBorder(ElectricBorder border, Effect *effect)
{
#ifdef KWIN_BUILD_SCREENEDGES
    ScreenEdges::self()->reserve(border, effect, "borderActivated");
#else
    Q_UNUSED(border)
    Q_UNUSED(effect)
#endif
}

void EffectsHandlerImpl::unreserveElectricBorder(ElectricBorder border, Effect *effect)
{
#ifdef KWIN_BUILD_SCREENEDGES
    ScreenEdges::self()->unreserve(border, effect);
#else
    Q_UNUSED(border)
    Q_UNUSED(effect)
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

QPainter *EffectsHandlerImpl::scenePainter()
{
    if (SceneQPainter *s = dynamic_cast<SceneQPainter*>(m_scene)) {
        return s->painter();
    } else {
        return NULL;
    }
}

KLibrary* EffectsHandlerImpl::findEffectLibrary(KService* service)
{
    QString libname = service->library();
#ifdef KWIN_HAVE_OPENGLES
    if (libname.startsWith(QStringLiteral("kwin4_effect_"))) {
        libname.replace(QStringLiteral("kwin4_effect_"), QStringLiteral("kwin4_effect_gles_"));
    }
#endif
    libname.replace(QStringLiteral("kwin"), QStringLiteral(KWIN_NAME));
    KLibrary* library = new KLibrary(libname);
    if (!library) {
        qCritical() << "couldn't open library for effect '" <<
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
    KService::List offers = KServiceTypeTrader::self()->query(QStringLiteral("KWin/Effect"));
    QStringList listOfModules;
    // First unload necessary effects
    for (const KService::Ptr & service : offers) {
        KPluginInfo plugininfo(service);
        listOfModules << plugininfo.pluginName();
    }
    return listOfModules;
}

KService::Ptr EffectsHandlerImpl::findEffectService(const QString &internalName) const
{
    QString constraint = QStringLiteral("[X-KDE-PluginInfo-Name] == '%1'").arg(internalName);
    KService::List offers = KServiceTypeTrader::self()->query(QStringLiteral("KWin/Effect"), constraint);
    if (offers.isEmpty()) {
        return KService::Ptr();
    }
    return offers.first();
}

bool EffectsHandlerImpl::isScriptedEffect(KService::Ptr service) const
{
    return service->property(QStringLiteral("X-Plasma-API")).toString() == QStringLiteral("javascript");
}

bool EffectsHandlerImpl::loadEffect(const QString& name, bool checkDefault)
{
    makeOpenGLContextCurrent();
    m_compositor->addRepaintFull();

    if (!name.startsWith(QLatin1String("kwin4_effect_")))
        qWarning() << "Effect names usually have kwin4_effect_ prefix" ;

    // Make sure a single effect won't be loaded multiple times
    for (QVector< EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if ((*it).first == name) {
            qDebug() << "EffectsHandler::loadEffect : Effect already loaded : " << name;
            return true;
        }
    }


    qDebug() << "Trying to load " << name;
    QString internalname = name.toLower();

    KService::Ptr service = findEffectService(internalname);
    if (!service) {
        qCritical() << "Couldn't find effect " << name << endl;
        return false;
    }

    if (isScriptedEffect(service)) {
        // this is a scripted effect - use different loader
        return loadScriptedEffect(name, service.data());
    }

    if (Effect *e = loadBuiltInEffect(internalname.remove(QStringLiteral("kwin4_effect_")).toUtf8(), checkDefault)) {
        effect_order.insert(service->property(QStringLiteral("X-KDE-Ordering")).toInt(), EffectPair(name, e));
        effectsChanged();
        return true;
    }

    KLibrary* library = findEffectLibrary(service.data());
    if (!library) {
        return false;
    }

    QString version_symbol = QStringLiteral("effect_version_") + name;
    KLibrary::void_function_ptr version_func = library->resolveFunction(version_symbol.toAscii().constData());
    if (version_func == NULL) {
        qWarning() << "Effect " << name << " does not provide required API version, ignoring.";
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
        qWarning() << "Effect " << name << " requires unsupported API version " << version;
        delete library;
        return false;
    }

    const QString enabledByDefault_symbol = QStringLiteral("effect_enabledbydefault_") + name;
    KLibrary::void_function_ptr enabledByDefault_func = library->resolveFunction(enabledByDefault_symbol.toAscii().data());

    const QString supported_symbol = QStringLiteral("effect_supported_") + name;
    KLibrary::void_function_ptr supported_func = library->resolveFunction(supported_symbol.toAscii().data());

    const QString create_symbol = QStringLiteral("effect_create_") + name;
    KLibrary::void_function_ptr create_func = library->resolveFunction(create_symbol.toAscii().data());

    if (supported_func) {
        typedef bool (*t_supportedfunc)();
        t_supportedfunc supported = reinterpret_cast<t_supportedfunc>(supported_func);
        if (!supported()) {
            qWarning() << "EffectsHandler::loadEffect : Effect " << name << " is not supported" ;
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
        qCritical() << "EffectsHandler::loadEffect : effect_create function not found" << endl;
        library->unload();
        return false;
    }

    typedef Effect*(*t_createfunc)();
    t_createfunc create = reinterpret_cast<t_createfunc>(create_func);

    // Make sure all depenedencies have been loaded
    // TODO: detect circular deps
    KPluginInfo plugininfo(service);
    QStringList dependencies = plugininfo.dependencies();
    for (const QString & depName : dependencies) {
        if (!loadEffect(depName)) {
            qCritical() << "EffectsHandler::loadEffect : Couldn't load dependencies for effect " << name << endl;
            library->unload();
            return false;
        }
    }

    Effect* e = create();

    effect_order.insert(service->property(QStringLiteral("X-KDE-Ordering")).toInt(), EffectPair(name, e));
    effectsChanged();
    effect_libraries[ name ] = library;

    return true;
}

Effect *EffectsHandlerImpl::loadBuiltInEffect(const QByteArray &name, bool checkDefault)
{
    if (!BuiltInEffects::available(name)) {
        return nullptr;
    }
    if (!BuiltInEffects::supported(name)) {
        qWarning() << "Effect " << name << " is not supported" ;
        return nullptr;
    }
    if (checkDefault) {
        if (!BuiltInEffects::enabledByDefault(name)) {
            return nullptr;
        }
    }
    return BuiltInEffects::create(name);
}

bool EffectsHandlerImpl::loadScriptedEffect(const QString& name, KService *service)
{
    const KDesktopFile df(QStandardPaths::GenericDataLocation, QStringLiteral("kde5/services/") + service->entryPath());
    const QString scriptName = df.desktopGroup().readEntry<QString>(QStringLiteral("X-Plasma-MainScript"), QString());
    if (scriptName.isEmpty()) {
        qDebug() << "X-Plasma-MainScript not set";
        return false;
    }
    const QString scriptFile = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral(KWIN_NAME) + QStringLiteral("/effects/") + name + QStringLiteral("/contents/") + scriptName);
    if (scriptFile.isNull()) {
        qDebug() << "Could not locate the effect script";
        return false;
    }
    ScriptedEffect *effect = ScriptedEffect::create(name, scriptFile);
    if (!effect) {
        qDebug() << "Could not initialize scripted effect: " << name;
        return false;
    }
    effect_order.insert(service->property(QStringLiteral("X-KDE-Ordering")).toInt(), EffectPair(name, effect));
    effectsChanged();
    return true;
}

void EffectsHandlerImpl::unloadEffect(const QString& name)
{
    makeOpenGLContextCurrent();
    m_compositor->addRepaintFull();

    for (QMap< int, EffectPair >::iterator it = effect_order.begin(); it != effect_order.end(); ++it) {
        if (it.value().first == name) {
            qDebug() << "EffectsHandler::unloadEffect : Unloading Effect : " << name;
            if (activeFullScreenEffect() == it.value().second) {
                setActiveFullScreenEffect(0);
            }
            stopMouseInterception(it.value().second);
            // remove support properties for the effect
            const QList<QByteArray> properties = m_propertiesForEffects.keys();
            for (const QByteArray &property : properties) {
                removeSupportProperty(property, it.value().second);
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

    qDebug() << "EffectsHandler::unloadEffect : Effect not loaded : " << name;
}

void EffectsHandlerImpl::reconfigureEffect(const QString& name)
{
    for (QVector< EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it)
        if ((*it).first == name) {
            KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral(KWIN_CONFIG));
            config->reparseConfiguration();
            makeOpenGLContextCurrent();
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

bool EffectsHandlerImpl::isEffectSupported(const QString &name)
{
    // if the effect is loaded, it is obviously supported
    auto it = std::find_if(loaded_effects.constBegin(), loaded_effects.constEnd(), [name](const EffectPair &pair) {
        return pair.first == name;
    });
    if (it != loaded_effects.constEnd()) {
        return true;
    }

    const QString internalName = name.toLower();
    KService::Ptr service = findEffectService(name.toLower());
    if (!service) {
        // effect not found
        return false;
    }

    if (isScriptedEffect(service)) {
        // scripted effects are generally supported
        return true;
    }

    // next checks might require a context
    makeOpenGLContextCurrent();
    m_compositor->addRepaintFull();

    // try builtin effects
    const QByteArray builtInName = internalName.mid(13).toUtf8(); // drop kwin4_effect_
    if (BuiltInEffects::available(builtInName)) {
        return BuiltInEffects::supported(builtInName);
    }

    // try remaining effects
    KLibrary* library = findEffectLibrary(service.data());
    if (!library) {
        return false;
    }

    const QString supported_symbol = QStringLiteral("effect_supported_") + name;
    KLibrary::void_function_ptr supported_func = library->resolveFunction(supported_symbol.toAscii().data());

    bool supported = true;

    if (supported_func) {
        typedef bool (*t_supportedfunc)();
        t_supportedfunc supportedFunction = reinterpret_cast<t_supportedfunc>(supported_func);
        supported = supportedFunction();
    }

    library->unload();

    return supported;
}

QList< bool > EffectsHandlerImpl::areEffectsSupported(const QStringList &names)
{
    QList< bool > retList;
    for (const QString &name : names) {
        retList << isEffectSupported(name);
    }
    return retList;
}

void EffectsHandlerImpl::reloadEffect(Effect *effect)
{
    QString effectName;
    for (QVector< EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if ((*it).second == effect) {
            effectName = (*it).first;
            break;
        }
    }
    if (!effectName.isNull()) {
        unloadEffect(effectName);
        m_effectLoader->queue(effectName);
    }
}

void EffectsHandlerImpl::effectsChanged()
{
    loaded_effects.clear();
    m_activeEffects.clear(); // it's possible to have a reconfigure and a quad rebuild between two paint cycles - bug #308201
//    qDebug() << "Recreating effects' list:";
    for (const EffectPair & effect : effect_order) {
//        qDebug() << effect.first;
        loaded_effects.append(effect);
    }
    m_activeEffects.reserve(loaded_effects.count());
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
        return decorationPlugin()->closeButtonCorner();
#ifdef KWIN_BUILD_SCREENEDGES
    case SwitchDesktopOnScreenEdge:
        return ScreenEdges::self()->isDesktopSwitching();
    case SwitchDesktopOnScreenEdgeMovingWindows:
        return ScreenEdges::self()->isDesktopSwitchingMovingClients();
#endif
    default:
        return QVariant(); // an invalid one
    }
}

QString EffectsHandlerImpl::supportInformation(const QString &name) const
{
    if (!isEffectLoaded(name)) {
        return QString();
    }
    for (QVector< EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if ((*it).first == name) {
            QString support((*it).first + QStringLiteral(":\n"));
            const QMetaObject *metaOptions = (*it).second->metaObject();
            for (int i=0; i<metaOptions->propertyCount(); ++i) {
                const QMetaProperty property = metaOptions->property(i);
                if (QLatin1String(property.name()) == QLatin1String("objectName")) {
                    continue;
                }
                support.append(QString::fromUtf8(property.name()) + QStringLiteral(": ") + (*it).second->property(property.name()).toString() + QStringLiteral("\n"));
            }
            return support;
        }
    }
    return QString();
}


bool EffectsHandlerImpl::isScreenLocked() const
{
    return m_screenLockerWatcher->isLocked();
}

QString EffectsHandlerImpl::debug(const QString& name, const QString& parameter) const
{
    QString internalName = name.startsWith(QStringLiteral("kwin4_effect_")) ? name : QStringLiteral("kwin4_effect_") + name;
    for (QVector< EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if ((*it).first == internalName) {
            return it->second->debug(parameter);
        }
    }
    return QString();
}

bool EffectsHandlerImpl::makeOpenGLContextCurrent()
{
    return m_scene->makeOpenGLContextCurrent();
}

void EffectsHandlerImpl::doneOpenGLContextCurrent()
{
    m_scene->doneOpenGLContextCurrent();
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
        return d->unrefWindow();   // delays deletion in case
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

template <typename T>
EffectWindowList getMainWindows(Toplevel *toplevel)
{
    T *c = static_cast<T*>(toplevel);
    EffectWindowList ret;
    ClientList mainclients = c->mainClients();
    for (Client * tmp : mainclients)
        ret.append(tmp->effectWindow());
    return ret;
}

EffectWindowList EffectWindowImpl::mainWindows() const
{
    if (toplevel->isClient()) {
        return getMainWindows<Client>(toplevel);
    } else if (toplevel->isDeleted()) {
        return getMainWindows<Deleted>(toplevel);
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

void EffectWindowImpl::elevate(bool elevate)
{
    effects->setElevatedWindow(this, elevate);
}

void EffectWindowImpl::registerThumbnail(AbstractThumbnailItem *item)
{
    if (WindowThumbnailItem *thumb = qobject_cast<WindowThumbnailItem*>(item)) {
        insertThumbnail(thumb);
        connect(thumb, SIGNAL(destroyed(QObject*)), SLOT(thumbnailDestroyed(QObject*)));
        connect(thumb, SIGNAL(wIdChanged(qulonglong)), SLOT(thumbnailTargetChanged()));
    } else if (DesktopThumbnailItem *desktopThumb = qobject_cast<DesktopThumbnailItem*>(item)) {
        m_desktopThumbnails.append(desktopThumb);
        connect(desktopThumb, SIGNAL(destroyed(QObject*)), SLOT(desktopThumbnailDestroyed(QObject*)));
    }
}

void EffectWindowImpl::thumbnailDestroyed(QObject *object)
{
    // we know it is a ThumbnailItem
    m_thumbnails.remove(static_cast<WindowThumbnailItem*>(object));
}

void EffectWindowImpl::thumbnailTargetChanged()
{
    if (WindowThumbnailItem *item = qobject_cast<WindowThumbnailItem*>(sender())) {
        insertThumbnail(item);
    }
}

void EffectWindowImpl::insertThumbnail(WindowThumbnailItem *item)
{
    EffectWindow *w = effects->findWindow(item->wId());
    if (w) {
        m_thumbnails.insert(item, QWeakPointer<EffectWindowImpl>(static_cast<EffectWindowImpl*>(w)));
    } else {
        m_thumbnails.insert(item, QWeakPointer<EffectWindowImpl>());
    }
}

void EffectWindowImpl::desktopThumbnailDestroyed(QObject *object)
{
    // we know it is a DesktopThumbnailItem
    m_desktopThumbnails.removeAll(static_cast<DesktopThumbnailItem*>(object));
}

void EffectWindowImpl::referencePreviousWindowPixmap()
{
    if (sw) {
        sw->referencePreviousPixmap();
    }
}

void EffectWindowImpl::unreferencePreviousWindowPixmap()
{
    if (sw) {
        sw->unreferencePreviousPixmap();
    }
}

//****************************************
// EffectWindowGroupImpl
//****************************************


EffectWindowList EffectWindowGroupImpl::members() const
{
    EffectWindowList ret;
    for (Toplevel * c : group->members())
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
    , m_theme(new Plasma::Theme(this))
{
    if (m_style == EffectFrameStyled) {
        m_frame.setImagePath(QStringLiteral("widgets/background"));
        m_frame.setCacheAllRenderedFrames(true);
        connect(m_theme, SIGNAL(themeChanged()), this, SLOT(plasmaThemeChanged()));
    }
    m_selection.setImagePath(QStringLiteral("widgets/viewitem"));
    m_selection.setElementPrefix(QStringLiteral("hover"));
    m_selection.setCacheAllRenderedFrames(true);
    m_selection.setEnabledBorders(Plasma::FrameSvg::AllBorders);

    m_sceneFrame = Compositor::self()->scene()->createEffectFrame(this);
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

const QIcon& EffectFrameImpl::icon() const
{
    return m_icon;
}

void EffectFrameImpl::setIcon(const QIcon& icon)
{
    m_icon = icon;
    if (isCrossFade()) {
        m_sceneFrame->crossFadeIcon();
    }
    if (m_iconSize.isEmpty() && !m_icon.availableSizes().isEmpty()) { // Set a size if we don't already have one
        setIconSize(m_icon.availableSizes().first());
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
    return m_theme->color(Plasma::Theme::TextColor);
}

} // namespace
