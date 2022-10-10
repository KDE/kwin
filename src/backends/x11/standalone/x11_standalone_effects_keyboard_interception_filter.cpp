/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "x11_standalone_effects_keyboard_interception_filter.h"
#include "x11_standalone_effects.h"
#include "x11_standalone_keyboard.h"

#include <QDebug>
#include <QKeyEvent>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtXkbCommonSupport/private/qxkbcommon_p.h>
#else
#include <QtGui/private/qxkbcommon_p.h>
#endif

namespace KWin
{

EffectsKeyboardInterceptionX11Filter::EffectsKeyboardInterceptionX11Filter(EffectsHandlerImpl *effects, X11Keyboard *keyboard)
    : X11EventFilter(QVector<int>{XCB_KEY_PRESS, XCB_KEY_RELEASE})
    , m_effects(effects)
    , m_keyboard(keyboard)
{
}

bool EffectsKeyboardInterceptionX11Filter::event(xcb_generic_event_t *event)
{
    switch (event->response_type & ~0x80) {
    case XCB_KEY_PRESS: {
        const auto keyEvent = reinterpret_cast<xcb_key_press_event_t *>(event);
        processKey(true, keyEvent->detail, keyEvent->time);
        return true;
    }
    case XCB_KEY_RELEASE: {
        const auto keyEvent = reinterpret_cast<xcb_key_release_event_t *>(event);
        processKey(false, keyEvent->detail, keyEvent->time);
        return true;
    }
    default:
        return false;
    }
}

void EffectsKeyboardInterceptionX11Filter::processKey(bool press, xcb_keycode_t keycode, xcb_timestamp_t timestamp)
{
    const xkb_keysym_t keysym = xkb_state_key_get_one_sym(m_keyboard->xkbState(), keycode);

    Qt::KeyboardModifiers modifiers = m_keyboard->modifiers();
    if (QXkbCommon::isKeypad(keysym)) {
        modifiers |= Qt::KeypadModifier;
    }

    const int qtKey = QXkbCommon::keysymToQtKey(keysym, modifiers, m_keyboard->xkbState(), keycode);
    const QString text = QXkbCommon::lookupString(m_keyboard->xkbState(), keycode);

    QKeyEvent keyEvent(press ? QEvent::KeyPress : QEvent::KeyRelease, qtKey, modifiers, text);
    keyEvent.setTimestamp(timestamp);

    m_effects->grabbedKeyboardEvent(&keyEvent);
}

} // namespace KWin
