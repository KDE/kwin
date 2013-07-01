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
// TODO: remove xtest
#include <xcb/xtest.h>
// system
#include <linux/input.h>

namespace KWin
{

KWIN_SINGLETON_FACTORY(InputRedirection)

InputRedirection::InputRedirection(QObject *parent)
    : QObject(parent)
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
    // TODO: keyboard modifiers
    QMouseEvent event(QEvent::MouseMove, m_globalPointer.toPoint(), m_globalPointer.toPoint(),
                      Qt::NoButton, qtButtonStates(), 0);
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

    // TODO: keyboard modifiers
    QMouseEvent event(buttonStateToEvent(state), m_globalPointer.toPoint(), m_globalPointer.toPoint(),
                      buttonToQtMouseButton(button), qtButtonStates(), 0);
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

} // namespace
