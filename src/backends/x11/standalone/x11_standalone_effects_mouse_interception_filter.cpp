/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_standalone_effects_mouse_interception_filter.h"
#include "effects.h"
#include "utils/common.h"

#include <QMouseEvent>

namespace KWin
{

EffectsMouseInterceptionX11Filter::EffectsMouseInterceptionX11Filter(xcb_window_t window, EffectsHandlerImpl *effects)
    : X11EventFilter(QVector<int>{XCB_BUTTON_PRESS, XCB_BUTTON_RELEASE, XCB_MOTION_NOTIFY})
    , m_effects(effects)
    , m_window(window)
{
}

bool EffectsMouseInterceptionX11Filter::event(xcb_generic_event_t *event)
{
    const uint8_t eventType = event->response_type & ~0x80;
    if (eventType == XCB_BUTTON_PRESS || eventType == XCB_BUTTON_RELEASE) {
        auto *me = reinterpret_cast<xcb_button_press_event_t *>(event);
        if (m_window == me->event) {
            const bool isWheel = me->detail >= 4 && me->detail <= 7;
            if (isWheel) {
                if (eventType != XCB_BUTTON_PRESS) {
                    return false;
                }
                QPoint angleDelta;
                switch (me->detail) {
                case 4:
                    angleDelta.setY(120);
                    break;
                case 5:
                    angleDelta.setY(-120);
                    break;
                case 6:
                    angleDelta.setX(120);
                    break;
                case 7:
                    angleDelta.setX(-120);
                    break;
                }

                const Qt::MouseButtons buttons = x11ToQtMouseButtons(me->state);
                const Qt::KeyboardModifiers modifiers = x11ToQtKeyboardModifiers(me->state);

                if (modifiers & Qt::AltModifier) {
                    int x = angleDelta.x();
                    int y = angleDelta.y();

                    angleDelta.setX(y);
                    angleDelta.setY(x);
                    // After Qt > 5.14 simplify to
                    // angleDelta = angleDelta.transposed();
                }

                QWheelEvent ev(QPoint(me->event_x, me->event_y), QCursor::pos(), QPoint(), angleDelta, buttons, modifiers, Qt::NoScrollPhase, false);
                ev.setTimestamp(me->time);
                return m_effects->checkInputWindowEvent(&ev);
            }
            const Qt::MouseButton button = x11ToQtMouseButton(me->detail);
            Qt::MouseButtons buttons = x11ToQtMouseButtons(me->state);
            const QEvent::Type type = (eventType == XCB_BUTTON_PRESS) ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease;
            if (type == QEvent::MouseButtonPress) {
                buttons |= button;
            } else {
                buttons &= ~button;
            }
            QMouseEvent ev(type, QPoint(me->event_x, me->event_y), QPoint(me->root_x, me->root_y),
                           button, buttons, x11ToQtKeyboardModifiers(me->state));
            ev.setTimestamp(me->time);
            return m_effects->checkInputWindowEvent(&ev);
        }
    } else if (eventType == XCB_MOTION_NOTIFY) {
        const auto *me = reinterpret_cast<xcb_motion_notify_event_t *>(event);
        if (m_window == me->event) {
            QMouseEvent ev(QEvent::MouseMove, QPoint(me->event_x, me->event_y), QPoint(me->root_x, me->root_y),
                           Qt::NoButton, x11ToQtMouseButtons(me->state), x11ToQtKeyboardModifiers(me->state));
            ev.setTimestamp(me->time);
            return m_effects->checkInputWindowEvent(&ev);
        }
    }
    return false;
}

}
