/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "libkwineffects/effects.h"

#include "config-kwin.h"

#include "compositor.h"
#include "core/output.h"
#include "core/renderbackend.h"
#include "core/renderlayer.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "decorations/decorationbridge.h"
#include "effectsadaptor.h"
#include "group.h"
#include "input.h"
#include "input_event.h"
#include "inputmethod.h"
#include "inputpanelv1window.h"
#include "internalwindow.h"
#include "libkwineffects/effectloader.h"
#include "libkwineffects/offscreenquickview.h"
#include "opengl/glutils.h"
#include "osd.h"
#include "pointer_input.h"
#include "scene/itemrenderer.h"
#include "scene/windowitem.h"
#include "scene/workspacescene.h"
#include "screenedge.h"
#include "scripting/scripting.h"
#include "sm.h"
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "waylandwindow.h"
#include "window_property_notify_x11_filter.h"
#include "workspace.h"
#include "x11window.h"

#if KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#if KWIN_BUILD_TABBOX
#include "tabbox/tabbox.h"
#endif
#if KWIN_BUILD_SCREENLOCKER
#include "screenlockerwatcher.h"
#endif

#include <KDecoration2/Decoration>
#include <KDecoration2/DecorationSettings>

#include <QFontMetrics>
#include <QMatrix4x4>
#include <QPainter>
#include <QPixmap>
#include <QTimeLine>
#include <QVariant>
#include <QWindow>
#include <QtMath>

#include <optional>

namespace KWin
{

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

static void deleteWindowProperty(xcb_window_t win, long int atom)
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
    UniqueCPtr<xcb_intern_atom_reply_t> atomReply(xcb_intern_atom_reply(c,
                                                                        xcb_intern_atom_unchecked(c, false, propertyName.size(), propertyName.constData()),
                                                                        nullptr));
    if (!atomReply) {
        return XCB_ATOM_NONE;
    }
    // announce property on root window
    unsigned char dummy = 0;
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, kwinApp()->x11RootWindow(), atomReply->atom, atomReply->atom, 8, 1, &dummy);
    // TODO: add to _NET_SUPPORTED
    return atomReply->atom;
}

//****************************************
// EffectsHandler
//****************************************

EffectsHandler::EffectsHandler(Compositor *compositor, WorkspaceScene *scene)
    : keyboard_grab_effect(nullptr)
    , fullscreen_effect(nullptr)
    , compositing_type(compositor->backend()->compositingType())
    , m_compositor(compositor)
    , m_scene(scene)
    , m_effectLoader(new EffectLoader(this))
    , m_trackingCursorChanges(0)
{
    if (compositing_type == NoCompositing) {
        return;
    }
    KWin::effects = this;

    qRegisterMetaType<QList<KWin::EffectWindow *>>();
    qRegisterMetaType<KWin::SessionState>();
    connect(m_effectLoader, &AbstractEffectLoader::effectLoaded, this, [this](Effect *effect, const QString &name) {
        effect_order.insert(effect->requestedEffectChainPosition(), EffectPair(name, effect));
        loaded_effects << EffectPair(name, effect);
        effectsChanged();
    });
    m_effectLoader->setConfig(kwinApp()->config());
    new EffectsAdaptor(this);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject(QStringLiteral("/Effects"), this);

    connect(options, &Options::animationSpeedChanged, this, &EffectsHandler::reconfigureEffects);

    Workspace *ws = Workspace::self();
    VirtualDesktopManager *vds = VirtualDesktopManager::self();
    connect(ws, &Workspace::showingDesktopChanged, this, [this](bool showing, bool animated) {
        if (animated) {
            Q_EMIT showingDesktopChanged(showing);
        }
    });
    connect(ws, &Workspace::currentDesktopChanged, this, [this](VirtualDesktop *old, Window *window) {
        VirtualDesktop *newDesktop = VirtualDesktopManager::self()->currentDesktop();
        Q_EMIT desktopChanged(old, newDesktop, window ? window->effectWindow() : nullptr);
    });
    connect(ws, &Workspace::currentDesktopChanging, this, [this](VirtualDesktop *currentDesktop, QPointF offset, KWin::Window *window) {
        Q_EMIT desktopChanging(currentDesktop, offset, window ? window->effectWindow() : nullptr);
    });
    connect(ws, &Workspace::currentDesktopChangingCancelled, this, [this]() {
        Q_EMIT desktopChangingCancelled();
    });
    connect(ws, &Workspace::windowAdded, this, [this](Window *window) {
        setupWindowConnections(window);
        Q_EMIT windowAdded(window->effectWindow());
    });
    connect(ws, &Workspace::windowActivated, this, [this](Window *window) {
        Q_EMIT windowActivated(window ? window->effectWindow() : nullptr);
    });
    connect(ws, &Workspace::deletedRemoved, this, [this](KWin::Window *d) {
        Q_EMIT windowDeleted(d->effectWindow());
    });
    connect(ws->sessionManager(), &SessionManager::stateChanged, this, &KWin::EffectsHandler::sessionStateChanged);
    connect(vds, &VirtualDesktopManager::layoutChanged, this, [this](int width, int height) {
        Q_EMIT desktopGridSizeChanged(QSize(width, height));
        Q_EMIT desktopGridWidthChanged(width);
        Q_EMIT desktopGridHeightChanged(height);
    });
    connect(vds, &VirtualDesktopManager::desktopAdded, this, &EffectsHandler::desktopAdded);
    connect(vds, &VirtualDesktopManager::desktopRemoved, this, &EffectsHandler::desktopRemoved);
    connect(Cursors::self()->mouse(), &Cursor::mouseChanged, this, &EffectsHandler::mouseChanged);
    connect(ws, &Workspace::geometryChanged, this, &EffectsHandler::virtualScreenSizeChanged);
    connect(ws, &Workspace::geometryChanged, this, &EffectsHandler::virtualScreenGeometryChanged);
#if KWIN_BUILD_ACTIVITIES
    if (Activities *activities = Workspace::self()->activities()) {
        connect(activities, &Activities::added, this, &EffectsHandler::activityAdded);
        connect(activities, &Activities::removed, this, &EffectsHandler::activityRemoved);
        connect(activities, &Activities::currentChanged, this, &EffectsHandler::currentActivityChanged);
    }
#endif
    connect(ws, &Workspace::stackingOrderChanged, this, &EffectsHandler::stackingOrderChanged);
#if KWIN_BUILD_TABBOX
    TabBox::TabBox *tabBox = workspace()->tabbox();
    connect(tabBox, &TabBox::TabBox::tabBoxAdded, this, &EffectsHandler::tabBoxAdded);
    connect(tabBox, &TabBox::TabBox::tabBoxUpdated, this, &EffectsHandler::tabBoxUpdated);
    connect(tabBox, &TabBox::TabBox::tabBoxClosed, this, &EffectsHandler::tabBoxClosed);
    connect(tabBox, &TabBox::TabBox::tabBoxKeyEvent, this, &EffectsHandler::tabBoxKeyEvent);
#endif
    connect(workspace()->screenEdges(), &ScreenEdges::approaching, this, &EffectsHandler::screenEdgeApproaching);
#if KWIN_BUILD_SCREENLOCKER
    connect(kwinApp()->screenLockerWatcher(), &ScreenLockerWatcher::locked, this, &EffectsHandler::screenLockingChanged);
    connect(kwinApp()->screenLockerWatcher(), &ScreenLockerWatcher::aboutToLock, this, &EffectsHandler::screenAboutToLock);
#endif

    connect(kwinApp(), &Application::x11ConnectionChanged, this, [this]() {
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
        Q_EMIT xcbConnectionChanged();
    });

    if (kwinApp()->x11Connection()) {
        m_x11WindowPropertyNotify = std::make_unique<WindowPropertyNotifyX11Filter>(this);
    }

    // connect all clients
    for (Window *window : ws->windows()) {
        setupWindowConnections(window);
    }

    connect(ws, &Workspace::outputAdded, this, &EffectsHandler::screenAdded);
    connect(ws, &Workspace::outputRemoved, this, &EffectsHandler::screenRemoved);

    if (auto inputMethod = kwinApp()->inputMethod()) {
        connect(inputMethod, &InputMethod::panelChanged, this, &EffectsHandler::inputPanelChanged);
    }

    reconfigure();
}

EffectsHandler::~EffectsHandler()
{
    unloadAllEffects();
    KWin::effects = nullptr;
}

xcb_window_t EffectsHandler::x11RootWindow() const
{
    return kwinApp()->x11RootWindow();
}

xcb_connection_t *EffectsHandler::xcbConnection() const
{
    return kwinApp()->x11Connection();
}

CompositingType EffectsHandler::compositingType() const
{
    return compositing_type;
}

bool EffectsHandler::isOpenGLCompositing() const
{
    return compositing_type & OpenGLCompositing;
}

void EffectsHandler::unloadAllEffects()
{
    for (const EffectPair &pair : std::as_const(loaded_effects)) {
        destroyEffect(pair.second);
    }

    effect_order.clear();
    m_effectLoader->clear();

    effectsChanged();
}

void EffectsHandler::setupWindowConnections(Window *window)
{
    connect(window, &Window::closed, this, [this, window]() {
        if (window->effectWindow()) {
            Q_EMIT windowClosed(window->effectWindow());
        }
    });
}

void EffectsHandler::reconfigure()
{
    m_effectLoader->queryAndLoadAll();
}

// the idea is that effects call this function again which calls the next one
void EffectsHandler::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (m_currentPaintScreenIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintScreenIterator++)->prePaintScreen(data, presentTime);
        --m_currentPaintScreenIterator;
    }
    // no special final code
}

void EffectsHandler::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, Output *screen)
{
    if (m_currentPaintScreenIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintScreenIterator++)->paintScreen(renderTarget, viewport, mask, region, screen);
        --m_currentPaintScreenIterator;
    } else {
        m_scene->finalPaintScreen(renderTarget, viewport, mask, region, screen);
    }
}

void EffectsHandler::postPaintScreen()
{
    if (m_currentPaintScreenIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintScreenIterator++)->postPaintScreen();
        --m_currentPaintScreenIterator;
    }
    // no special final code
}

void EffectsHandler::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (m_currentPaintWindowIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintWindowIterator++)->prePaintWindow(w, data, presentTime);
        --m_currentPaintWindowIterator;
    }
    // no special final code
}

void EffectsHandler::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data)
{
    if (m_currentPaintWindowIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintWindowIterator++)->paintWindow(renderTarget, viewport, w, mask, region, data);
        --m_currentPaintWindowIterator;
    } else {
        m_scene->finalPaintWindow(renderTarget, viewport, w, mask, region, data);
    }
}

void EffectsHandler::postPaintWindow(EffectWindow *w)
{
    if (m_currentPaintWindowIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintWindowIterator++)->postPaintWindow(w);
        --m_currentPaintWindowIterator;
    }
    // no special final code
}

Effect *EffectsHandler::provides(Effect::Feature ef)
{
    for (int i = 0; i < loaded_effects.size(); ++i) {
        if (loaded_effects.at(i).second->provides(ef)) {
            return loaded_effects.at(i).second;
        }
    }
    return nullptr;
}

void EffectsHandler::drawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data)
{
    if (m_currentDrawWindowIterator != m_activeEffects.constEnd()) {
        (*m_currentDrawWindowIterator++)->drawWindow(renderTarget, viewport, w, mask, region, data);
        --m_currentDrawWindowIterator;
    } else {
        m_scene->finalDrawWindow(renderTarget, viewport, w, mask, region, data);
    }
}

void EffectsHandler::renderWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data)
{
    m_scene->finalDrawWindow(renderTarget, viewport, w, mask, region, data);
}

bool EffectsHandler::hasDecorationShadows() const
{
    return false;
}

bool EffectsHandler::decorationsHaveAlpha() const
{
    return true;
}

// start another painting pass
void EffectsHandler::startPaint()
{
    m_activeEffects.clear();
    m_activeEffects.reserve(loaded_effects.count());
    for (QList<KWin::EffectPair>::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->isActive()) {
            m_activeEffects << it->second;
        }
    }
    m_currentDrawWindowIterator = m_activeEffects.constBegin();
    m_currentPaintWindowIterator = m_activeEffects.constBegin();
    m_currentPaintScreenIterator = m_activeEffects.constBegin();
}

void EffectsHandler::setActiveFullScreenEffect(Effect *e)
{
    if (fullscreen_effect == e) {
        return;
    }
    const bool activeChanged = (e == nullptr || fullscreen_effect == nullptr);
    fullscreen_effect = e;
    Q_EMIT activeFullScreenEffectChanged();
    if (activeChanged) {
        Q_EMIT hasActiveFullScreenEffectChanged();
        workspace()->screenEdges()->checkBlocking();
    }
}

Effect *EffectsHandler::activeFullScreenEffect() const
{
    return fullscreen_effect;
}

bool EffectsHandler::hasActiveFullScreenEffect() const
{
    return fullscreen_effect;
}

bool EffectsHandler::grabKeyboard(Effect *effect)
{
    if (keyboard_grab_effect != nullptr) {
        return false;
    }
    if (!doGrabKeyboard()) {
        return false;
    }
    keyboard_grab_effect = effect;
    return true;
}

bool EffectsHandler::doGrabKeyboard()
{
    return true;
}

void EffectsHandler::ungrabKeyboard()
{
    Q_ASSERT(keyboard_grab_effect != nullptr);
    doUngrabKeyboard();
    keyboard_grab_effect = nullptr;
}

void EffectsHandler::doUngrabKeyboard()
{
}

void EffectsHandler::grabbedKeyboardEvent(QKeyEvent *e)
{
    if (keyboard_grab_effect != nullptr) {
        keyboard_grab_effect->grabbedKeyboardEvent(e);
    }
}

void EffectsHandler::startMouseInterception(Effect *effect, Qt::CursorShape shape)
{
    if (m_grabbedMouseEffects.contains(effect)) {
        return;
    }
    m_grabbedMouseEffects.append(effect);
    if (m_grabbedMouseEffects.size() != 1) {
        return;
    }
    doStartMouseInterception(shape);
}

void EffectsHandler::doStartMouseInterception(Qt::CursorShape shape)
{
    input()->pointer()->setEffectsOverrideCursor(shape);

    // We want to allow global shortcuts to be triggered when moving a
    // window so it is possible to pick up a window and then move it to a
    // different desktop by using the global shortcut to switch desktop.
    // However, that means that some other things can also be triggered. If
    // an effect that fill the screen gets triggered that way, we end up in a
    // weird state where the move will restart after the effect closes. So to
    // avoid that, abort move/resize if a full screen effect starts.
    if (workspace()->moveResizeWindow()) {
        workspace()->moveResizeWindow()->endInteractiveMoveResize();
    }
}

void EffectsHandler::stopMouseInterception(Effect *effect)
{
    if (!m_grabbedMouseEffects.contains(effect)) {
        return;
    }
    m_grabbedMouseEffects.removeAll(effect);
    if (m_grabbedMouseEffects.isEmpty()) {
        doStopMouseInterception();
    }
}

void EffectsHandler::doStopMouseInterception()
{
    input()->pointer()->removeEffectsOverrideCursor();
}

bool EffectsHandler::isMouseInterception() const
{
    return m_grabbedMouseEffects.count() > 0;
}

bool EffectsHandler::touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->touchDown(id, pos, time)) {
            return true;
        }
    }
    return false;
}

bool EffectsHandler::touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->touchMotion(id, pos, time)) {
            return true;
        }
    }
    return false;
}

bool EffectsHandler::touchUp(qint32 id, std::chrono::microseconds time)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->touchUp(id, time)) {
            return true;
        }
    }
    return false;
}

bool EffectsHandler::tabletToolEvent(TabletEvent *event)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->tabletToolEvent(event)) {
            return true;
        }
    }
    return false;
}

bool EffectsHandler::tabletToolButtonEvent(uint button, bool pressed, const TabletToolId &tabletToolId, std::chrono::microseconds time)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->tabletToolButtonEvent(button, pressed, tabletToolId.m_uniqueId)) {
            return true;
        }
    }
    return false;
}

bool EffectsHandler::tabletPadButtonEvent(uint button, bool pressed, const TabletPadId &tabletPadId, std::chrono::microseconds time)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->tabletPadButtonEvent(button, pressed, tabletPadId.data)) {
            return true;
        }
    }
    return false;
}

bool EffectsHandler::tabletPadStripEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->tabletPadStripEvent(number, position, isFinger, tabletPadId.data)) {
            return true;
        }
    }
    return false;
}

bool EffectsHandler::tabletPadRingEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->tabletPadRingEvent(number, position, isFinger, tabletPadId.data)) {
            return true;
        }
    }
    return false;
}

void EffectsHandler::registerPointerShortcut(Qt::KeyboardModifiers modifiers, Qt::MouseButton pointerButtons, QAction *action)
{
    input()->registerPointerShortcut(modifiers, pointerButtons, action);
}

void EffectsHandler::registerAxisShortcut(Qt::KeyboardModifiers modifiers, PointerAxisDirection axis, QAction *action)
{
    input()->registerAxisShortcut(modifiers, axis, action);
}

void EffectsHandler::registerTouchpadSwipeShortcut(SwipeDirection dir, uint fingerCount, QAction *onUp, std::function<void(qreal)> progressCallback)
{
    input()->registerTouchpadSwipeShortcut(dir, fingerCount, onUp, progressCallback);
}

void EffectsHandler::registerTouchpadPinchShortcut(PinchDirection dir, uint fingerCount, QAction *onUp, std::function<void(qreal)> progressCallback)
{
    input()->registerTouchpadPinchShortcut(dir, fingerCount, onUp, progressCallback);
}

void EffectsHandler::registerTouchscreenSwipeShortcut(SwipeDirection direction, uint fingerCount, QAction *action, std::function<void(qreal)> progressCallback)
{
    input()->registerTouchscreenSwipeShortcut(direction, fingerCount, action, progressCallback);
}

void EffectsHandler::startMousePolling()
{
    if (Cursors::self()->mouse()) {
        Cursors::self()->mouse()->startMousePolling();
    }
}

void EffectsHandler::stopMousePolling()
{
    if (Cursors::self()->mouse()) {
        Cursors::self()->mouse()->stopMousePolling();
    }
}

bool EffectsHandler::hasKeyboardGrab() const
{
    return keyboard_grab_effect != nullptr;
}

void EffectsHandler::registerPropertyType(long atom, bool reg)
{
    if (reg) {
        ++registered_atoms[atom]; // initialized to 0 if not present yet
    } else {
        if (--registered_atoms[atom] == 0) {
            registered_atoms.remove(atom);
        }
    }
}

xcb_atom_t EffectsHandler::announceSupportProperty(const QByteArray &propertyName, Effect *effect)
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
    m_propertiesForEffects.insert(propertyName, QList<Effect *>() << effect);
    const auto atom = registerSupportProperty(propertyName);
    if (atom == XCB_ATOM_NONE) {
        return atom;
    }
    m_compositor->keepSupportProperty(atom);
    m_managedProperties.insert(propertyName, atom);
    registerPropertyType(atom, true);
    return atom;
}

void EffectsHandler::removeSupportProperty(const QByteArray &propertyName, Effect *effect)
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

QByteArray EffectsHandler::readRootProperty(long atom, long type, int format) const
{
    if (!kwinApp()->x11Connection()) {
        return QByteArray();
    }
    return readWindowProperty(kwinApp()->x11RootWindow(), atom, type, format);
}

void EffectsHandler::activateWindow(EffectWindow *effectWindow)
{
    auto window = effectWindow->window();
    if (window->isClient()) {
        Workspace::self()->activateWindow(window, true);
    }
}

EffectWindow *EffectsHandler::activeWindow() const
{
    return Workspace::self()->activeWindow() ? Workspace::self()->activeWindow()->effectWindow() : nullptr;
}

void EffectsHandler::moveWindow(EffectWindow *w, const QPoint &pos, bool snap, double snapAdjust)
{
    auto window = w->window();
    if (!window->isClient() || !window->isMovable()) {
        return;
    }

    if (snap) {
        window->move(Workspace::self()->adjustWindowPosition(window, pos, true, snapAdjust));
    } else {
        window->move(pos);
    }
}

void EffectsHandler::windowToDesktops(EffectWindow *w, const QList<VirtualDesktop *> &desktops)
{
    auto window = w->window();
    if (!window->isClient() || window->isDesktop() || window->isDock()) {
        return;
    }
    window->setDesktops(desktops);
}

void EffectsHandler::windowToScreen(EffectWindow *w, Output *screen)
{
    auto window = w->window();
    if (window->isClient() && !window->isDesktop() && !window->isDock()) {
        Workspace::self()->sendWindowToOutput(window, screen);
    }
}

void EffectsHandler::setShowingDesktop(bool showing)
{
    Workspace::self()->setShowingDesktop(showing);
}

QString EffectsHandler::currentActivity() const
{
#if KWIN_BUILD_ACTIVITIES
    if (!Workspace::self()->activities()) {
        return QString();
    }
    return Workspace::self()->activities()->current();
#else
    return QString();
#endif
}

VirtualDesktop *EffectsHandler::currentDesktop() const
{
    return VirtualDesktopManager::self()->currentDesktop();
}

QList<VirtualDesktop *> EffectsHandler::desktops() const
{
    return VirtualDesktopManager::self()->desktops();
}

void EffectsHandler::setCurrentDesktop(VirtualDesktop *desktop)
{
    VirtualDesktopManager::self()->setCurrent(desktop);
}

QSize EffectsHandler::desktopGridSize() const
{
    return VirtualDesktopManager::self()->grid().size();
}

int EffectsHandler::desktopGridWidth() const
{
    return desktopGridSize().width();
}

int EffectsHandler::desktopGridHeight() const
{
    return desktopGridSize().height();
}

int EffectsHandler::workspaceWidth() const
{
    return desktopGridWidth() * Workspace::self()->geometry().width();
}

int EffectsHandler::workspaceHeight() const
{
    return desktopGridHeight() * Workspace::self()->geometry().height();
}

VirtualDesktop *EffectsHandler::desktopAtCoords(QPoint coords) const
{
    return VirtualDesktopManager::self()->grid().at(coords);
}

QPoint EffectsHandler::desktopGridCoords(VirtualDesktop *desktop) const
{
    return VirtualDesktopManager::self()->grid().gridCoords(desktop);
}

QPoint EffectsHandler::desktopCoords(VirtualDesktop *desktop) const
{
    QPoint coords = VirtualDesktopManager::self()->grid().gridCoords(desktop);
    if (coords.x() == -1) {
        return QPoint(-1, -1);
    }
    const QSize displaySize = Workspace::self()->geometry().size();
    return QPoint(coords.x() * displaySize.width(), coords.y() * displaySize.height());
}

VirtualDesktop *EffectsHandler::desktopAbove(VirtualDesktop *desktop, bool wrap) const
{
    return VirtualDesktopManager::self()->inDirection(desktop, VirtualDesktopManager::Direction::Up, wrap);
}

VirtualDesktop *EffectsHandler::desktopToRight(VirtualDesktop *desktop, bool wrap) const
{
    return VirtualDesktopManager::self()->inDirection(desktop, VirtualDesktopManager::Direction::Right, wrap);
}

VirtualDesktop *EffectsHandler::desktopBelow(VirtualDesktop *desktop, bool wrap) const
{
    return VirtualDesktopManager::self()->inDirection(desktop, VirtualDesktopManager::Direction::Down, wrap);
}

VirtualDesktop *EffectsHandler::desktopToLeft(VirtualDesktop *desktop, bool wrap) const
{
    return VirtualDesktopManager::self()->inDirection(desktop, VirtualDesktopManager::Direction::Left, wrap);
}

QString EffectsHandler::desktopName(VirtualDesktop *desktop) const
{
    return desktop->name();
}

bool EffectsHandler::optionRollOverDesktops() const
{
    return options->isRollOverDesktops();
}

double EffectsHandler::animationTimeFactor() const
{
    return options->animationTimeFactor();
}

EffectWindow *EffectsHandler::findWindow(WId id) const
{
    if (X11Window *w = Workspace::self()->findClient(Predicate::WindowMatch, id)) {
        return w->effectWindow();
    }
    if (X11Window *w = Workspace::self()->findUnmanaged(id)) {
        return w->effectWindow();
    }
    return nullptr;
}

EffectWindow *EffectsHandler::findWindow(SurfaceInterface *surf) const
{
    if (waylandServer()) {
        if (Window *w = waylandServer()->findWindow(surf)) {
            return w->effectWindow();
        }
    }
    return nullptr;
}

EffectWindow *EffectsHandler::findWindow(QWindow *w) const
{
    if (Window *window = workspace()->findInternal(w)) {
        return window->effectWindow();
    }
    return nullptr;
}

EffectWindow *EffectsHandler::findWindow(const QUuid &id) const
{
    if (Window *window = workspace()->findWindow(id)) {
        return window->effectWindow();
    }
    return nullptr;
}

EffectWindowList EffectsHandler::stackingOrder() const
{
    QList<Window *> list = workspace()->stackingOrder();
    EffectWindowList ret;
    for (Window *t : list) {
        if (EffectWindow *w = t->effectWindow()) {
            ret.append(w);
        }
    }
    return ret;
}

void EffectsHandler::setElevatedWindow(KWin::EffectWindow *w, bool set)
{
    WindowItem *item = w->windowItem();

    if (set) {
        item->elevate();
    } else {
        item->deelevate();
    }
}

void EffectsHandler::setTabBoxWindow(EffectWindow *w)
{
#if KWIN_BUILD_TABBOX
    auto window = w->window();
    if (window->isClient()) {
        workspace()->tabbox()->setCurrentClient(window);
    }
#endif
}

EffectWindowList EffectsHandler::currentTabBoxWindowList() const
{
#if KWIN_BUILD_TABBOX
    const auto clients = workspace()->tabbox()->currentClientList();
    EffectWindowList ret;
    ret.reserve(clients.size());
    std::transform(std::cbegin(clients), std::cend(clients),
                   std::back_inserter(ret),
                   [](auto client) {
                       return client->effectWindow();
                   });
    return ret;
#else
    return EffectWindowList();
#endif
}

void EffectsHandler::refTabBox()
{
#if KWIN_BUILD_TABBOX
    workspace()->tabbox()->reference();
#endif
}

void EffectsHandler::unrefTabBox()
{
#if KWIN_BUILD_TABBOX
    workspace()->tabbox()->unreference();
#endif
}

void EffectsHandler::closeTabBox()
{
#if KWIN_BUILD_TABBOX
    workspace()->tabbox()->close();
#endif
}

EffectWindow *EffectsHandler::currentTabBoxWindow() const
{
#if KWIN_BUILD_TABBOX
    if (auto c = workspace()->tabbox()->currentClient()) {
        return c->effectWindow();
    }
#endif
    return nullptr;
}

void EffectsHandler::addRepaintFull()
{
    m_compositor->scene()->addRepaintFull();
}

void EffectsHandler::addRepaint(const QRect &r)
{
    m_compositor->scene()->addRepaint(r);
}

void EffectsHandler::addRepaint(const QRectF &r)
{
    m_compositor->scene()->addRepaint(r.toAlignedRect());
}

void EffectsHandler::addRepaint(const QRegion &r)
{
    m_compositor->scene()->addRepaint(r);
}

void EffectsHandler::addRepaint(int x, int y, int w, int h)
{
    m_compositor->scene()->addRepaint(x, y, w, h);
}

Output *EffectsHandler::activeScreen() const
{
    return workspace()->activeOutput();
}

QRectF EffectsHandler::clientArea(clientAreaOption opt, const Output *screen, const VirtualDesktop *desktop) const
{
    return Workspace::self()->clientArea(opt, screen, desktop);
}

QRectF EffectsHandler::clientArea(clientAreaOption opt, const EffectWindow *effectWindow) const
{
    const Window *window = effectWindow->window();
    return Workspace::self()->clientArea(opt, window);
}

QRectF EffectsHandler::clientArea(clientAreaOption opt, const QPoint &p, const VirtualDesktop *desktop) const
{
    const Output *output = Workspace::self()->outputAt(p);
    return Workspace::self()->clientArea(opt, output, desktop);
}

QRect EffectsHandler::virtualScreenGeometry() const
{
    return Workspace::self()->geometry();
}

QSize EffectsHandler::virtualScreenSize() const
{
    return Workspace::self()->geometry().size();
}

void EffectsHandler::defineCursor(Qt::CursorShape shape)
{
    input()->pointer()->setEffectsOverrideCursor(shape);
}

bool EffectsHandler::checkInputWindowEvent(QMouseEvent *e)
{
    if (m_grabbedMouseEffects.isEmpty()) {
        return false;
    }
    for (Effect *effect : std::as_const(m_grabbedMouseEffects)) {
        effect->windowInputMouseEvent(e);
    }
    return true;
}

bool EffectsHandler::checkInputWindowEvent(QWheelEvent *e)
{
    if (m_grabbedMouseEffects.isEmpty()) {
        return false;
    }
    for (Effect *effect : std::as_const(m_grabbedMouseEffects)) {
        effect->windowInputMouseEvent(e);
    }
    return true;
}

void EffectsHandler::connectNotify(const QMetaMethod &signal)
{
    if (signal == QMetaMethod::fromSignal(&EffectsHandler::cursorShapeChanged)) {
        if (!m_trackingCursorChanges) {
            connect(Cursors::self()->mouse(), &Cursor::cursorChanged, this, &EffectsHandler::cursorShapeChanged);
            Cursors::self()->mouse()->startCursorTracking();
        }
        ++m_trackingCursorChanges;
    }
    QObject::connectNotify(signal);
}

void EffectsHandler::disconnectNotify(const QMetaMethod &signal)
{
    if (signal == QMetaMethod::fromSignal(&EffectsHandler::cursorShapeChanged)) {
        Q_ASSERT(m_trackingCursorChanges > 0);
        if (!--m_trackingCursorChanges) {
            Cursors::self()->mouse()->stopCursorTracking();
            disconnect(Cursors::self()->mouse(), &Cursor::cursorChanged, this, &EffectsHandler::cursorShapeChanged);
        }
    }
    QObject::disconnectNotify(signal);
}

void EffectsHandler::checkInputWindowStacking()
{
    if (m_grabbedMouseEffects.isEmpty()) {
        return;
    }
    doCheckInputWindowStacking();
}

void EffectsHandler::doCheckInputWindowStacking()
{
}

QPointF EffectsHandler::cursorPos() const
{
    return Cursors::self()->mouse()->pos();
}

void EffectsHandler::reserveElectricBorder(ElectricBorder border, Effect *effect)
{
    workspace()->screenEdges()->reserve(border, effect, "borderActivated");
}

void EffectsHandler::unreserveElectricBorder(ElectricBorder border, Effect *effect)
{
    workspace()->screenEdges()->unreserve(border, effect);
}

void EffectsHandler::registerTouchBorder(ElectricBorder border, QAction *action)
{
    workspace()->screenEdges()->reserveTouch(border, action);
}

void EffectsHandler::registerRealtimeTouchBorder(ElectricBorder border, QAction *action, EffectsHandler::TouchBorderCallback progressCallback)
{
    workspace()->screenEdges()->reserveTouch(border, action, progressCallback);
}

void EffectsHandler::unregisterTouchBorder(ElectricBorder border, QAction *action)
{
    workspace()->screenEdges()->unreserveTouch(border, action);
}

QPainter *EffectsHandler::scenePainter()
{
    return m_scene->renderer()->painter();
}

void EffectsHandler::toggleEffect(const QString &name)
{
    if (isEffectLoaded(name)) {
        unloadEffect(name);
    } else {
        loadEffect(name);
    }
}

QStringList EffectsHandler::loadedEffects() const
{
    QStringList listModules;
    listModules.reserve(loaded_effects.count());
    std::transform(loaded_effects.constBegin(), loaded_effects.constEnd(),
                   std::back_inserter(listModules),
                   [](const EffectPair &pair) {
                       return pair.first;
                   });
    return listModules;
}

QStringList EffectsHandler::listOfEffects() const
{
    return m_effectLoader->listOfKnownEffects();
}

bool EffectsHandler::loadEffect(const QString &name)
{
    makeOpenGLContextCurrent();
    m_compositor->scene()->addRepaintFull();

    return m_effectLoader->loadEffect(name);
}

void EffectsHandler::unloadEffect(const QString &name)
{
    auto it = std::find_if(effect_order.begin(), effect_order.end(),
                           [name](EffectPair &pair) {
                               return pair.first == name;
                           });
    if (it == effect_order.end()) {
        qCDebug(KWIN_CORE) << "EffectsHandler::unloadEffect : Effect not loaded :" << name;
        return;
    }

    qCDebug(KWIN_CORE) << "EffectsHandler::unloadEffect : Unloading Effect :" << name;
    destroyEffect((*it).second);
    effect_order.erase(it);
    effectsChanged();

    m_compositor->scene()->addRepaintFull();
}

void EffectsHandler::destroyEffect(Effect *effect)
{
    makeOpenGLContextCurrent();

    if (fullscreen_effect == effect) {
        setActiveFullScreenEffect(nullptr);
    }

    if (keyboard_grab_effect == effect) {
        ungrabKeyboard();
    }

    stopMouseInterception(effect);

    const QList<QByteArray> properties = m_propertiesForEffects.keys();
    for (const QByteArray &property : properties) {
        removeSupportProperty(property, effect);
    }

    delete effect;
}

void EffectsHandler::reconfigureEffects()
{
    makeOpenGLContextCurrent();
    for (const EffectPair &pair : loaded_effects) {
        pair.second->reconfigure(Effect::ReconfigureAll);
    }
}

void EffectsHandler::reconfigureEffect(const QString &name)
{
    for (QList<EffectPair>::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if ((*it).first == name) {
            kwinApp()->config()->reparseConfiguration();
            makeOpenGLContextCurrent();
            (*it).second->reconfigure(Effect::ReconfigureAll);
            return;
        }
    }
}

bool EffectsHandler::isEffectLoaded(const QString &name) const
{
    auto it = std::find_if(loaded_effects.constBegin(), loaded_effects.constEnd(),
                           [&name](const EffectPair &pair) {
                               return pair.first == name;
                           });
    return it != loaded_effects.constEnd();
}

bool EffectsHandler::isEffectSupported(const QString &name)
{
    // If the effect is loaded, it is obviously supported.
    if (isEffectLoaded(name)) {
        return true;
    }

    // next checks might require a context
    makeOpenGLContextCurrent();

    return m_effectLoader->isEffectSupported(name);
}

QList<bool> EffectsHandler::areEffectsSupported(const QStringList &names)
{
    QList<bool> retList;
    retList.reserve(names.count());
    std::transform(names.constBegin(), names.constEnd(),
                   std::back_inserter(retList),
                   [this](const QString &name) {
                       return isEffectSupported(name);
                   });
    return retList;
}

void EffectsHandler::reloadEffect(Effect *effect)
{
    QString effectName;
    for (QList<EffectPair>::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
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

void EffectsHandler::effectsChanged()
{
    loaded_effects.clear();
    m_activeEffects.clear(); // it's possible to have a reconfigure and a quad rebuild between two paint cycles - bug #308201

    loaded_effects.reserve(effect_order.count());
    std::copy(effect_order.constBegin(), effect_order.constEnd(),
              std::back_inserter(loaded_effects));

    m_activeEffects.reserve(loaded_effects.count());

    m_currentPaintScreenIterator = m_activeEffects.constBegin();
    m_currentPaintWindowIterator = m_activeEffects.constBegin();
    m_currentDrawWindowIterator = m_activeEffects.constBegin();
}

QStringList EffectsHandler::activeEffects() const
{
    QStringList ret;
    for (QList<KWin::EffectPair>::const_iterator it = loaded_effects.constBegin(),
                                                 end = loaded_effects.constEnd();
         it != end; ++it) {
        if (it->second->isActive()) {
            ret << it->first;
        }
    }
    return ret;
}

bool EffectsHandler::isEffectActive(const QString &pluginId) const
{
    auto it = std::find_if(loaded_effects.cbegin(), loaded_effects.cend(), [&pluginId](const EffectPair &p) {
        return p.first == pluginId;
    });
    if (it == loaded_effects.cend()) {
        return false;
    }
    return it->second->isActive();
}

bool EffectsHandler::blocksDirectScanout() const
{
    return std::any_of(m_activeEffects.constBegin(), m_activeEffects.constEnd(), [](const Effect *effect) {
        return effect->blocksDirectScanout();
    });
}

Display *EffectsHandler::waylandDisplay() const
{
    if (waylandServer()) {
        return waylandServer()->display();
    }
    return nullptr;
}

QVariant EffectsHandler::kwinOption(KWinOption kwopt)
{
    switch (kwopt) {
    case CloseButtonCorner: {
        // TODO: this could become per window and be derived from the actual position in the deco
        const auto settings = Workspace::self()->decorationBridge()->settings();
        return settings && settings->decorationButtonsLeft().contains(KDecoration2::DecorationButtonType::Close) ? Qt::TopLeftCorner : Qt::TopRightCorner;
    }
    case SwitchDesktopOnScreenEdge:
        return workspace()->screenEdges()->isDesktopSwitching();
    case SwitchDesktopOnScreenEdgeMovingWindows:
        return workspace()->screenEdges()->isDesktopSwitchingMovingClients();
    default:
        return QVariant(); // an invalid one
    }
}

QString EffectsHandler::supportInformation(const QString &name) const
{
    auto it = std::find_if(loaded_effects.constBegin(), loaded_effects.constEnd(),
                           [name](const EffectPair &pair) {
                               return pair.first == name;
                           });
    if (it == loaded_effects.constEnd()) {
        return QString();
    }

    QString support((*it).first + QLatin1String(":\n"));
    const QMetaObject *metaOptions = (*it).second->metaObject();
    for (int i = 0; i < metaOptions->propertyCount(); ++i) {
        const QMetaProperty property = metaOptions->property(i);
        if (qstrcmp(property.name(), "objectName") == 0) {
            continue;
        }
        support += QString::fromUtf8(property.name()) + QLatin1String(": ") + (*it).second->property(property.name()).toString() + QLatin1Char('\n');
    }

    return support;
}

bool EffectsHandler::isScreenLocked() const
{
#if KWIN_BUILD_SCREENLOCKER
    return kwinApp()->screenLockerWatcher()->isLocked();
#else
    return false;
#endif
}

QString EffectsHandler::debug(const QString &name, const QString &parameter) const
{
    QString internalName = name.toLower();
    for (QList<EffectPair>::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if ((*it).first == internalName) {
            return it->second->debug(parameter);
        }
    }
    return QString();
}

bool EffectsHandler::makeOpenGLContextCurrent()
{
    return m_scene->makeOpenGLContextCurrent();
}

void EffectsHandler::doneOpenGLContextCurrent()
{
    m_scene->doneOpenGLContextCurrent();
}

bool EffectsHandler::animationsSupported() const
{
    static const QByteArray forceEnvVar = qgetenv("KWIN_EFFECTS_FORCE_ANIMATIONS");
    if (!forceEnvVar.isEmpty()) {
        static const int forceValue = forceEnvVar.toInt();
        return forceValue == 1;
    }
    return m_scene->animationsSupported();
}

void EffectsHandler::highlightWindows(const QList<EffectWindow *> &windows)
{
    Effect *e = provides(Effect::HighlightWindows);
    if (!e) {
        return;
    }
    e->perform(Effect::HighlightWindows, QVariantList{QVariant::fromValue(windows)});
}

PlatformCursorImage EffectsHandler::cursorImage() const
{
    return kwinApp()->cursorImage();
}

void EffectsHandler::hideCursor()
{
    Cursors::self()->hideCursor();
}

void EffectsHandler::showCursor()
{
    Cursors::self()->showCursor();
}

void EffectsHandler::startInteractiveWindowSelection(std::function<void(KWin::EffectWindow *)> callback)
{
    kwinApp()->startInteractiveWindowSelection([callback](KWin::Window *window) {
        if (window && window->effectWindow()) {
            callback(window->effectWindow());
        } else {
            callback(nullptr);
        }
    });
}

void EffectsHandler::startInteractivePositionSelection(std::function<void(const QPointF &)> callback)
{
    kwinApp()->startInteractivePositionSelection(callback);
}

void EffectsHandler::showOnScreenMessage(const QString &message, const QString &iconName)
{
    OSD::show(message, iconName);
}

void EffectsHandler::hideOnScreenMessage(OnScreenMessageHideFlags flags)
{
    OSD::HideFlags osdFlags;
    if (flags.testFlag(OnScreenMessageHideFlag::SkipsCloseAnimation)) {
        osdFlags |= OSD::HideFlag::SkipCloseAnimation;
    }
    OSD::hide(osdFlags);
}

KSharedConfigPtr EffectsHandler::config() const
{
    return kwinApp()->config();
}

KSharedConfigPtr EffectsHandler::inputConfig() const
{
    return kwinApp()->inputConfig();
}

Effect *EffectsHandler::findEffect(const QString &name) const
{
    auto it = std::find_if(loaded_effects.constBegin(), loaded_effects.constEnd(), [name](const EffectPair &pair) {
        return pair.first == name;
    });
    if (it == loaded_effects.constEnd()) {
        return nullptr;
    }
    return (*it).second;
}

void EffectsHandler::renderOffscreenQuickView(const RenderTarget &renderTarget, const RenderViewport &viewport, OffscreenQuickView *w) const
{
    if (!w->isVisible()) {
        return;
    }
    if (compositingType() == OpenGLCompositing) {
        GLTexture *t = w->bufferAsTexture();
        if (!t) {
            return;
        }

        ShaderTraits traits = ShaderTrait::MapTexture | ShaderTrait::TransformColorspace;
        const qreal a = w->opacity();
        if (a != 1.0) {
            traits |= ShaderTrait::Modulate;
        }

        GLShader *shader = ShaderManager::instance()->pushShader(traits);
        const QRectF rect = scaledRect(w->geometry(), viewport.scale());

        QMatrix4x4 mvp(viewport.projectionMatrix());
        mvp.translate(rect.x(), rect.y());
        shader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);

        if (a != 1.0) {
            shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
        }
        shader->setColorspaceUniformsFromSRGB(renderTarget.colorDescription());

        const bool alphaBlending = w->hasAlphaChannel() || (a != 1.0);
        if (alphaBlending) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }

        t->render(w->size(), viewport.scale());

        if (alphaBlending) {
            glDisable(GL_BLEND);
        }

        ShaderManager::instance()->popShader();
    } else if (compositingType() == QPainterCompositing) {
        QPainter *painter = effects->scenePainter();
        const QImage buffer = w->bufferAsImage();
        if (buffer.isNull()) {
            return;
        }
        painter->save();
        painter->setOpacity(w->opacity());
        painter->drawImage(w->geometry(), buffer);
        painter->restore();
    }
}

SessionState EffectsHandler::sessionState() const
{
    return Workspace::self()->sessionManager()->state();
}

QList<Output *> EffectsHandler::screens() const
{
    return Workspace::self()->outputs();
}

Output *EffectsHandler::screenAt(const QPoint &point) const
{
    return Workspace::self()->outputAt(point);
}

Output *EffectsHandler::findScreen(const QString &name) const
{
    const auto outputs = Workspace::self()->outputs();
    for (Output *screen : outputs) {
        if (screen->name() == name) {
            return screen;
        }
    }
    return nullptr;
}

Output *EffectsHandler::findScreen(int screenId) const
{
    return Workspace::self()->outputs().value(screenId);
}

void EffectsHandler::renderScreen(Output *output)
{
    RenderTarget renderTarget(GLFramebuffer::currentFramebuffer());

    RenderLayer layer(output->renderLoop());
    SceneDelegate delegate(m_scene, output);
    delegate.setLayer(&layer);

    m_scene->prePaint(&delegate);
    m_scene->paint(renderTarget, output->geometry());
    m_scene->postPaint();
}

bool EffectsHandler::isCursorHidden() const
{
    return Cursors::self()->isCursorHidden();
}

KWin::EffectWindow *EffectsHandler::inputPanel() const
{
    if (!kwinApp()->inputMethod() || !kwinApp()->inputMethod()->isEnabled()) {
        return nullptr;
    }

    auto panel = kwinApp()->inputMethod()->panel();
    if (panel) {
        return panel->effectWindow();
    }
    return nullptr;
}

bool EffectsHandler::isInputPanelOverlay() const
{
    if (!kwinApp()->inputMethod() || !kwinApp()->inputMethod()->isEnabled()) {
        return true;
    }

    auto panel = kwinApp()->inputMethod()->panel();
    if (panel) {
        return panel->mode() == InputPanelV1Window::Mode::Overlay;
    }
    return true;
}

QQmlEngine *EffectsHandler::qmlEngine() const
{
    return Scripting::self()->qmlEngine();
}

EffectsHandler *effects = nullptr;

//****************************************
// EffectWindow
//****************************************

class Q_DECL_HIDDEN EffectWindow::Private
{
public:
    Private(EffectWindow *q, WindowItem *windowItem);

    EffectWindow *q;
    Window *m_window;
    WindowItem *m_windowItem; // This one is used only during paint pass.
    QHash<int, QVariant> dataMap;
    bool managed = false;
    bool m_waylandWindow;
    bool m_x11Window;
};

EffectWindow::Private::Private(EffectWindow *q, WindowItem *windowItem)
    : q(q)
    , m_window(windowItem->window())
    , m_windowItem(windowItem)
{
}

EffectWindow::EffectWindow(WindowItem *windowItem)
    : d(new Private(this, windowItem))
{
    // Deleted windows are not managed. So, when windowClosed signal is
    // emitted, effects can't distinguish managed windows from unmanaged
    // windows(e.g. combo box popups, popup menus, etc). Save value of the
    // managed property during construction of EffectWindow. At that time,
    // parent can be Client, XdgShellClient, or Unmanaged. So, later on, when
    // an instance of Deleted becomes parent of the EffectWindow, effects
    // can still figure out whether it is/was a managed window.
    d->managed = d->m_window->isClient();

    d->m_waylandWindow = qobject_cast<KWin::WaylandWindow *>(d->m_window) != nullptr;
    d->m_x11Window = qobject_cast<KWin::X11Window *>(d->m_window) != nullptr;

    connect(d->m_window, &Window::windowShown, this, [this]() {
        Q_EMIT windowShown(this);
    });
    connect(d->m_window, &Window::windowHidden, this, [this]() {
        Q_EMIT windowHidden(this);
    });
    connect(d->m_window, &Window::maximizedChanged, this, [this]() {
        const MaximizeMode mode = d->m_window->maximizeMode();
        Q_EMIT windowMaximizedStateChanged(this, mode & MaximizeHorizontal, mode & MaximizeVertical);
    });
    connect(d->m_window, &Window::maximizedAboutToChange, this, [this](MaximizeMode m) {
        Q_EMIT windowMaximizedStateAboutToChange(this, m & MaximizeHorizontal, m & MaximizeVertical);
    });
    connect(d->m_window, &Window::frameGeometryAboutToChange, this, [this]() {
        Q_EMIT windowFrameGeometryAboutToChange(this);
    });
    connect(d->m_window, &Window::interactiveMoveResizeStarted, this, [this]() {
        Q_EMIT windowStartUserMovedResized(this);
    });
    connect(d->m_window, &Window::interactiveMoveResizeStepped, this, [this](const QRectF &geometry) {
        Q_EMIT windowStepUserMovedResized(this, geometry);
    });
    connect(d->m_window, &Window::interactiveMoveResizeFinished, this, [this]() {
        Q_EMIT windowFinishUserMovedResized(this);
    });
    connect(d->m_window, &Window::opacityChanged, this, [this](Window *window, qreal oldOpacity) {
        Q_EMIT windowOpacityChanged(this, oldOpacity, window->opacity());
    });
    connect(d->m_window, &Window::minimizedChanged, this, [this]() {
        if (d->m_window->isMinimized()) {
            Q_EMIT windowMinimized(this);
        } else {
            Q_EMIT windowUnminimized(this);
        }
    });
    connect(d->m_window, &Window::modalChanged, this, [this]() {
        Q_EMIT windowModalityChanged(this);
    });
    connect(d->m_window, &Window::frameGeometryChanged, this, [this](const QRectF &oldGeometry) {
        Q_EMIT windowFrameGeometryChanged(this, oldGeometry);
    });
    connect(d->m_window, &Window::damaged, this, [this]() {
        Q_EMIT windowDamaged(this);
    });
    connect(d->m_window, &Window::unresponsiveChanged, this, [this](bool unresponsive) {
        Q_EMIT windowUnresponsiveChanged(this, unresponsive);
    });
    connect(d->m_window, &Window::keepAboveChanged, this, [this]() {
        Q_EMIT windowKeepAboveChanged(this);
    });
    connect(d->m_window, &Window::keepBelowChanged, this, [this]() {
        Q_EMIT windowKeepBelowChanged(this);
    });
    connect(d->m_window, &Window::fullScreenChanged, this, [this]() {
        Q_EMIT windowFullScreenChanged(this);
    });
    connect(d->m_window, &Window::visibleGeometryChanged, this, [this]() {
        Q_EMIT windowExpandedGeometryChanged(this);
    });
    connect(d->m_window, &Window::decorationChanged, this, [this]() {
        Q_EMIT windowDecorationChanged(this);
    });
    connect(d->m_window, &Window::desktopsChanged, this, [this]() {
        Q_EMIT windowDesktopsChanged(this);
    });
}

EffectWindow::~EffectWindow()
{
}

Window *EffectWindow::window() const
{
    return d->m_window;
}

WindowItem *EffectWindow::windowItem() const
{
    return d->m_windowItem;
}

bool EffectWindow::isOnActivity(const QString &activity) const
{
    const QStringList _activities = activities();
    return _activities.isEmpty() || _activities.contains(activity);
}

bool EffectWindow::isOnAllActivities() const
{
    return activities().isEmpty();
}

void EffectWindow::setMinimized(bool min)
{
    if (min) {
        minimize();
    } else {
        unminimize();
    }
}

bool EffectWindow::isOnCurrentActivity() const
{
    return isOnActivity(effects->currentActivity());
}

bool EffectWindow::isOnCurrentDesktop() const
{
    return isOnDesktop(effects->currentDesktop());
}

bool EffectWindow::isOnDesktop(VirtualDesktop *desktop) const
{
    const QList<VirtualDesktop *> ds = desktops();
    return ds.isEmpty() || ds.contains(desktop);
}

bool EffectWindow::isOnAllDesktops() const
{
    return desktops().isEmpty();
}

bool EffectWindow::hasDecoration() const
{
    return contentsRect() != QRect(0, 0, width(), height());
}

bool EffectWindow::isVisible() const
{
    return !isMinimized()
        && isOnCurrentDesktop()
        && isOnCurrentActivity();
}

void EffectWindow::refVisible(const EffectWindowVisibleRef *holder)
{
    d->m_windowItem->refVisible(holder->reason());
}

void EffectWindow::unrefVisible(const EffectWindowVisibleRef *holder)
{
    d->m_windowItem->unrefVisible(holder->reason());
}

void EffectWindow::addRepaint(const QRect &r)
{
    d->m_windowItem->scheduleRepaint(QRegion(r));
}

void EffectWindow::addRepaintFull()
{
    d->m_windowItem->scheduleRepaint(d->m_windowItem->boundingRect());
}

void EffectWindow::addLayerRepaint(const QRect &r)
{
    d->m_windowItem->scheduleRepaint(d->m_windowItem->mapFromGlobal(r));
}

const EffectWindowGroup *EffectWindow::group() const
{
    if (auto c = qobject_cast<X11Window *>(d->m_window)) {
        return c->group()->effectGroup();
    }
    return nullptr; // TODO
}

void EffectWindow::refWindow()
{
    if (d->m_window->isDeleted()) {
        return d->m_window->ref();
    }
    Q_UNREACHABLE(); // TODO
}

void EffectWindow::unrefWindow()
{
    if (d->m_window->isDeleted()) {
        return d->m_window->unref();
    }
    Q_UNREACHABLE(); // TODO
}

Output *EffectWindow::screen() const
{
    return d->m_window->output();
}

#define WINDOW_HELPER(rettype, prototype, toplevelPrototype) \
    rettype EffectWindow::prototype() const                  \
    {                                                        \
        return d->m_window->toplevelPrototype();             \
    }

WINDOW_HELPER(double, opacity, opacity)
WINDOW_HELPER(qreal, x, x)
WINDOW_HELPER(qreal, y, y)
WINDOW_HELPER(qreal, width, width)
WINDOW_HELPER(qreal, height, height)
WINDOW_HELPER(QPointF, pos, pos)
WINDOW_HELPER(QSizeF, size, size)
WINDOW_HELPER(QRectF, frameGeometry, frameGeometry)
WINDOW_HELPER(QRectF, bufferGeometry, bufferGeometry)
WINDOW_HELPER(QRectF, clientGeometry, clientGeometry)
WINDOW_HELPER(QRectF, expandedGeometry, visibleGeometry)
WINDOW_HELPER(QRectF, rect, rect)
WINDOW_HELPER(bool, isDesktop, isDesktop)
WINDOW_HELPER(bool, isDock, isDock)
WINDOW_HELPER(bool, isToolbar, isToolbar)
WINDOW_HELPER(bool, isMenu, isMenu)
WINDOW_HELPER(bool, isNormalWindow, isNormalWindow)
WINDOW_HELPER(bool, isDialog, isDialog)
WINDOW_HELPER(bool, isSplash, isSplash)
WINDOW_HELPER(bool, isUtility, isUtility)
WINDOW_HELPER(bool, isDropdownMenu, isDropdownMenu)
WINDOW_HELPER(bool, isPopupMenu, isPopupMenu)
WINDOW_HELPER(bool, isTooltip, isTooltip)
WINDOW_HELPER(bool, isNotification, isNotification)
WINDOW_HELPER(bool, isCriticalNotification, isCriticalNotification)
WINDOW_HELPER(bool, isAppletPopup, isAppletPopup)
WINDOW_HELPER(bool, isOnScreenDisplay, isOnScreenDisplay)
WINDOW_HELPER(bool, isComboBox, isComboBox)
WINDOW_HELPER(bool, isDNDIcon, isDNDIcon)
WINDOW_HELPER(bool, isDeleted, isDeleted)
WINDOW_HELPER(QString, windowRole, windowRole)
WINDOW_HELPER(QStringList, activities, activities)
WINDOW_HELPER(bool, skipsCloseAnimation, skipsCloseAnimation)
WINDOW_HELPER(SurfaceInterface *, surface, surface)
WINDOW_HELPER(bool, isPopupWindow, isPopupWindow)
WINDOW_HELPER(bool, isOutline, isOutline)
WINDOW_HELPER(bool, isLockScreen, isLockScreen)
WINDOW_HELPER(pid_t, pid, pid)
WINDOW_HELPER(QUuid, internalId, internalId)
WINDOW_HELPER(bool, isMinimized, isMinimized)
WINDOW_HELPER(bool, isHidden, isHidden)
WINDOW_HELPER(bool, isHiddenByShowDesktop, isHiddenByShowDesktop)
WINDOW_HELPER(bool, isModal, isModal)
WINDOW_HELPER(bool, isFullScreen, isFullScreen)
WINDOW_HELPER(bool, keepAbove, keepAbove)
WINDOW_HELPER(bool, keepBelow, keepBelow)
WINDOW_HELPER(QString, caption, caption)
WINDOW_HELPER(bool, isMovable, isMovable)
WINDOW_HELPER(bool, isMovableAcrossScreens, isMovableAcrossScreens)
WINDOW_HELPER(bool, isUserMove, isInteractiveMove)
WINDOW_HELPER(bool, isUserResize, isInteractiveResize)
WINDOW_HELPER(QRectF, iconGeometry, iconGeometry)
WINDOW_HELPER(bool, isSpecialWindow, isSpecialWindow)
WINDOW_HELPER(bool, acceptsFocus, wantsInput)
WINDOW_HELPER(QIcon, icon, icon)
WINDOW_HELPER(bool, isSkipSwitcher, skipSwitcher)
WINDOW_HELPER(bool, decorationHasAlpha, decorationHasAlpha)
WINDOW_HELPER(bool, isUnresponsive, unresponsive)
WINDOW_HELPER(QList<VirtualDesktop *>, desktops, desktops)
WINDOW_HELPER(bool, isInputMethod, isInputMethod)

#undef WINDOW_HELPER

qlonglong EffectWindow::windowId() const
{
    if (X11Window *x11Window = qobject_cast<X11Window *>(d->m_window)) {
        return x11Window->window();
    }
    return 0;
}

QString EffectWindow::windowClass() const
{
    return d->m_window->resourceName() + QLatin1Char(' ') + d->m_window->resourceClass();
}

QRectF EffectWindow::contentsRect() const
{
    return QRectF(d->m_window->clientPos(), d->m_window->clientSize());
}

NET::WindowType EffectWindow::windowType() const
{
    return d->m_window->windowType();
}

QSizeF EffectWindow::basicUnit() const
{
    if (auto window = qobject_cast<X11Window *>(d->m_window)) {
        return window->basicUnit();
    }
    return QSize(1, 1);
}

QRectF EffectWindow::decorationInnerRect() const
{
    return d->m_window->rect() - d->m_window->frameMargins();
}

KDecoration2::Decoration *EffectWindow::decoration() const
{
    return d->m_window->decoration();
}

QByteArray EffectWindow::readProperty(long atom, long type, int format) const
{
    auto x11Window = qobject_cast<X11Window *>(d->m_window);
    if (!x11Window) {
        return QByteArray();
    }
    if (!kwinApp()->x11Connection()) {
        return QByteArray();
    }
    return readWindowProperty(x11Window->window(), atom, type, format);
}

void EffectWindow::deleteProperty(long int atom) const
{
    auto x11Window = qobject_cast<X11Window *>(d->m_window);
    if (!x11Window) {
        return;
    }
    if (kwinApp()->x11Connection()) {
        deleteWindowProperty(x11Window->window(), atom);
    }
}

EffectWindow *EffectWindow::findModal()
{
    Window *modal = d->m_window->findModal();
    if (modal) {
        return modal->effectWindow();
    }

    return nullptr;
}

EffectWindow *EffectWindow::transientFor()
{
    Window *transientFor = d->m_window->transientFor();
    if (transientFor) {
        return transientFor->effectWindow();
    }

    return nullptr;
}

QWindow *EffectWindow::internalWindow() const
{
    if (auto window = qobject_cast<InternalWindow *>(d->m_window)) {
        return window->handle();
    }
    return nullptr;
}

template<typename T>
EffectWindowList getMainWindows(T *c)
{
    const auto mainwindows = c->mainWindows();
    EffectWindowList ret;
    ret.reserve(mainwindows.size());
    std::transform(std::cbegin(mainwindows), std::cend(mainwindows),
                   std::back_inserter(ret),
                   [](auto window) {
                       return window->effectWindow();
                   });
    return ret;
}

EffectWindowList EffectWindow::mainWindows() const
{
    return getMainWindows(d->m_window);
}

void EffectWindow::setData(int role, const QVariant &data)
{
    if (!data.isNull()) {
        d->dataMap[role] = data;
    } else {
        d->dataMap.remove(role);
    }
    Q_EMIT effects->windowDataChanged(this, role);
}

QVariant EffectWindow::data(int role) const
{
    return d->dataMap.value(role);
}

void EffectWindow::elevate(bool elevate)
{
    effects->setElevatedWindow(this, elevate);
}

void EffectWindow::minimize()
{
    if (d->m_window->isClient()) {
        d->m_window->setMinimized(true);
    }
}

void EffectWindow::unminimize()
{
    if (d->m_window->isClient()) {
        d->m_window->setMinimized(false);
    }
}

void EffectWindow::closeWindow()
{
    if (d->m_window->isClient()) {
        d->m_window->closeWindow();
    }
}

bool EffectWindow::isManaged() const
{
    return d->managed;
}

bool EffectWindow::isWaylandClient() const
{
    return d->m_waylandWindow;
}

bool EffectWindow::isX11Client() const
{
    return d->m_x11Window;
}

//****************************************
// EffectWindowGroup
//****************************************

EffectWindowGroup::EffectWindowGroup(Group *group)
    : m_group(group)
{
}

EffectWindowGroup::~EffectWindowGroup()
{
}

EffectWindowList EffectWindowGroup::members() const
{
    const auto memberList = m_group->members();
    EffectWindowList ret;
    ret.reserve(memberList.size());
    std::transform(std::cbegin(memberList), std::cend(memberList), std::back_inserter(ret), [](auto window) {
        return window->effectWindow();
    });
    return ret;
}

/***************************************************************
 WindowQuad
***************************************************************/

WindowQuad WindowQuad::makeSubQuad(double x1, double y1, double x2, double y2) const
{
    Q_ASSERT(x1 < x2 && y1 < y2 && x1 >= left() && x2 <= right() && y1 >= top() && y2 <= bottom());
    WindowQuad ret(*this);
    // vertices are clockwise starting from topleft
    ret.verts[0].px = x1;
    ret.verts[3].px = x1;
    ret.verts[1].px = x2;
    ret.verts[2].px = x2;
    ret.verts[0].py = y1;
    ret.verts[1].py = y1;
    ret.verts[2].py = y2;
    ret.verts[3].py = y2;

    const double xOrigin = left();
    const double yOrigin = top();

    const double widthReciprocal = 1 / (right() - xOrigin);
    const double heightReciprocal = 1 / (bottom() - yOrigin);

    for (int i = 0; i < 4; ++i) {
        const double w1 = (ret.verts[i].px - xOrigin) * widthReciprocal;
        const double w2 = (ret.verts[i].py - yOrigin) * heightReciprocal;

        // Use bilinear interpolation to compute the texture coords.
        ret.verts[i].tx = (1 - w1) * (1 - w2) * verts[0].tx + w1 * (1 - w2) * verts[1].tx + w1 * w2 * verts[2].tx + (1 - w1) * w2 * verts[3].tx;
        ret.verts[i].ty = (1 - w1) * (1 - w2) * verts[0].ty + w1 * (1 - w2) * verts[1].ty + w1 * w2 * verts[2].ty + (1 - w1) * w2 * verts[3].ty;
    }

    return ret;
}

/***************************************************************
 WindowQuadList
***************************************************************/

WindowQuadList WindowQuadList::splitAtX(double x) const
{
    WindowQuadList ret;
    ret.reserve(count());
    for (const WindowQuad &quad : *this) {
        bool wholeleft = true;
        bool wholeright = true;
        for (int i = 0; i < 4; ++i) {
            if (quad[i].x() < x) {
                wholeright = false;
            }
            if (quad[i].x() > x) {
                wholeleft = false;
            }
        }
        if (wholeleft || wholeright) { // is whole in one split part
            ret.append(quad);
            continue;
        }
        if (quad.top() == quad.bottom() || quad.left() == quad.right()) { // quad has no size
            ret.append(quad);
            continue;
        }
        ret.append(quad.makeSubQuad(quad.left(), quad.top(), x, quad.bottom()));
        ret.append(quad.makeSubQuad(x, quad.top(), quad.right(), quad.bottom()));
    }
    return ret;
}

WindowQuadList WindowQuadList::splitAtY(double y) const
{
    WindowQuadList ret;
    ret.reserve(count());
    for (const WindowQuad &quad : *this) {
        bool wholetop = true;
        bool wholebottom = true;
        for (int i = 0; i < 4; ++i) {
            if (quad[i].y() < y) {
                wholebottom = false;
            }
            if (quad[i].y() > y) {
                wholetop = false;
            }
        }
        if (wholetop || wholebottom) { // is whole in one split part
            ret.append(quad);
            continue;
        }
        if (quad.top() == quad.bottom() || quad.left() == quad.right()) { // quad has no size
            ret.append(quad);
            continue;
        }
        ret.append(quad.makeSubQuad(quad.left(), quad.top(), quad.right(), y));
        ret.append(quad.makeSubQuad(quad.left(), y, quad.right(), quad.bottom()));
    }
    return ret;
}

WindowQuadList WindowQuadList::makeGrid(int maxQuadSize) const
{
    if (empty()) {
        return *this;
    }

    // Find the bounding rectangle
    double left = first().left();
    double right = first().right();
    double top = first().top();
    double bottom = first().bottom();

    for (const WindowQuad &quad : std::as_const(*this)) {
        left = std::min(left, quad.left());
        right = std::max(right, quad.right());
        top = std::min(top, quad.top());
        bottom = std::max(bottom, quad.bottom());
    }

    WindowQuadList ret;

    for (const WindowQuad &quad : std::as_const(*this)) {
        const double quadLeft = quad.left();
        const double quadRight = quad.right();
        const double quadTop = quad.top();
        const double quadBottom = quad.bottom();

        // sanity check, see BUG 390953
        if (quadLeft == quadRight || quadTop == quadBottom) {
            ret.append(quad);
            continue;
        }

        // Compute the top-left corner of the first intersecting grid cell
        const double xBegin = left + qFloor((quadLeft - left) / maxQuadSize) * maxQuadSize;
        const double yBegin = top + qFloor((quadTop - top) / maxQuadSize) * maxQuadSize;

        // Loop over all intersecting cells and add sub-quads
        for (double y = yBegin; y < quadBottom; y += maxQuadSize) {
            const double y0 = std::max(y, quadTop);
            const double y1 = std::min(quadBottom, y + maxQuadSize);

            for (double x = xBegin; x < quadRight; x += maxQuadSize) {
                const double x0 = std::max(x, quadLeft);
                const double x1 = std::min(quadRight, x + maxQuadSize);

                ret.append(quad.makeSubQuad(x0, y0, x1, y1));
            }
        }
    }

    return ret;
}

WindowQuadList WindowQuadList::makeRegularGrid(int xSubdivisions, int ySubdivisions) const
{
    if (empty()) {
        return *this;
    }

    // Find the bounding rectangle
    double left = first().left();
    double right = first().right();
    double top = first().top();
    double bottom = first().bottom();

    for (const WindowQuad &quad : *this) {
        left = std::min(left, quad.left());
        right = std::max(right, quad.right());
        top = std::min(top, quad.top());
        bottom = std::max(bottom, quad.bottom());
    }

    double xIncrement = (right - left) / xSubdivisions;
    double yIncrement = (bottom - top) / ySubdivisions;

    WindowQuadList ret;

    for (const WindowQuad &quad : *this) {
        const double quadLeft = quad.left();
        const double quadRight = quad.right();
        const double quadTop = quad.top();
        const double quadBottom = quad.bottom();

        // sanity check, see BUG 390953
        if (quadLeft == quadRight || quadTop == quadBottom) {
            ret.append(quad);
            continue;
        }

        // Compute the top-left corner of the first intersecting grid cell
        const double xBegin = left + qFloor((quadLeft - left) / xIncrement) * xIncrement;
        const double yBegin = top + qFloor((quadTop - top) / yIncrement) * yIncrement;

        // Loop over all intersecting cells and add sub-quads
        for (double y = yBegin; y < quadBottom; y += yIncrement) {
            const double y0 = std::max(y, quadTop);
            const double y1 = std::min(quadBottom, y + yIncrement);

            for (double x = xBegin; x < quadRight; x += xIncrement) {
                const double x0 = std::max(x, quadLeft);
                const double x1 = std::min(quadRight, x + xIncrement);

                ret.append(quad.makeSubQuad(x0, y0, x1, y1));
            }
        }
    }

    return ret;
}

void RenderGeometry::copy(std::span<GLVertex2D> destination)
{
    Q_ASSERT(int(destination.size()) >= size());
    std::copy(cbegin(), cend(), destination.begin());
}

void RenderGeometry::appendWindowVertex(const WindowVertex &windowVertex, qreal deviceScale)
{
    GLVertex2D glVertex;
    switch (m_vertexSnappingMode) {
    case VertexSnappingMode::None:
        glVertex.position = QVector2D(windowVertex.x(), windowVertex.y()) * deviceScale;
        break;
    case VertexSnappingMode::Round:
        glVertex.position = roundVector(QVector2D(windowVertex.x(), windowVertex.y()) * deviceScale);
        break;
    }
    glVertex.texcoord = QVector2D(windowVertex.u(), windowVertex.v());
    append(glVertex);
}

void RenderGeometry::appendWindowQuad(const WindowQuad &quad, qreal deviceScale)
{
    // Geometry assumes we're rendering triangles, so add the quad's
    // vertices as two triangles. Vertex order is top-left, bottom-left,
    // top-right followed by top-right, bottom-left, bottom-right.
    appendWindowVertex(quad[0], deviceScale);
    appendWindowVertex(quad[3], deviceScale);
    appendWindowVertex(quad[1], deviceScale);

    appendWindowVertex(quad[1], deviceScale);
    appendWindowVertex(quad[3], deviceScale);
    appendWindowVertex(quad[2], deviceScale);
}

void RenderGeometry::appendSubQuad(const WindowQuad &quad, const QRectF &subquad, qreal deviceScale)
{
    std::array<GLVertex2D, 4> vertices;
    vertices[0].position = QVector2D(subquad.topLeft());
    vertices[1].position = QVector2D(subquad.topRight());
    vertices[2].position = QVector2D(subquad.bottomRight());
    vertices[3].position = QVector2D(subquad.bottomLeft());

    const auto deviceQuad = QRectF{QPointF(std::round(quad.left() * deviceScale), std::round(quad.top() * deviceScale)),
                                   QPointF(std::round(quad.right() * deviceScale), std::round(quad.bottom() * deviceScale))};

    const QPointF origin = deviceQuad.topLeft();
    const QSizeF size = deviceQuad.size();

#pragma GCC unroll 4
    for (int i = 0; i < 4; ++i) {
        const double weight1 = (vertices[i].position.x() - origin.x()) / size.width();
        const double weight2 = (vertices[i].position.y() - origin.y()) / size.height();
        const double oneMinW1 = 1.0 - weight1;
        const double oneMinW2 = 1.0 - weight2;

        const float u = oneMinW1 * oneMinW2 * quad[0].u() + weight1 * oneMinW2 * quad[1].u()
            + weight1 * weight2 * quad[2].u() + oneMinW1 * weight2 * quad[3].u();
        const float v = oneMinW1 * oneMinW2 * quad[0].v() + weight1 * oneMinW2 * quad[1].v()
            + weight1 * weight2 * quad[2].v() + oneMinW1 * weight2 * quad[3].v();
        vertices[i].texcoord = QVector2D(u, v);
    }

    append(vertices[0]);
    append(vertices[3]);
    append(vertices[1]);

    append(vertices[1]);
    append(vertices[3]);
    append(vertices[2]);
}

void RenderGeometry::postProcessTextureCoordinates(const QMatrix4x4 &textureMatrix)
{
    if (!textureMatrix.isIdentity()) {
        const QVector2D coeff(textureMatrix(0, 0), textureMatrix(1, 1));
        const QVector2D offset(textureMatrix(0, 3), textureMatrix(1, 3));

        for (auto &vertex : (*this)) {
            vertex.texcoord = vertex.texcoord * coeff + offset;
        }
    }
}

/***************************************************************
 Motion1D
***************************************************************/

Motion1D::Motion1D(double initial, double strength, double smoothness)
    : Motion<double>(initial, strength, smoothness)
{
}

Motion1D::Motion1D(const Motion1D &other)
    : Motion<double>(other)
{
}

Motion1D::~Motion1D()
{
}

/***************************************************************
 Motion2D
***************************************************************/

Motion2D::Motion2D(QPointF initial, double strength, double smoothness)
    : Motion<QPointF>(initial, strength, smoothness)
{
}

Motion2D::Motion2D(const Motion2D &other)
    : Motion<QPointF>(other)
{
}

Motion2D::~Motion2D()
{
}

/***************************************************************
 WindowMotionManager
***************************************************************/

WindowMotionManager::WindowMotionManager(bool useGlobalAnimationModifier)
    : m_useGlobalAnimationModifier(useGlobalAnimationModifier)

{
    // TODO: Allow developer to modify motion attributes
} // TODO: What happens when the window moves by an external force?

WindowMotionManager::~WindowMotionManager()
{
}

void WindowMotionManager::manage(EffectWindow *w)
{
    if (m_managedWindows.contains(w)) {
        return;
    }

    double strength = 0.08;
    double smoothness = 4.0;
    if (m_useGlobalAnimationModifier && effects->animationTimeFactor()) {
        // If the factor is == 0 then we just skip the calculation completely
        strength = 0.08 / effects->animationTimeFactor();
        smoothness = effects->animationTimeFactor() * 4.0;
    }

    WindowMotion &motion = m_managedWindows[w];
    motion.translation.setStrength(strength);
    motion.translation.setSmoothness(smoothness);
    motion.scale.setStrength(strength * 1.33);
    motion.scale.setSmoothness(smoothness / 2.0);

    motion.translation.setValue(w->pos());
    motion.scale.setValue(QPointF(1.0, 1.0));
}

void WindowMotionManager::unmanage(EffectWindow *w)
{
    m_movingWindowsSet.remove(w);
    m_managedWindows.remove(w);
}

void WindowMotionManager::unmanageAll()
{
    m_managedWindows.clear();
    m_movingWindowsSet.clear();
}

void WindowMotionManager::calculate(int time)
{
    if (!effects->animationTimeFactor()) {
        // Just skip it completely if the user wants no animation
        m_movingWindowsSet.clear();
        QHash<EffectWindow *, WindowMotion>::iterator it = m_managedWindows.begin();
        for (; it != m_managedWindows.end(); ++it) {
            WindowMotion *motion = &it.value();
            motion->translation.finish();
            motion->scale.finish();
        }
    }

    QHash<EffectWindow *, WindowMotion>::iterator it = m_managedWindows.begin();
    for (; it != m_managedWindows.end(); ++it) {
        WindowMotion *motion = &it.value();
        int stopped = 0;

        // TODO: What happens when distance() == 0 but we are still moving fast?
        // TODO: Motion needs to be calculated from the window's center

        Motion2D *trans = &motion->translation;
        if (trans->distance().isNull()) {
            ++stopped;
        } else {
            // Still moving
            trans->calculate(time);
            const short fx = trans->target().x() <= trans->startValue().x() ? -1 : 1;
            const short fy = trans->target().y() <= trans->startValue().y() ? -1 : 1;
            if (trans->distance().x() * fx / 0.5 < 1.0 && trans->velocity().x() * fx / 0.2 < 1.0
                && trans->distance().y() * fy / 0.5 < 1.0 && trans->velocity().y() * fy / 0.2 < 1.0) {
                // Hide tiny oscillations
                motion->translation.finish();
                ++stopped;
            }
        }

        Motion2D *scale = &motion->scale;
        if (scale->distance().isNull()) {
            ++stopped;
        } else {
            // Still scaling
            scale->calculate(time);
            const short fx = scale->target().x() < 1.0 ? -1 : 1;
            const short fy = scale->target().y() < 1.0 ? -1 : 1;
            if (scale->distance().x() * fx / 0.001 < 1.0 && scale->velocity().x() * fx / 0.05 < 1.0
                && scale->distance().y() * fy / 0.001 < 1.0 && scale->velocity().y() * fy / 0.05 < 1.0) {
                // Hide tiny oscillations
                motion->scale.finish();
                ++stopped;
            }
        }

        // We just finished this window's motion
        if (stopped == 2) {
            m_movingWindowsSet.remove(it.key());
        }
    }
}

void WindowMotionManager::reset()
{
    QHash<EffectWindow *, WindowMotion>::iterator it = m_managedWindows.begin();
    for (; it != m_managedWindows.end(); ++it) {
        WindowMotion *motion = &it.value();
        EffectWindow *window = it.key();
        motion->translation.setTarget(window->pos());
        motion->translation.finish();
        motion->scale.setTarget(QPointF(1.0, 1.0));
        motion->scale.finish();
    }
}

void WindowMotionManager::reset(EffectWindow *w)
{
    QHash<EffectWindow *, WindowMotion>::iterator it = m_managedWindows.find(w);
    if (it == m_managedWindows.end()) {
        return;
    }

    WindowMotion *motion = &it.value();
    motion->translation.setTarget(w->pos());
    motion->translation.finish();
    motion->scale.setTarget(QPointF(1.0, 1.0));
    motion->scale.finish();
}

void WindowMotionManager::apply(EffectWindow *w, WindowPaintData &data)
{
    QHash<EffectWindow *, WindowMotion>::iterator it = m_managedWindows.find(w);
    if (it == m_managedWindows.end()) {
        return;
    }

    // TODO: Take into account existing scale so that we can work with multiple managers (E.g. Present windows + grid)
    WindowMotion *motion = &it.value();
    data += (motion->translation.value() - QPointF(w->x(), w->y()));
    data *= QVector2D(motion->scale.value());
}

void WindowMotionManager::moveWindow(EffectWindow *w, QPoint target, double scale, double yScale)
{
    QHash<EffectWindow *, WindowMotion>::iterator it = m_managedWindows.find(w);
    Q_ASSERT(it != m_managedWindows.end()); // Notify the effect author that they did something wrong

    WindowMotion *motion = &it.value();

    if (yScale == 0.0) {
        yScale = scale;
    }
    QPointF scalePoint(scale, yScale);

    if (motion->translation.value() == target && motion->scale.value() == scalePoint) {
        return; // Window already at that position
    }

    motion->translation.setTarget(target);
    motion->scale.setTarget(scalePoint);

    m_movingWindowsSet << w;
}

QRectF WindowMotionManager::transformedGeometry(EffectWindow *w) const
{
    QHash<EffectWindow *, WindowMotion>::const_iterator it = m_managedWindows.constFind(w);
    if (it == m_managedWindows.end()) {
        return w->frameGeometry();
    }

    const WindowMotion *motion = &it.value();
    QRectF geometry(w->frameGeometry());

    // TODO: Take into account existing scale so that we can work with multiple managers (E.g. Present windows + grid)
    geometry.moveTo(motion->translation.value());
    geometry.setWidth(geometry.width() * motion->scale.value().x());
    geometry.setHeight(geometry.height() * motion->scale.value().y());

    return geometry;
}

void WindowMotionManager::setTransformedGeometry(EffectWindow *w, const QRectF &geometry)
{
    QHash<EffectWindow *, WindowMotion>::iterator it = m_managedWindows.find(w);
    if (it == m_managedWindows.end()) {
        return;
    }
    WindowMotion *motion = &it.value();
    motion->translation.setValue(geometry.topLeft());
    motion->scale.setValue(QPointF(geometry.width() / qreal(w->width()), geometry.height() / qreal(w->height())));
}

QRectF WindowMotionManager::targetGeometry(EffectWindow *w) const
{
    QHash<EffectWindow *, WindowMotion>::const_iterator it = m_managedWindows.constFind(w);
    if (it == m_managedWindows.end()) {
        return w->frameGeometry();
    }

    const WindowMotion *motion = &it.value();
    QRectF geometry(w->frameGeometry());

    // TODO: Take into account existing scale so that we can work with multiple managers (E.g. Present windows + grid)
    geometry.moveTo(motion->translation.target());
    geometry.setWidth(geometry.width() * motion->scale.target().x());
    geometry.setHeight(geometry.height() * motion->scale.target().y());

    return geometry;
}

EffectWindow *WindowMotionManager::windowAtPoint(QPoint point, bool useStackingOrder) const
{
    // TODO: Stacking order uses EffectsHandler::stackingOrder() then filters by m_managedWindows
    QHash<EffectWindow *, WindowMotion>::ConstIterator it = m_managedWindows.constBegin();
    while (it != m_managedWindows.constEnd()) {
        if (exclusiveContains(transformedGeometry(it.key()), point)) {
            return it.key();
        }
        ++it;
    }

    return nullptr;
}

} // namespace

#include "moc_effects.cpp"
#include "moc_globals.cpp"
