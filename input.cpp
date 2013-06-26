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
#include "effects.h"
// TODO: remove xtest
#include <xcb/xtest.h>
// system
#include <linux/input.h>

namespace KWin
{

KWIN_SINGLETON_FACTORY(InputRedirection)

InputRedirection::InputRedirection(QObject *parent)
    : QObject(parent)
{
}

InputRedirection::~InputRedirection()
{
    s_self = NULL;
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
    // TODO: redirect to proper client

    // TODO: don't use xtest
    xcb_test_fake_input(connection(), XCB_MOTION_NOTIFY, 0, XCB_TIME_CURRENT_TIME, XCB_WINDOW_NONE,
                        pos.x(), pos.y(), 0);
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
    // TODO: redirect to proper client

    // TODO: don't use xtest
    uint8_t type = XCB_BUTTON_PRESS;
    if (state == KWin::InputRedirection::PointerButtonReleased) {
        type = XCB_BUTTON_RELEASE;
    }
    // TODO: there must be a better way for mapping
    uint8_t xButton = 0;
    switch (button) {
    case BTN_LEFT:
        xButton = XCB_BUTTON_INDEX_1;
        break;
    case BTN_RIGHT:
        xButton = XCB_BUTTON_INDEX_3;
        break;
    case BTN_MIDDLE:
        xButton = XCB_BUTTON_INDEX_2;
        break;
    default:
        // TODO: add more buttons
        return;
    }
    xcb_test_fake_input(connection(), type, xButton, XCB_TIME_CURRENT_TIME, XCB_WINDOW_NONE, 0, 0, 0);
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
    // TODO: redirect to proper client

    // TODO: don't use xtest
    uint8_t xButton = 0;
    switch (axis) {
    case PointerAxisVertical:
        xButton = delta > 0 ? XCB_BUTTON_INDEX_5 : XCB_BUTTON_INDEX_4;
        break;
    case PointerAxisHorizontal:
        // no enum values defined for buttons larger than 5
        xButton = delta > 0 ? 7 : 6;
        break;
    default:
        // doesn't exist
        return;
    }
    for (int i = 0; i < qAbs(delta); ++i) {
        xcb_test_fake_input(connection(), XCB_BUTTON_PRESS, xButton, XCB_TIME_CURRENT_TIME, XCB_WINDOW_NONE, 0, 0, 0);
        xcb_test_fake_input(connection(), XCB_BUTTON_RELEASE, xButton, XCB_TIME_CURRENT_TIME, XCB_WINDOW_NONE, 0, 0, 0);
    }
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


} // namespace
