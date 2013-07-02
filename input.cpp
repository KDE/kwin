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
#include "unmanaged.h"
#include "workspace.h"
// KDE
#include <kkeyserver.h>
// TODO: remove xtest
#include <xcb/xtest.h>
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
        qDebug() << "Could not create xkb context";
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
        qDebug() << "Could not map keymap from file";
        return;
    }
    xkb_state *state = xkb_state_new(keymap);
    if (!state) {
        qDebug() << "Could not create XKB state";
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

void Xkb::updateKey(uint32_t key, InputRedirection::KeyboardKeyState state)
{
    if (!m_keymap || !m_state) {
        return;
    }
    xkb_state_update_key(m_state, key + 8, static_cast<xkb_key_direction>(state));
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
{
}

InputRedirection::~InputRedirection()
{
    s_self = NULL;
}

void InputRedirection::updatePointerWindow()
{
    // TODO: handle pointer grab aka popups
    Toplevel *t = findToplevel(m_globalPointer.toPoint());
    const bool oldWindowValid = !m_pointerWindow.isNull();
    if (oldWindowValid && t == m_pointerWindow.data()) {
        return;
    }
    if (oldWindowValid) {
        m_pointerWindow.data()->sendPointerLeaveEvent(m_globalPointer);
    }
    if (!t) {
        m_pointerWindow.clear();
        return;
    }
    m_pointerWindow = QWeakPointer<Toplevel>(t);
    t->sendPointerEnterEvent(m_globalPointer);
}

void InputRedirection::processPointerMotion(const QPointF &pos, uint32_t time)
{
    Q_UNUSED(time)
    // first update to new mouse position
//     const QPointF oldPos = m_globalPointer;
    m_globalPointer = pos;
    emit globalPointerChanged(m_globalPointer);

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
    if (!m_pointerWindow.isNull() && old.data() == m_pointerWindow.data()) {
        m_pointerWindow.data()->sendPointerMoveEvent(pos);
    }

    // TODO: don't use xtest
    // still doing the fake event here as it requires the event to be send on the root window
    xcb_test_fake_input(connection(), XCB_MOTION_NOTIFY, 0, XCB_TIME_CURRENT_TIME, XCB_WINDOW_NONE,
                        pos.toPoint().x(), pos.toPoint().y(), 0);
}

void InputRedirection::processPointerButton(uint32_t button, InputRedirection::PointerButtonState state, uint32_t time)
{
    Q_UNUSED(time)
    m_pointerButtons[button] = state;
    emit pointerButtonStateChanged(button, state);

    QMouseEvent event(buttonStateToEvent(state), m_globalPointer.toPoint(), m_globalPointer.toPoint(),
                      buttonToQtMouseButton(button), qtButtonStates(), keyboardModifiers());
    // check whether an effect has a mouse grab
    if (effects && static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowEvent(&event)) {
        // an effect grabbed the pointer, we do not forward the event to surfaces
        return;
    }
    // TODO: check which part of KWin would like to intercept the event
    if (m_pointerWindow.isNull()) {
        // there is no window which can receive the
        return;
    }
    m_pointerWindow.data()->sendPointerButtonEvent(button, state);
}

void InputRedirection::processPointerAxis(InputRedirection::PointerAxis axis, qreal delta, uint32_t time)
{
    Q_UNUSED(time)
    if (delta == 0) {
        return;
    }
    emit pointerAxisChanged(axis, delta);

    // TODO: check which part of KWin would like to intercept the event
    // TODO: Axis support for effect redirection
    if (m_pointerWindow.isNull()) {
        // there is no window which can receive the
        return;
    }
    m_pointerWindow.data()->sendPointerAxisEvent(axis, delta);
}

void InputRedirection::processKeyboardKey(uint32_t key, InputRedirection::KeyboardKeyState state, uint32_t time)
{
    Q_UNUSED(time)
#if HAVE_XKB
    m_xkb->updateKey(key, state);
    // TODO: process global shortcuts
    // TODO: pass to internal parts of KWin
    if (effects && static_cast< EffectsHandlerImpl* >(effects)->hasKeyboardGrab()) {
        const xkb_keysym_t keysym = m_xkb->toKeysym(key);
        // TODO: start auto-repeat
        // TODO: add modifiers to the event
        const QEvent::Type type = (state == KeyboardKeyPressed) ? QEvent::KeyPress : QEvent::KeyRelease;
        QKeyEvent event(type, m_xkb->toQtKey(keysym), m_xkb->modifiers(), m_xkb->toString(keysym));
        static_cast< EffectsHandlerImpl* >(effects)->grabbedKeyboardEvent(&event);
        return;
    }
    if (Client *c = workspace()->getMovingClient()) {
        // TODO: this does not yet fully support moving of the Client
        // cursor events change the cursor and on Wayland pointer warping is not possible
        c->keyPressEvent(m_xkb->toQtKey(m_xkb->toKeysym(key)));
        return;
    }
#endif
    // check unmanaged
    if (!workspace()->unmanagedList().isEmpty()) {
        // TODO: better check whether this unmanaged should get the key event
        workspace()->unmanagedList().first()->sendKeybordKeyEvent(key, state);
        return;
    }
    if (Client *client = workspace()->activeClient()) {
        client->sendKeybordKeyEvent(key, state);
    }
}

void InputRedirection::processKeyboardModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group)
{
    // TODO: send to proper Client and also send when active Client changes
#if HAVE_XKB
    m_xkb->updateModifiers(modsDepressed, modsLatched, modsLocked, group);
#endif
}

void InputRedirection::processKeymapChange(int fd, uint32_t size)
{
    // TODO: should we pass the keymap to our Clients? Or only to the currently active one and update
#if HAVE_XKB
    m_xkb->installKeymap(fd, size);
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


} // namespace
