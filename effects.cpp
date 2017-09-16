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
#include "effects_mouse_interception_x11_filter.h"
#include "effectloader.h"
#ifdef KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#include "deleted.h"
#include "client.h"
#include "cursor.h"
#include "group.h"
#include "osd.h"
#include "pointer_input.h"
#include "unmanaged.h"
#ifdef KWIN_BUILD_TABBOX
#include "tabbox.h"
#endif
#include "screenedge.h"
#include "scripting/scriptedeffect.h"
#include "screens.h"
#include "screenlockerwatcher.h"
#include "thumbnailitem.h"
#include "virtualdesktops.h"
#include "window_property_notify_x11_filter.h"
#include "workspace.h"
#include "kwinglutils.h"

#include <QDebug>
#include <QDesktopWidget>

#include <Plasma/Theme>

#include <assert.h>
#include "composite.h"
#include "xcbutils.h"
#include "platform.h"
#include "shell_client.h"
#include "wayland_server.h"

#include "decorations/decorationbridge.h"
#include <KDecoration2/DecorationSettings>

namespace KWin
{
//---------------------
// Static

static QByteArray readWindowProperty(xcb_window_t win, xcb_atom_t atom, xcb_atom_t type, int format)
{
    if (win == XCB_WINDOW_NONE) {
        return QByteArray();
    }
    uint32_t len = 32768;
    for (;;) {
        Xcb::Property prop(false, win, atom, XCB_ATOM_ANY, 0, len);
        if (prop.isNull()) {
            // get property failed
            return QByteArray();
        }
        if (prop->bytes_after > 0) {
            len *= 2;
            continue;
        }
        return prop.toByteArray(format, type);
    }
}

static void deleteWindowProperty(Window win, long int atom)
{
    if (win == XCB_WINDOW_NONE) {
        return;
    }
    xcb_delete_property(kwinApp()->x11Connection(), win, atom);
}

static xcb_atom_t registerSupportProperty(const QByteArray &propertyName)
{
    auto c = kwinApp()->x11Connection();
    if (!c) {
        return XCB_ATOM_NONE;
    }
    // get the atom for the propertyName
    ScopedCPointer<xcb_intern_atom_reply_t> atomReply(xcb_intern_atom_reply(c,
        xcb_intern_atom_unchecked(c, false, propertyName.size(), propertyName.constData()),
        NULL));
    if (atomReply.isNull()) {
        return XCB_ATOM_NONE;
    }
    // announce property on root window
    unsigned char dummy = 0;
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, kwinApp()->x11RootWindow(), atomReply->atom, atomReply->atom, 8, 1, &dummy);
    // TODO: add to _NET_SUPPORTED
    return atomReply->atom;
}

//---------------------

EffectsHandlerImpl::EffectsHandlerImpl(Compositor *compositor, Scene *scene)
    : EffectsHandler(scene->compositingType())
    , keyboard_grab_effect(NULL)
    , fullscreen_effect(0)
    , next_window_quad_type(EFFECT_QUAD_TYPE_START)
    , m_compositor(compositor)
    , m_scene(scene)
    , m_desktopRendering(false)
    , m_currentRenderedDesktop(0)
    , m_effectLoader(new EffectLoader(this))
    , m_trackingCursorChanges(0)
{
    qRegisterMetaType<QVector<KWin::EffectWindow*>>();
    connect(m_effectLoader, &AbstractEffectLoader::effectLoaded, this,
        [this](Effect *effect, const QString &name) {
            effect_order.insert(effect->requestedEffectChainPosition(), EffectPair(name, effect));
            loaded_effects << EffectPair(name, effect);
            effectsChanged();
        }
    );
    m_effectLoader->setConfig(kwinApp()->config());
    new EffectsAdaptor(this);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject(QStringLiteral("/Effects"), this);
    // init is important, otherwise causes crashes when quads are build before the first painting pass start
    m_currentBuildQuadsIterator = m_activeEffects.constEnd();

    Workspace *ws = Workspace::self();
    VirtualDesktopManager *vds = VirtualDesktopManager::self();
    connect(ws, &Workspace::showingDesktopChanged,
            this, &EffectsHandlerImpl::showingDesktopChanged);
    connect(ws, &Workspace::currentDesktopChanged, this,
        [this](int old, AbstractClient *c) {
            const int newDesktop = VirtualDesktopManager::self()->current();
            if (old != 0 && newDesktop != old) {
                emit desktopChanged(old, newDesktop, c ? c->effectWindow() : 0);
                // TODO: remove in 4.10
                emit desktopChanged(old, newDesktop);
            }
        }
    );
    connect(ws, &Workspace::desktopPresenceChanged, this,
        [this](AbstractClient *c, int old) {
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
        [this](KWin::AbstractClient *c) {
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
    connect(screens(), &Screens::countChanged,    this, &EffectsHandler::numberScreensChanged);
    connect(screens(), &Screens::sizeChanged,     this, &EffectsHandler::virtualScreenSizeChanged);
    connect(screens(), &Screens::geometryChanged, this, &EffectsHandler::virtualScreenGeometryChanged);
#ifdef KWIN_BUILD_ACTIVITIES
    if (Activities *activities = Activities::self()) {
        connect(activities, &Activities::added,          this, &EffectsHandler::activityAdded);
        connect(activities, &Activities::removed,        this, &EffectsHandler::activityRemoved);
        connect(activities, &Activities::currentChanged, this, &EffectsHandler::currentActivityChanged);
    }
#endif
    connect(ws, &Workspace::stackingOrderChanged, this, &EffectsHandler::stackingOrderChanged);
#ifdef KWIN_BUILD_TABBOX
    TabBox::TabBox *tabBox = TabBox::TabBox::self();
    connect(tabBox, &TabBox::TabBox::tabBoxAdded,    this, &EffectsHandler::tabBoxAdded);
    connect(tabBox, &TabBox::TabBox::tabBoxUpdated,  this, &EffectsHandler::tabBoxUpdated);
    connect(tabBox, &TabBox::TabBox::tabBoxClosed,   this, &EffectsHandler::tabBoxClosed);
    connect(tabBox, &TabBox::TabBox::tabBoxKeyEvent, this, &EffectsHandler::tabBoxKeyEvent);
#endif
    connect(ScreenEdges::self(), &ScreenEdges::approaching, this, &EffectsHandler::screenEdgeApproaching);
    connect(ScreenLockerWatcher::self(), &ScreenLockerWatcher::locked, this, &EffectsHandler::screenLockingChanged);

    connect(kwinApp(), &Application::x11ConnectionChanged, this,
        [this] {
            registered_atoms.clear();
            for (auto it = m_propertiesForEffects.keyBegin(); it != m_propertiesForEffects.keyEnd(); it++) {
                const auto atom = registerSupportProperty(*it);
                if (atom == XCB_ATOM_NONE) {
                    continue;
                }
                m_compositor->keepSupportProperty(atom);
                m_managedProperties.insert(*it, atom);
                registerPropertyType(atom, true);
            }
            if (kwinApp()->x11Connection()) {
                m_x11WindowPropertyNotify = std::make_unique<WindowPropertyNotifyX11Filter>(this);
            } else {
                m_x11WindowPropertyNotify.reset();
            }
            emit xcbConnectionChanged();
        }
    );

    if (kwinApp()->x11Connection()) {
        m_x11WindowPropertyNotify = std::make_unique<WindowPropertyNotifyX11Filter>(this);
    }

    // connect all clients
    for (Client *c : ws->clientList()) {
        setupClientConnections(c);
    }
    for (Unmanaged *u : ws->unmanagedList()) {
        setupUnmanagedConnections(u);
    }
    if (auto w = waylandServer()) {
        connect(w, &WaylandServer::shellClientAdded, this,
            [this](ShellClient *c) {
                if (c->readyForPainting())
                    slotShellClientShown(c);
                else
                    connect(c, &Toplevel::windowShown, this, &EffectsHandlerImpl::slotShellClientShown);
            }
        );
    }
    reconfigure();
}

EffectsHandlerImpl::~EffectsHandlerImpl()
{
    unloadAllEffects();
}

void EffectsHandlerImpl::unloadAllEffects()
{
    makeOpenGLContextCurrent();
    if (keyboard_grab_effect != NULL)
        ungrabKeyboard();
    setActiveFullScreenEffect(nullptr);
    for (auto it = loaded_effects.begin(); it != loaded_effects.end(); ++it) {
        Effect *effect = (*it).second;
        stopMouseInterception(effect);
        // remove support properties for the effect
        const QList<QByteArray> properties = m_propertiesForEffects.keys();
        for (const QByteArray &property : properties) {
            removeSupportProperty(property, effect);
        }
        delete effect;
    }
    loaded_effects.clear();
    m_effectLoader->clear();
}

void EffectsHandlerImpl::setupAbstractClientConnections(AbstractClient* c)
{
    connect(c, &AbstractClient::windowClosed, this, &EffectsHandlerImpl::slotWindowClosed);
    connect(c, static_cast<void (AbstractClient::*)(KWin::AbstractClient*, MaximizeMode)>(&AbstractClient::clientMaximizedStateChanged),
            this, &EffectsHandlerImpl::slotClientMaximized);
    connect(c, &AbstractClient::clientStartUserMovedResized, this,
        [this](AbstractClient *c) {
            emit windowStartUserMovedResized(c->effectWindow());
        }
    );
    connect(c, &AbstractClient::clientStepUserMovedResized, this,
        [this](AbstractClient *c, const QRect &geometry) {
            emit windowStepUserMovedResized(c->effectWindow(), geometry);
        }
    );
    connect(c, &AbstractClient::clientFinishUserMovedResized, this,
        [this](AbstractClient *c) {
            emit windowFinishUserMovedResized(c->effectWindow());
        }
    );
    connect(c, &AbstractClient::opacityChanged, this, &EffectsHandlerImpl::slotOpacityChanged);
    connect(c, &AbstractClient::clientMinimized, this,
        [this](AbstractClient *c, bool animate) {
            // TODO: notify effects even if it should not animate?
            if (animate) {
                emit windowMinimized(c->effectWindow());
            }
        }
    );
    connect(c, &AbstractClient::clientUnminimized, this,
        [this](AbstractClient* c, bool animate) {
            // TODO: notify effects even if it should not animate?
            if (animate) {
                emit windowUnminimized(c->effectWindow());
            }
        }
    );
    connect(c, &AbstractClient::modalChanged,         this, &EffectsHandlerImpl::slotClientModalityChanged);
    connect(c, &AbstractClient::geometryShapeChanged, this, &EffectsHandlerImpl::slotGeometryShapeChanged);
    connect(c, &AbstractClient::damaged,              this, &EffectsHandlerImpl::slotWindowDamaged);
    connect(c, &AbstractClient::unresponsiveChanged, this,
        [this, c](bool unresponsive) {
            emit windowUnresponsiveChanged(c->effectWindow(), unresponsive);
        }
    );
    connect(c, &AbstractClient::windowShown, this,
        [this](Toplevel *c) {
            emit windowShown(c->effectWindow());
        }
    );
    connect(c, &AbstractClient::windowHidden, this,
        [this](Toplevel *c) {
            emit windowHidden(c->effectWindow());
        }
    );
}

void EffectsHandlerImpl::setupClientConnections(Client* c)
{
    setupAbstractClientConnections(c);
    connect(c, &Client::paddingChanged,       this, &EffectsHandlerImpl::slotPaddingChanged);
}

void EffectsHandlerImpl::setupUnmanagedConnections(Unmanaged* u)
{
    connect(u, &Unmanaged::windowClosed,         this, &EffectsHandlerImpl::slotWindowClosed);
    connect(u, &Unmanaged::opacityChanged,       this, &EffectsHandlerImpl::slotOpacityChanged);
    connect(u, &Unmanaged::geometryShapeChanged, this, &EffectsHandlerImpl::slotGeometryShapeChanged);
    connect(u, &Unmanaged::paddingChanged,       this, &EffectsHandlerImpl::slotPaddingChanged);
    connect(u, &Unmanaged::damaged,              this, &EffectsHandlerImpl::slotWindowDamaged);
}

void EffectsHandlerImpl::reconfigure()
{
    m_effectLoader->queryAndLoadAll();
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
    return false;
}

bool EffectsHandlerImpl::decorationsHaveAlpha() const
{
    return true;
}

bool EffectsHandlerImpl::decorationSupportsBlurBehind() const
{
    return Decoration::DecorationBridge::self()->needsBlur();
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

void EffectsHandlerImpl::slotClientMaximized(KWin::AbstractClient *c, MaximizeMode maxMode)
{
    bool horizontal = false;
    bool vertical = false;
    switch (maxMode) {
    case MaximizeHorizontal:
        horizontal = true;
        break;
    case MaximizeVertical:
        vertical = true;
        break;
    case MaximizeFull:
        horizontal = true;
        vertical = true;
        break;
    case MaximizeRestore: // fall through
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
    disconnect(c, &Toplevel::windowShown, this, &EffectsHandlerImpl::slotClientShown);
    setupClientConnections(c);
    if (!c->tabGroup()) // the "window" has already been there
        emit windowAdded(c->effectWindow());
}

void EffectsHandlerImpl::slotShellClientShown(Toplevel *t)
{
    ShellClient *c = static_cast<ShellClient*>(t);
    setupAbstractClientConnections(c);
    emit windowAdded(t->effectWindow());
}

void EffectsHandlerImpl::slotUnmanagedShown(KWin::Toplevel *t)
{   // regardless, unmanaged windows are -yet?- not synced anyway
    Q_ASSERT(dynamic_cast<Unmanaged*>(t));
    Unmanaged *u = static_cast<Unmanaged*>(t);
    setupUnmanagedConnections(u);
    emit windowAdded(u->effectWindow());
}

void EffectsHandlerImpl::slotWindowClosed(KWin::Toplevel *c, KWin::Deleted *d)
{
    c->disconnect(this);
    if (d) {
        emit windowClosed(c->effectWindow());
    }
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
        // Workaround for Qt 5.9 regression introduced with 2b34aefcf02f09253473b096eb4faffd3e62b5f4
        // we no longer get any events for the root window, one needs to call winId() on the desktop window
        // TODO: change effects event handling to create the appropriate QKeyEvent without relying on Qt
        // as it's done already in the Wayland case.
        qApp->desktop()->winId();
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
        input()->pointer()->setEffectsOverrideCursor(shape);
        return;
    }
    // NOTE: it is intended to not perform an XPointerGrab on X11. See documentation in kwineffects.h
    // The mouse grab is implemented by using a full screen input only window
    if (!m_mouseInterceptionWindow.isValid()) {
        const QSize &s = screens()->size();
        const QRect geo(0, 0, s.width(), s.height());
        const uint32_t mask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
        const uint32_t values[] = {
            true,
            XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION
        };
        m_mouseInterceptionWindow.reset(Xcb::createInputWindow(geo, mask, values));
        defineCursor(shape);
    } else {
        defineCursor(shape);
    }
    m_mouseInterceptionWindow.map();
    m_mouseInterceptionWindow.raise();
    m_x11MouseInterception = std::make_unique<EffectsMouseInterceptionX11Filter>(m_mouseInterceptionWindow, this);
    // Raise electric border windows above the input windows
    // so they can still be triggered.
    ScreenEdges::self()->ensureOnTop();
}

void EffectsHandlerImpl::stopMouseInterception(Effect *effect)
{
    if (!m_grabbedMouseEffects.contains(effect)) {
        return;
    }
    m_grabbedMouseEffects.removeAll(effect);
    if (m_grabbedMouseEffects.isEmpty()) {
        if (kwinApp()->operationMode() != Application::OperationModeX11) {
            input()->pointer()->removeEffectsOverrideCursor();
            return;
        }
        m_mouseInterceptionWindow.unmap();
        m_x11MouseInterception.reset();
        Workspace::self()->stackScreenEdgesUnderOverrideRedirect();
    }
}

bool EffectsHandlerImpl::isMouseInterception() const
{
    return m_grabbedMouseEffects.count() > 0;
}


bool EffectsHandlerImpl::touchDown(quint32 id, const QPointF &pos, quint32 time)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->touchDown(id, pos, time)) {
            return true;
        }
    }
    return false;
}

bool EffectsHandlerImpl::touchMotion(quint32 id, const QPointF &pos, quint32 time)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->touchMotion(id, pos, time)) {
            return true;
        }
    }
    return false;
}

bool EffectsHandlerImpl::touchUp(quint32 id, quint32 time)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->touchUp(id, time)) {
            return true;
        }
    }
    return false;
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

void EffectsHandlerImpl::registerTouchpadSwipeShortcut(SwipeDirection direction, QAction *action)
{
    input()->registerTouchpadSwipeShortcut(direction, action);
}

void* EffectsHandlerImpl::getProxy(QString name)
{
    for (QVector< EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it)
        if ((*it).first == name)
            return (*it).second->proxy();

    return NULL;
}

void EffectsHandlerImpl::startMousePolling()
{
    if (Cursor::self())
        Cursor::self()->startMousePolling();
}

void EffectsHandlerImpl::stopMousePolling()
{
    if (Cursor::self())
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
        return m_managedProperties.value(propertyName, XCB_ATOM_NONE);
    }
    m_propertiesForEffects.insert(propertyName, QList<Effect*>() << effect);
    const auto atom = registerSupportProperty(propertyName);
    if (atom == XCB_ATOM_NONE) {
        return atom;
    }
    m_compositor->keepSupportProperty(atom);
    m_managedProperties.insert(propertyName, atom);
    registerPropertyType(atom, true);
    return atom;
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
    if (!kwinApp()->x11Connection()) {
        return QByteArray();
    }
    return readWindowProperty(kwinApp()->x11RootWindow(), atom, type, format);
}

void EffectsHandlerImpl::activateWindow(EffectWindow* c)
{
    if (AbstractClient* cl = dynamic_cast< AbstractClient* >(static_cast<EffectWindowImpl*>(c)->window()))
        Workspace::self()->activateClient(cl, true);
}

EffectWindow* EffectsHandlerImpl::activeWindow() const
{
    return Workspace::self()->activeClient() ? Workspace::self()->activeClient()->effectWindow() : NULL;
}

void EffectsHandlerImpl::moveWindow(EffectWindow* w, const QPoint& pos, bool snap, double snapAdjust)
{
    AbstractClient* cl = dynamic_cast< AbstractClient* >(static_cast<EffectWindowImpl*>(w)->window());
    if (!cl || !cl->isMovable())
        return;

    if (snap)
        cl->move(Workspace::self()->adjustClientPosition(cl, pos, true, snapAdjust));
    else
        cl->move(pos);
}

void EffectsHandlerImpl::windowToDesktop(EffectWindow* w, int desktop)
{
    AbstractClient* cl = dynamic_cast< AbstractClient* >(static_cast<EffectWindowImpl*>(w)->window());
    if (cl && !cl->isDesktop() && !cl->isDock())
        Workspace::self()->sendClientToDesktop(cl, desktop, true);
}

void EffectsHandlerImpl::windowToScreen(EffectWindow* w, int screen)
{
    AbstractClient* cl = dynamic_cast< AbstractClient* >(static_cast<EffectWindowImpl*>(w)->window());
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
    if (!Activities::self()) {
        return QString();
    }
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
    return desktopGridWidth() * screens()->size().width();
}

int EffectsHandlerImpl::workspaceHeight() const
{
    return desktopGridHeight() * screens()->size().height();
}

int EffectsHandlerImpl::desktopAtCoords(QPoint coords) const
{
    if (auto vd = VirtualDesktopManager::self()->grid().at(coords)) {
        return vd->x11DesktopNumber();
    }
    return 0;
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
    const QSize displaySize = screens()->size();
    return QPoint(coords.x() * displaySize.width(), coords.y() * displaySize.height());
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
    if (waylandServer()) {
        if (ShellClient *w = waylandServer()->findClient(id)) {
            return w->effectWindow();
        }
    }
    return NULL;
}

EffectWindow* EffectsHandlerImpl::findWindow(KWayland::Server::SurfaceInterface *surf) const
{
    if (waylandServer()) {
        if (ShellClient *w = waylandServer()->findClient(surf)) {
            return w->effectWindow();
        }
    }
    return nullptr;
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

void EffectsHandlerImpl::setElevatedWindow(KWin::EffectWindow* w, bool set)
{
    elevated_windows.removeAll(w);
    if (set)
        elevated_windows.append(w);
}

void EffectsHandlerImpl::setTabBoxWindow(EffectWindow* w)
{
#ifdef KWIN_BUILD_TABBOX
    if (AbstractClient* c = dynamic_cast< AbstractClient* >(static_cast< EffectWindowImpl* >(w)->window())) {
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
    const auto clients = TabBox::TabBox::self()->currentClientList();
    for (auto c : clients)
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
    if (auto c = TabBox::TabBox::self()->currentClient())
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
    if (const AbstractClient* cl = dynamic_cast< const AbstractClient* >(t))
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
        input()->pointer()->setEffectsOverrideCursor(shape);
        return;
    }
    const xcb_cursor_t c = Cursor::x11Cursor(shape);
    if (c != XCB_CURSOR_NONE) {
        m_mouseInterceptionWindow.defineCursor(c);
    }
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

bool EffectsHandlerImpl::checkInputWindowEvent(QWheelEvent *e)
{
    if (m_grabbedMouseEffects.isEmpty()) {
        return false;
    }
    foreach (Effect *effect, m_grabbedMouseEffects) {
        effect->windowInputMouseEvent(e);
    }
    return true;
}

void EffectsHandlerImpl::connectNotify(const QMetaMethod &signal)
{
    if (signal == QMetaMethod::fromSignal(&EffectsHandler::cursorShapeChanged)) {
        if (!m_trackingCursorChanges) {
            connect(Cursor::self(), &Cursor::cursorChanged, this, &EffectsHandler::cursorShapeChanged);
            Cursor::self()->startCursorTracking();
        }
        ++m_trackingCursorChanges;
    }
    EffectsHandler::connectNotify(signal);
}

void EffectsHandlerImpl::disconnectNotify(const QMetaMethod &signal)
{
    if (signal == QMetaMethod::fromSignal(&EffectsHandler::cursorShapeChanged)) {
        Q_ASSERT(m_trackingCursorChanges > 0);
        if (!--m_trackingCursorChanges) {
            Cursor::self()->stopCursorTracking();
            disconnect(Cursor::self(), &Cursor::cursorChanged, this, &EffectsHandler::cursorShapeChanged);
        }
    }
    EffectsHandler::disconnectNotify(signal);
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
    ScreenEdges::self()->ensureOnTop();
}

QPoint EffectsHandlerImpl::cursorPos() const
{
    return Cursor::pos();
}

void EffectsHandlerImpl::reserveElectricBorder(ElectricBorder border, Effect *effect)
{
    ScreenEdges::self()->reserve(border, effect, "borderActivated");
}

void EffectsHandlerImpl::unreserveElectricBorder(ElectricBorder border, Effect *effect)
{
    ScreenEdges::self()->unreserve(border, effect);
}

void EffectsHandlerImpl::registerTouchBorder(ElectricBorder border, QAction *action)
{
    ScreenEdges::self()->reserveTouch(border, action);
}

void EffectsHandlerImpl::unregisterTouchBorder(ElectricBorder border, QAction *action)
{
    ScreenEdges::self()->unreserveTouch(border, action);
}

unsigned long EffectsHandlerImpl::xrenderBufferPicture()
{
    return m_scene->xrenderBufferPicture();
}

QPainter *EffectsHandlerImpl::scenePainter()
{
    return m_scene->scenePainter();
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
    return m_effectLoader->listOfKnownEffects();
}

bool EffectsHandlerImpl::loadEffect(const QString& name)
{
    makeOpenGLContextCurrent();
    m_compositor->addRepaintFull();

    return m_effectLoader->loadEffect(name);
}

void EffectsHandlerImpl::unloadEffect(const QString& name)
{
    makeOpenGLContextCurrent();
    m_compositor->addRepaintFull();

    for (QMap< int, EffectPair >::iterator it = effect_order.begin(); it != effect_order.end(); ++it) {
        if (it.value().first == name) {
            qCDebug(KWIN_CORE) << "EffectsHandler::unloadEffect : Unloading Effect : " << name;
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
            return;
        }
    }

    qCDebug(KWIN_CORE) << "EffectsHandler::unloadEffect : Effect not loaded : " << name;
}

void EffectsHandlerImpl::reconfigureEffect(const QString& name)
{
    for (QVector< EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it)
        if ((*it).first == name) {
            kwinApp()->config()->reparseConfiguration();
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

    // next checks might require a context
    makeOpenGLContextCurrent();
    m_compositor->addRepaintFull();

    return m_effectLoader->isEffectSupported(name);

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
        m_effectLoader->loadEffect(effectName);
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

KWayland::Server::Display *EffectsHandlerImpl::waylandDisplay() const
{
    if (waylandServer()) {
        return waylandServer()->display();
    }
    return nullptr;
}

EffectFrame* EffectsHandlerImpl::effectFrame(EffectFrameStyle style, bool staticSize, const QPoint& position, Qt::Alignment alignment) const
{
    return new EffectFrameImpl(style, staticSize, position, alignment);
}


QVariant EffectsHandlerImpl::kwinOption(KWinOption kwopt)
{
    switch (kwopt) {
    case CloseButtonCorner:
        // TODO: this could become per window and be derived from the actual position in the deco
        return Decoration::DecorationBridge::self()->settings()->decorationButtonsLeft().contains(KDecoration2::DecorationButtonType::Close) ? Qt::TopLeftCorner : Qt::TopRightCorner;
    case SwitchDesktopOnScreenEdge:
        return ScreenEdges::self()->isDesktopSwitching();
    case SwitchDesktopOnScreenEdgeMovingWindows:
        return ScreenEdges::self()->isDesktopSwitchingMovingClients();
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
            QString support((*it).first + QLatin1String(":\n"));
            const QMetaObject *metaOptions = (*it).second->metaObject();
            for (int i=0; i<metaOptions->propertyCount(); ++i) {
                const QMetaProperty property = metaOptions->property(i);
                if (qstrcmp(property.name(), "objectName") == 0) {
                    continue;
                }
                support += QString::fromUtf8(property.name()) + QLatin1String(": ") + (*it).second->property(property.name()).toString() + QLatin1Char('\n');
            }
            return support;
        }
    }
    return QString();
}


bool EffectsHandlerImpl::isScreenLocked() const
{
    return ScreenLockerWatcher::self()->isLocked();
}

QString EffectsHandlerImpl::debug(const QString& name, const QString& parameter) const
{
    QString internalName = name.toLower();;
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

bool EffectsHandlerImpl::animationsSupported() const
{
    static const QByteArray forceEnvVar = qgetenv("KWIN_EFFECTS_FORCE_ANIMATIONS");
    if (!forceEnvVar.isEmpty()) {
        static const int forceValue = forceEnvVar.toInt();
        return forceValue == 1;
    }
    return m_scene->animationsSupported();
}

void EffectsHandlerImpl::highlightWindows(const QVector<EffectWindow *> &windows)
{
    Effect *e = provides(Effect::HighlightWindows);
    if (!e) {
        return;
    }
    e->perform(Effect::HighlightWindows, QVariantList{QVariant::fromValue(windows)});
}

PlatformCursorImage EffectsHandlerImpl::cursorImage() const
{
    return kwinApp()->platform()->cursorImage();
}

void EffectsHandlerImpl::hideCursor()
{
    kwinApp()->platform()->hideCursor();
}

void EffectsHandlerImpl::showCursor()
{
    kwinApp()->platform()->showCursor();
}

void EffectsHandlerImpl::startInteractiveWindowSelection(std::function<void(KWin::EffectWindow*)> callback)
{
    kwinApp()->platform()->startInteractiveWindowSelection(
        [callback] (KWin::Toplevel *t) {
            if (t && t->effectWindow()) {
                callback(t->effectWindow());
            } else {
                callback(nullptr);
            }
        }
    );
}

void EffectsHandlerImpl::startInteractivePositionSelection(std::function<void(const QPoint&)> callback)
{
    kwinApp()->platform()->startInteractivePositionSelection(callback);
}

void EffectsHandlerImpl::showOnScreenMessage(const QString &message, const QString &iconName)
{
    OSD::show(message, iconName);
}

void EffectsHandlerImpl::hideOnScreenMessage(OnScreenMessageHideFlags flags)
{
    OSD::HideFlags osdFlags;
    if (flags.testFlag(OnScreenMessageHideFlag::SkipsCloseAnimation)) {
        osdFlags |= OSD::HideFlag::SkipCloseAnimation;
    }
    OSD::hide(osdFlags);
}

KSharedConfigPtr EffectsHandlerImpl::config() const
{
    return kwinApp()->config();
}

KSharedConfigPtr EffectsHandlerImpl::inputConfig() const
{
    return kwinApp()->inputConfig();
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
    if (!kwinApp()->x11Connection()) {
        return QByteArray();
    }
    return readWindowProperty(window()->window(), atom, type, format);
}

void EffectWindowImpl::deleteProperty(long int atom) const
{
    if (kwinApp()->x11Connection()) {
        deleteWindowProperty(window()->window(), atom);
    }
}

EffectWindow* EffectWindowImpl::findModal()
{
    if (AbstractClient* c = dynamic_cast< AbstractClient* >(toplevel)) {
        if (AbstractClient* c2 = c->findModal())
            return c2->effectWindow();
    }
    return NULL;
}

template <typename T>
EffectWindowList getMainWindows(Toplevel *toplevel)
{
    T *c = static_cast<T*>(toplevel);
    EffectWindowList ret;
    const auto mainclients = c->mainClients();
    for (auto tmp : mainclients)
        ret.append(tmp->effectWindow());
    return ret;
}

EffectWindowList EffectWindowImpl::mainWindows() const
{
    if (dynamic_cast<AbstractClient*>(toplevel)) {
        return getMainWindows<AbstractClient>(toplevel);
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
    emit effects->windowDataChanged(this, role);
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
    setScreenProjectionMatrix(static_cast<EffectsHandlerImpl*>(effects)->scene()->screenProjectionMatrix());
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
