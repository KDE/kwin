/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "effect/effecthandler.h"

#include "config-kwin.h"

#include "compositor.h"
#include "core/inputdevice.h"
#include "core/output.h"
#include "core/renderbackend.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "decorations/decorationbridge.h"
#include "effect/effectloader.h"
#include "effect/offscreenquickview.h"
#include "effectsadaptor.h"
#include "input.h"
#include "input_event.h"
#include "inputmethod.h"
#include "inputpanelv1window.h"
#include "keyboard_input.h"
#include "opengl/glshader.h"
#include "opengl/glshadermanager.h"
#include "opengl/gltexture.h"
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
#include "window_property_notify_x11_filter.h"
#include "workspace.h"
#if KWIN_BUILD_X11
#include "x11window.h"
#endif
#if KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#if KWIN_BUILD_TABBOX
#include "tabbox/tabbox.h"
#endif
#if KWIN_BUILD_SCREENLOCKER
#include "screenlockerwatcher.h"
#endif

#include <KDecoration3/Decoration>
#include <KDecoration3/DecorationSettings>

#include <QFontMetrics>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QTimeLine>
#include <QVariant>
#include <QWindow>
#include <QtMath>

namespace KWin
{
#if KWIN_BUILD_X11
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
#endif

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
        connect(activities, &Activities::currentAboutToChange, this, &EffectsHandler::currentActivityAboutToChange);
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

#if KWIN_BUILD_X11
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
#endif

    // connect all clients
    for (Window *window : ws->windows()) {
        setupWindowConnections(window);
    }

    connect(ws, &Workspace::outputAdded, this, &EffectsHandler::screenAdded);
    connect(ws, &Workspace::outputRemoved, this, &EffectsHandler::screenRemoved);

    if (auto inputMethod = kwinApp()->inputMethod()) {
        connect(inputMethod, &InputMethod::panelChanged, this, &EffectsHandler::inputPanelChanged);
    }

    connect(Cursors::self()->mouse(), &Cursor::cursorChanged, this, &EffectsHandler::cursorShapeChanged);

    reconfigure();
}

EffectsHandler::~EffectsHandler()
{
    unloadAllEffects();
    KWin::effects = nullptr;
}

#if KWIN_BUILD_X11
xcb_window_t EffectsHandler::x11RootWindow() const
{
    return kwinApp()->x11RootWindow();
}

xcb_connection_t *EffectsHandler::xcbConnection() const
{
    return kwinApp()->x11Connection();
}
#endif

CompositingType EffectsHandler::compositingType() const
{
    return compositing_type;
}

bool EffectsHandler::isOpenGLCompositing() const
{
    return compositing_type & OpenGLCompositing;
}

OpenGlContext *EffectsHandler::openglContext() const
{
    return m_scene->openglContext();
}

void EffectsHandler::unloadAllEffects()
{
    m_activeEffects.clear();
    effect_order.clear();
    m_effectLoader->clear();

    const auto loaded = std::move(loaded_effects);
    for (const EffectPair &pair : loaded) {
        destroyEffect(pair.second);
    }

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

        setShowingDesktop(false);
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

bool EffectsHandler::isColorPickerActive() const
{
    return isEffectActive(QStringLiteral("colorpicker"));
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
    input()->keyboard()->update();
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

void EffectsHandler::touchCancel()
{
    for (const auto &[name, effect] : std::as_const(loaded_effects)) {
        effect->touchCancel();
    }
}

bool EffectsHandler::tabletToolProximityEvent(TabletToolProximityEvent *event)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->tabletToolProximity(event)) {
            return true;
        }
    }
    return false;
}

bool EffectsHandler::tabletToolAxisEvent(TabletToolAxisEvent *event)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->tabletToolAxis(event)) {
            return true;
        }
    }
    return false;
}

bool EffectsHandler::tabletToolTipEvent(TabletToolTipEvent *event)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->tabletToolTip(event)) {
            return true;
        }
    }
    return false;
}

bool EffectsHandler::tabletToolButtonEvent(uint button, bool pressed, InputDeviceTabletTool *tool, std::chrono::microseconds time)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->tabletToolButtonEvent(button, pressed, tool->uniqueId())) {
            return true;
        }
    }
    return false;
}

bool EffectsHandler::tabletPadButtonEvent(uint button, bool pressed, std::chrono::microseconds time, InputDevice *device)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->tabletPadButtonEvent(button, pressed, device)) {
            return true;
        }
    }
    return false;
}

bool EffectsHandler::tabletPadStripEvent(int number, int position, bool isFinger, std::chrono::microseconds time, InputDevice *device)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->tabletPadStripEvent(number, position, isFinger, device)) {
            return true;
        }
    }
    return false;
}

bool EffectsHandler::tabletPadRingEvent(int number, int position, bool isFinger, std::chrono::microseconds time, InputDevice *device)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->tabletPadRingEvent(number, position, isFinger, device)) {
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

bool EffectsHandler::hasKeyboardGrab() const
{
    return keyboard_grab_effect != nullptr;
}

#if KWIN_BUILD_X11
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
#endif

QByteArray EffectsHandler::readRootProperty(long atom, long type, int format) const
{
#if KWIN_BUILD_X11
    if (!kwinApp()->x11Connection()) {
        return QByteArray();
    }
    return readWindowProperty(kwinApp()->x11RootWindow(), atom, type, format);
#else
    return {};
#endif
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
        window->sendToOutput(screen);
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
#if KWIN_BUILD_X11
    if (X11Window *w = Workspace::self()->findClient(Predicate::WindowMatch, id)) {
        return w->effectWindow();
    }
    if (X11Window *w = Workspace::self()->findUnmanaged(id)) {
        return w->effectWindow();
    }
#endif
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

QList<EffectWindow *> EffectsHandler::stackingOrder() const
{
    QList<Window *> list = workspace()->stackingOrder();
    QList<EffectWindow *> ret;
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

QList<EffectWindow *> EffectsHandler::currentTabBoxWindowList() const
{
#if KWIN_BUILD_TABBOX
    const auto clients = workspace()->tabbox()->currentClientList();
    QList<EffectWindow *> ret;
    ret.reserve(clients.size());
    std::transform(std::cbegin(clients), std::cend(clients),
                   std::back_inserter(ret),
                   [](auto client) {
                       return client->effectWindow();
                   });
    return ret;
#else
    return QList<EffectWindow *>();
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

#if KWIN_BUILD_X11
    const QList<QByteArray> properties = m_propertiesForEffects.keys();
    for (const QByteArray &property : properties) {
        removeSupportProperty(property, effect);
    }
#endif

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
        return settings && settings->decorationButtonsLeft().contains(KDecoration3::DecorationButtonType::Close) ? Qt::TopLeftCorner : Qt::TopRightCorner;
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
        shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, mvp);

        if (a != 1.0) {
            shader->setUniform(GLShader::Vec4Uniform::ModulationConstant, QVector4D(a, a, a, a));
        }
        shader->setColorspaceUniforms(ColorDescription::sRGB, renderTarget.colorDescription(), RenderingIntent::Perceptual);

        const bool alphaBlending = w->hasAlphaChannel() || (a != 1.0);
        if (alphaBlending) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }

        t->render(rect.size());

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

} // namespace

#include "moc_effecthandler.cpp"
#include "moc_globals.cpp"
