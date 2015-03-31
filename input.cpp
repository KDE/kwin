/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

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
#include "input.h"
#include "client.h"
#include "effects.h"
#include "globalshortcuts.h"
#include "logind.h"
#include "main.h"
#ifdef KWIN_BUILD_TABBOX
#include "tabbox/tabbox.h"
#endif
#include "unmanaged.h"
#include "screens.h"
#include "workspace.h"
#if HAVE_INPUT
#include "libinput/connection.h"
#endif
#if HAVE_WAYLAND
#include "abstract_backend.h"
#include "wayland_server.h"
#include <KWayland/Server/seat_interface.h>
#endif
// Qt
#include <QKeyEvent>
#include <QMouseEvent>
// KDE
#include <kkeyserver.h>
#if HAVE_XKB
#include <xkbcommon/xkbcommon.h>
#endif
// system
#include <linux/input.h>
#include <sys/mman.h>

namespace KWin
{

#if HAVE_XKB
Xkb::Xkb()
    : m_context(xkb_context_new(static_cast<xkb_context_flags>(0)))
    , m_keymap(NULL)
    , m_state(NULL)
    , m_shiftModifier(0)
    , m_controlModifier(0)
    , m_altModifier(0)
    , m_metaModifier(0)
    , m_modifiers(Qt::NoModifier)
{
    if (!m_context) {
        qCDebug(KWIN_CORE) << "Could not create xkb context";
    } else {
        // load default keymap
        xkb_keymap *keymap = xkb_keymap_new_from_names(m_context, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);
        if (keymap) {
            updateKeymap(keymap);
        } else {
            qCDebug(KWIN_CORE) << "Could not create default xkb keymap";
        }
    }
}

Xkb::~Xkb()
{
    xkb_state_unref(m_state);
    xkb_keymap_unref(m_keymap);
    xkb_context_unref(m_context);
}

void Xkb::installKeymap(int fd, uint32_t size)
{
    if (!m_context) {
        return;
    }
    char *map = reinterpret_cast<char*>(mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0));
    if (map == MAP_FAILED) {
        return;
    }
    xkb_keymap *keymap = xkb_keymap_new_from_string(m_context, map, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_MAP_COMPILE_PLACEHOLDER);
    munmap(map, size);
    if (!keymap) {
        qCDebug(KWIN_CORE) << "Could not map keymap from file";
        return;
    }
    updateKeymap(keymap);
}

void Xkb::updateKeymap(xkb_keymap *keymap)
{
    Q_ASSERT(keymap);
    xkb_state *state = xkb_state_new(keymap);
    if (!state) {
        qCDebug(KWIN_CORE) << "Could not create XKB state";
        xkb_keymap_unref(keymap);
        return;
    }
    // now release the old ones
    xkb_state_unref(m_state);
    xkb_keymap_unref(m_keymap);

    m_keymap = keymap;
    m_state = state;

    m_shiftModifier   = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_SHIFT);
    m_controlModifier = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_CTRL);
    m_altModifier     = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_ALT);
    m_metaModifier    = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_LOGO);
}

void Xkb::updateModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group)
{
    if (!m_keymap || !m_state) {
        return;
    }
    xkb_state_update_mask(m_state, modsDepressed, modsLatched, modsLocked, 0, 0, group);
    updateModifiers();
}

void Xkb::updateKey(uint32_t key, InputRedirection::KeyboardKeyState state)
{
    if (!m_keymap || !m_state) {
        return;
    }
    xkb_state_update_key(m_state, key + 8, static_cast<xkb_key_direction>(state));
    updateModifiers();
}

void Xkb::updateModifiers()
{
    Qt::KeyboardModifiers mods = Qt::NoModifier;
    if (xkb_state_mod_index_is_active(m_state, m_shiftModifier, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::ShiftModifier;
    }
    if (xkb_state_mod_index_is_active(m_state, m_altModifier, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::AltModifier;
    }
    if (xkb_state_mod_index_is_active(m_state, m_controlModifier, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::ControlModifier;
    }
    if (xkb_state_mod_index_is_active(m_state, m_metaModifier, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::MetaModifier;
    }
    m_modifiers = mods;
}

xkb_keysym_t Xkb::toKeysym(uint32_t key)
{
    if (!m_state) {
        return XKB_KEY_NoSymbol;
    }
    return xkb_state_key_get_one_sym(m_state, key + 8);
}

QString Xkb::toString(xkb_keysym_t keysym)
{
    if (!m_state || keysym == XKB_KEY_NoSymbol) {
        return QString();
    }
    QByteArray byteArray(7, 0);
    int ok = xkb_keysym_to_utf8(keysym, byteArray.data(), byteArray.size());
    if (ok == -1 || ok == 0) {
        return QString();
    }
    return QString::fromUtf8(byteArray.constData());
}

Qt::Key Xkb::toQtKey(xkb_keysym_t keysym)
{
    int key = Qt::Key_unknown;
    KKeyServer::symXToKeyQt(keysym, &key);
    return static_cast<Qt::Key>(key);
}
#endif

KWIN_SINGLETON_FACTORY(InputRedirection)

InputRedirection::InputRedirection(QObject *parent)
    : QObject(parent)
#if HAVE_XKB
    , m_xkb(new Xkb())
#endif
    , m_pointerWindow()
    , m_shortcuts(new GlobalShortcutsManager(this))
{
#if HAVE_INPUT
    if (Application::usesLibinput()) {
        LogindIntegration *logind = LogindIntegration::self();
        auto takeControl = [logind, this]() {
            if (logind->hasSessionControl()) {
                setupLibInput();
            } else {
                logind->takeControl();
                m_sessionControlConnection = connect(logind, &LogindIntegration::hasSessionControlChanged, this, &InputRedirection::setupLibInput);
            }
        };
        if (logind->isConnected()) {
            takeControl();
        } else {
            connect(logind, &LogindIntegration::connectedChanged, this, takeControl);
        }
    }
#endif
}

InputRedirection::~InputRedirection()
{
    s_self = NULL;
}

#if HAVE_WAYLAND
static KWayland::Server::SeatInterface *findSeat()
{
    auto server = waylandServer();
    if (!server) {
        return nullptr;
    }
    return server->seat();
}
#endif

void InputRedirection::setupLibInput()
{
#if HAVE_INPUT
    if (!Application::usesLibinput()) {
        return;
    }
    if (m_sessionControlConnection) {
        disconnect(m_sessionControlConnection);
        m_sessionControlConnection = QMetaObject::Connection();
    }
    if (m_libInput) {
        return;
    }
    LibInput::Connection *conn = LibInput::Connection::create(this);
    m_libInput = conn;
    if (conn) {
        conn->setup();
        connect(conn, &LibInput::Connection::pointerButtonChanged, this, &InputRedirection::processPointerButton);
        connect(conn, &LibInput::Connection::pointerAxisChanged, this, &InputRedirection::processPointerAxis);
        connect(conn, &LibInput::Connection::keyChanged, this, &InputRedirection::processKeyboardKey);
        connect(conn, &LibInput::Connection::pointerMotion, this,
            [this] (QPointF delta, uint32_t time) {
                processPointerMotion(m_globalPointer + delta, time);
            }
        );
        connect(conn, &LibInput::Connection::pointerMotionAbsolute, this,
            [this] (QPointF orig, QPointF screen, uint32_t time) {
                Q_UNUSED(orig)
                processPointerMotion(screen, time);
            }
        );
        connect(conn, &LibInput::Connection::touchDown, this, &InputRedirection::processTouchDown);
        connect(conn, &LibInput::Connection::touchUp, this, &InputRedirection::processTouchUp);
        connect(conn, &LibInput::Connection::touchMotion, this, &InputRedirection::processTouchMotion);
        connect(conn, &LibInput::Connection::touchCanceled, this, &InputRedirection::cancelTouch);
        connect(conn, &LibInput::Connection::touchFrame, this, &InputRedirection::touchFrame);
        if (screens()) {
            setupLibInputWithScreens();
        } else {
            connect(kwinApp(), &Application::screensCreated, this, &InputRedirection::setupLibInputWithScreens);
        }
#if HAVE_WAYLAND
        if (auto s = findSeat()) {
            s->setHasKeyboard(conn->hasKeyboard());
            s->setHasPointer(conn->hasPointer());
            s->setHasTouch(conn->hasTouch());
            connect(conn, &LibInput::Connection::hasKeyboardChanged, this,
                [this, s] (bool set) {
                    if (m_libInput->isSuspended()) {
                        return;
                    }
                    s->setHasKeyboard(set);
                }
            );
            connect(conn, &LibInput::Connection::hasPointerChanged, this,
                [this, s] (bool set) {
                    if (m_libInput->isSuspended()) {
                        return;
                    }
                    s->setHasPointer(set);
                }
            );
            connect(conn, &LibInput::Connection::hasTouchChanged, this,
                [this, s] (bool set) {
                    if (m_libInput->isSuspended()) {
                        return;
                    }
                    s->setHasTouch(set);
                }
            );
        }
#endif
    }
#endif
}

void InputRedirection::setupLibInputWithScreens()
{
#if HAVE_INPUT
    if (!screens() || !m_libInput) {
        return;
    }
    m_libInput->setScreenSize(screens()->size());
    connect(screens(), &Screens::sizeChanged, this,
        [this] {
            m_libInput->setScreenSize(screens()->size());
        }
    );
    // set pos to center of all screens
    connect(screens(), &Screens::changed, this, &InputRedirection::updatePointerAfterScreenChange);
    m_globalPointer = screens()->geometry().center();
    emit globalPointerChanged(m_globalPointer);
    // sanitize
    updatePointerAfterScreenChange();
#endif
}

void InputRedirection::updatePointerWindow()
{
    // TODO: handle pointer grab aka popups
    Toplevel *t = findToplevel(m_globalPointer.toPoint());
    auto oldWindow = m_pointerWindow;
    if (!oldWindow.isNull() && t == m_pointerWindow.data()) {
        return;
    }
#if HAVE_WAYLAND
    if (auto seat = findSeat()) {
        // disconnect old surface
        if (oldWindow) {
            disconnect(oldWindow.data(), &Toplevel::geometryChanged, this, &InputRedirection::updateFocusedPointerPosition);
            if (AbstractBackend *b = waylandServer()->backend()) {
                disconnect(seat->focusedPointer()->cursor(), &KWayland::Server::Cursor::changed, b, &AbstractBackend::installCursorFromServer);
            }
        }
        if (t && t->surface()) {
            seat->setFocusedPointerSurface(t->surface(), t->pos());
            connect(t, &Toplevel::geometryChanged, this, &InputRedirection::updateFocusedPointerPosition);
            if (AbstractBackend *b = waylandServer()->backend()) {
                b->installCursorFromServer();
                connect(seat->focusedPointer()->cursor(), &KWayland::Server::Cursor::changed, b, &AbstractBackend::installCursorFromServer);
            }
        } else {
            seat->setFocusedPointerSurface(nullptr);
            t = nullptr;
        }
    }
#endif
    if (!t) {
        m_pointerWindow.clear();
        return;
    }
    m_pointerWindow = QWeakPointer<Toplevel>(t);
}

void InputRedirection::updateFocusedPointerPosition()
{
#if HAVE_WAYLAND
    if (m_pointerWindow.isNull()) {
        return;
    }
    if (workspace()->getMovingClient()) {
        // don't update while moving
        return;
    }
    if (auto seat = findSeat()) {
        if (m_pointerWindow.data()->surface() != seat->focusedPointerSurface()) {
            return;
        }
        seat->setFocusedPointerSurfacePosition(m_pointerWindow.data()->pos());
    }
#endif
}

void InputRedirection::updateFocusedTouchPosition()
{
#if HAVE_WAYLAND
    if (m_touchWindow.isNull()) {
        return;
    }
    if (auto seat = findSeat()) {
        if (m_touchWindow.data()->surface() != seat->focusedTouchSurface()) {
            return;
        }
        seat->setFocusedTouchSurfacePosition(m_touchWindow.data()->pos());
    }
#endif
}

void InputRedirection::processPointerMotion(const QPointF &pos, uint32_t time)
{
    // first update to new mouse position
//     const QPointF oldPos = m_globalPointer;
    updatePointerPosition(pos);

    // TODO: check which part of KWin would like to intercept the event
    QMouseEvent event(QEvent::MouseMove, m_globalPointer.toPoint(), m_globalPointer.toPoint(),
                      Qt::NoButton, qtButtonStates(), keyboardModifiers());
    // check whether an effect has a mouse grab
    if (effects && static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowEvent(&event)) {
        // an effect grabbed the pointer, we do not forward the event to surfaces
        return;
    }
    QWeakPointer<Toplevel> old = m_pointerWindow;
    updatePointerWindow();
#if HAVE_WAYLAND
    if (auto seat = findSeat()) {
        seat->setTimestamp(time);
        seat->setPointerPos(pos);
    }
#endif
}

void InputRedirection::processPointerButton(uint32_t button, InputRedirection::PointerButtonState state, uint32_t time)
{
    m_pointerButtons[button] = state;
    emit pointerButtonStateChanged(button, state);

    QMouseEvent event(buttonStateToEvent(state), m_globalPointer.toPoint(), m_globalPointer.toPoint(),
                      buttonToQtMouseButton(button), qtButtonStates(), keyboardModifiers());
    // check whether an effect has a mouse grab
    if (effects && static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowEvent(&event)) {
        // an effect grabbed the pointer, we do not forward the event to surfaces
        return;
    }
#if HAVE_XKB
    if (state == KWin::InputRedirection::PointerButtonPressed) {
        if (m_shortcuts->processPointerPressed(m_xkb->modifiers(), qtButtonStates())) {
            return;
        }
    }
#endif
    // TODO: check which part of KWin would like to intercept the event
#if HAVE_WAYLAND
    if (auto seat = findSeat()) {
        seat->setTimestamp(time);
        state == PointerButtonPressed ? seat->pointerButtonPressed(button) : seat->pointerButtonReleased(button);
    }
#endif
}

void InputRedirection::processPointerAxis(InputRedirection::PointerAxis axis, qreal delta, uint32_t time)
{
    if (delta == 0) {
        return;
    }
    emit pointerAxisChanged(axis, delta);
#if HAVE_XKB
    if (m_xkb->modifiers() != Qt::NoModifier) {
        PointerAxisDirection direction = PointerAxisUp;
        if (axis == PointerAxisHorizontal) {
            if (delta > 0) {
                direction = PointerAxisUp;
            } else {
                direction = PointerAxisDown;
            }
        } else {
            if (delta > 0) {
                direction = PointerAxisLeft;
            } else {
                direction = PointerAxisRight;
            }
        }
        if (m_shortcuts->processAxis(m_xkb->modifiers(), direction)) {
            return;
        }
    }
#endif

    // TODO: check which part of KWin would like to intercept the event
    // TODO: Axis support for effect redirection
#if HAVE_WAYLAND
    if (auto seat = findSeat()) {
        seat->setTimestamp(time);
        seat->pointerAxis(axis == InputRedirection::PointerAxisHorizontal ? Qt::Horizontal : Qt::Vertical, delta);
    }
#endif
}

void InputRedirection::processKeyboardKey(uint32_t key, InputRedirection::KeyboardKeyState state, uint32_t time)
{
#if HAVE_XKB
    const Qt::KeyboardModifiers oldMods = keyboardModifiers();
    m_xkb->updateKey(key, state);
    if (oldMods != keyboardModifiers()) {
        emit keyboardModifiersChanged(keyboardModifiers(), oldMods);
    }
    // TODO: pass to internal parts of KWin
#ifdef KWIN_BUILD_TABBOX
    if (TabBox::TabBox::self() && TabBox::TabBox::self()->isGrabbed()) {
        if (state == KWin::InputRedirection::KeyboardKeyPressed) {
            TabBox::TabBox::self()->keyPress(m_xkb->modifiers() | m_xkb->toQtKey(m_xkb->toKeysym(key)));
        }
        return;
    }
#endif
    if (effects && static_cast< EffectsHandlerImpl* >(effects)->hasKeyboardGrab()) {
        const xkb_keysym_t keysym = m_xkb->toKeysym(key);
        // TODO: start auto-repeat
        // TODO: add modifiers to the event
        const QEvent::Type type = (state == KeyboardKeyPressed) ? QEvent::KeyPress : QEvent::KeyRelease;
        QKeyEvent event(type, m_xkb->toQtKey(keysym), m_xkb->modifiers(), m_xkb->toString(keysym));
        static_cast< EffectsHandlerImpl* >(effects)->grabbedKeyboardEvent(&event);
        return;
    }
    if (workspace()) {
        if (Client *c = workspace()->getMovingClient()) {
            // TODO: this does not yet fully support moving of the Client
            // cursor events change the cursor and on Wayland pointer warping is not possible
            c->keyPressEvent(m_xkb->toQtKey(m_xkb->toKeysym(key)));
            return;
        }
    }
    // process global shortcuts
    if (state == KeyboardKeyPressed) {
        if (m_shortcuts->processKey(m_xkb->modifiers(), m_xkb->toKeysym(key))) {
            return;
        }
    }
#endif
#if HAVE_WAYLAND
    if (auto seat = findSeat()) {
        seat->setTimestamp(time);
        // TODO: this needs better integration
        // check unmanaged
        Toplevel *t = nullptr;
        if (!workspace()->unmanagedList().isEmpty()) {
            // TODO: better check whether this unmanaged should get the key event
            t = workspace()->unmanagedList().first();
        }
        if (!t) {
            t = workspace()->activeClient();
        }
        if (t && t->surface()) {
            if (t->surface() != seat->focusedKeyboardSurface()) {
                seat->setFocusedKeyboardSurface(t->surface());
            }
            state == InputRedirection::KeyboardKeyPressed ? seat->keyPressed(key) : seat->keyReleased(key);
        }
    }
#endif
}

void InputRedirection::processKeyboardModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group)
{
    // TODO: send to proper Client and also send when active Client changes
#if HAVE_XKB
    Qt::KeyboardModifiers oldMods = keyboardModifiers();
    m_xkb->updateModifiers(modsDepressed, modsLatched, modsLocked, group);
    if (oldMods != keyboardModifiers()) {
        emit keyboardModifiersChanged(keyboardModifiers(), oldMods);
    }
#else
    Q_UNUSED(modsDepressed)
    Q_UNUSED(modsLatched)
    Q_UNUSED(modsLocked)
    Q_UNUSED(group)
#endif
}

void InputRedirection::processKeymapChange(int fd, uint32_t size)
{
    // TODO: should we pass the keymap to our Clients? Or only to the currently active one and update
#if HAVE_XKB
    m_xkb->installKeymap(fd, size);
#else
    Q_UNUSED(fd)
    Q_UNUSED(size)
#endif
}

void InputRedirection::processTouchDown(qint32 id, const QPointF &pos, quint32 time)
{
    // TODO: internal handling?
#if HAVE_WAYLAND
    if (auto seat = findSeat()) {
        seat->setTimestamp(time);
        if (!seat->isTouchSequence()) {
            updateTouchWindow(pos);
        }
        m_touchIdMapper.insert(id, seat->touchDown(pos));
    }
#else
    Q_UNUSED(id)
    Q_UNUSED(pos)
    Q_UNUSED(time)
#endif
}

void InputRedirection::updateTouchWindow(const QPointF &pos)
{
    // TODO: handle pointer grab aka popups
    Toplevel *t = findToplevel(pos.toPoint());
    auto oldWindow = m_touchWindow;
    if (!oldWindow.isNull() && t == oldWindow.data()) {
        return;
    }
#if HAVE_WAYLAND
    if (auto seat = findSeat()) {
        // disconnect old surface
        if (oldWindow) {
            disconnect(oldWindow.data(), &Toplevel::geometryChanged, this, &InputRedirection::updateFocusedTouchPosition);
        }
        if (t && t->surface()) {
            seat->setFocusedTouchSurface(t->surface(), t->pos());
            connect(t, &Toplevel::geometryChanged, this, &InputRedirection::updateFocusedTouchPosition);
        } else {
            seat->setFocusedTouchSurface(nullptr);
            t = nullptr;
        }
    }
#endif
    if (!t) {
        m_touchWindow.clear();
        return;
    }
    m_touchWindow = QWeakPointer<Toplevel>(t);
}


void InputRedirection::processTouchUp(qint32 id, quint32 time)
{
    // TODO: internal handling?
#if HAVE_WAYLAND
    if (auto seat = findSeat()) {
        auto it = m_touchIdMapper.constFind(id);
        if (it != m_touchIdMapper.constEnd()) {
            seat->setTimestamp(time);
            seat->touchUp(it.value());
        }
    }
#else
    Q_UNUSED(id)
    Q_UNUSED(time)
#endif
}

void InputRedirection::processTouchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    // TODO: internal handling?
#if HAVE_WAYLAND
    if (auto seat = findSeat()) {
        seat->setTimestamp(time);
        auto it = m_touchIdMapper.constFind(id);
        if (it != m_touchIdMapper.constEnd()) {
            seat->setTimestamp(time);
            seat->touchMove(it.value(), pos);
        }
    }
#else
    Q_UNUSED(id)
    Q_UNUSED(pos)
    Q_UNUSED(time)
#endif
}

void InputRedirection::cancelTouch()
{
#if HAVE_WAYLAND
    if (auto seat = findSeat()) {
        seat->cancelTouchSequence();
    }
#endif
}

void InputRedirection::touchFrame()
{
#if HAVE_WAYLAND
    if (auto seat = findSeat()) {
        seat->touchFrame();
    }
#endif
}

QEvent::Type InputRedirection::buttonStateToEvent(InputRedirection::PointerButtonState state)
{
    switch (state) {
    case KWin::InputRedirection::PointerButtonReleased:
        return QEvent::MouseButtonRelease;
    case KWin::InputRedirection::PointerButtonPressed:
        return QEvent::MouseButtonPress;
    }
    return QEvent::None;
}

Qt::MouseButton InputRedirection::buttonToQtMouseButton(uint32_t button)
{
    switch (button) {
    case BTN_LEFT:
        return Qt::LeftButton;
    case BTN_MIDDLE:
        return Qt::MiddleButton;
    case BTN_RIGHT:
        return Qt::RightButton;
    case BTN_BACK:
        return Qt::XButton1;
    case BTN_FORWARD:
        return Qt::XButton2;
    }
    return Qt::NoButton;
}

Qt::MouseButtons InputRedirection::qtButtonStates() const
{
    Qt::MouseButtons buttons;
    for (auto it = m_pointerButtons.constBegin(); it != m_pointerButtons.constEnd(); ++it) {
        if (it.value() == KWin::InputRedirection::PointerButtonReleased) {
            continue;
        }
        Qt::MouseButton button = buttonToQtMouseButton(it.key());
        if (button != Qt::NoButton) {
            buttons |= button;
        }
    }
    return buttons;
}

Toplevel *InputRedirection::findToplevel(const QPoint &pos)
{
    if (!Workspace::self()) {
        return nullptr;
    }
    // TODO: check whether the unmanaged wants input events at all
    const UnmanagedList &unmanaged = Workspace::self()->unmanagedList();
    foreach (Unmanaged *u, unmanaged) {
        if (u->geometry().contains(pos)) {
            return u;
        }
    }
    const ToplevelList &stacking = Workspace::self()->stackingOrder();
    if (stacking.isEmpty()) {
        return NULL;
    }
    auto it = stacking.end();
    do {
        --it;
        Toplevel *t = (*it);
        if (t->isDeleted()) {
            // a deleted window doesn't get mouse events
            continue;
        }
        if (t->isClient()) {
            Client *c = static_cast<Client*>(t);
            if (!c->isOnCurrentActivity() || !c->isOnCurrentDesktop() || c->isMinimized() || !c->isCurrentTab()) {
                continue;
            }
        }
        if (t->geometry().contains(pos)) {
            return t;
        }
    } while (it != stacking.begin());
    return NULL;
}

uint8_t InputRedirection::toXPointerButton(uint32_t button)
{
    switch (button) {
    case BTN_LEFT:
        return XCB_BUTTON_INDEX_1;
    case BTN_RIGHT:
        return XCB_BUTTON_INDEX_3;
    case BTN_MIDDLE:
        return XCB_BUTTON_INDEX_2;
    default:
        // TODO: add more buttons
        return XCB_BUTTON_INDEX_ANY;
    }
}

uint8_t InputRedirection::toXPointerButton(InputRedirection::PointerAxis axis, qreal delta)
{
    switch (axis) {
    case PointerAxisVertical:
        if (delta < 0) {
            return 4;
        } else {
            return 5;
        }
    case PointerAxisHorizontal:
        if (delta < 0) {
            return 6;
        } else {
            return 7;
        }
    }
    return XCB_BUTTON_INDEX_ANY;
}

Qt::KeyboardModifiers InputRedirection::keyboardModifiers() const
{
#if HAVE_XKB
    return m_xkb->modifiers();
#else
    return Qt::NoModifier;
#endif
}

void InputRedirection::registerShortcut(const QKeySequence &shortcut, QAction *action)
{
    m_shortcuts->registerShortcut(action, shortcut);
    registerShortcutForGlobalAccelTimestamp(action);
}

void InputRedirection::registerPointerShortcut(Qt::KeyboardModifiers modifiers, Qt::MouseButton pointerButtons, QAction *action)
{
    m_shortcuts->registerPointerShortcut(action, modifiers, pointerButtons);
}

void InputRedirection::registerAxisShortcut(Qt::KeyboardModifiers modifiers, PointerAxisDirection axis, QAction *action)
{
    m_shortcuts->registerAxisShortcut(action, modifiers, axis);
}

void InputRedirection::registerShortcutForGlobalAccelTimestamp(QAction *action)
{
    connect(action, &QAction::triggered, kwinApp(), [action] {
        QVariant timestamp = action->property("org.kde.kglobalaccel.activationTimestamp");
        bool ok = false;
        const quint32 t = timestamp.toULongLong(&ok);
        if (ok) {
            kwinApp()->setX11Time(t);
        }
    });
}

static bool screenContainsPos(const QPointF &pos)
{
    for (int i = 0; i < screens()->count(); ++i) {
        if (screens()->geometry(i).contains(pos.toPoint())) {
            return true;
        }
    }
    return false;
}

void InputRedirection::updatePointerPosition(const QPointF &pos)
{
    // verify that at least one screen contains the pointer position
    if (!screenContainsPos(pos)) {
        return;
    }
    m_globalPointer = pos;
    emit globalPointerChanged(m_globalPointer);
}

void InputRedirection::updatePointerAfterScreenChange()
{
    if (screenContainsPos(m_globalPointer)) {
        // pointer still on a screen
        return;
    }
    // pointer no longer on a screen, reposition to closes screen
    m_globalPointer = screens()->geometry(screens()->number(m_globalPointer.toPoint())).center();
    emit globalPointerChanged(m_globalPointer);
}

} // namespace
