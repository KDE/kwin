/*
    SPDX-FileCopyrightText: 2024 Jin Liu <m.liu.jin@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "hidecursor.h"
#include "cursor.h"
#include "effect/effecthandler.h"
#include "hidecursorconfig.h"
#include "input_event.h"

namespace KWin
{

HideCursorEffect::HideCursorEffect()
    : m_cursor(Cursors::self()->mouse())
{
    input()->installInputEventSpy(this);

    m_inactivityTimer.setSingleShot(true);
    connect(&m_inactivityTimer, &QTimer::timeout, this, [this]() {
        hideCursor();
    });

    HideCursorConfig::instance(effects->config());
    reconfigure(ReconfigureAll);
}

HideCursorEffect::~HideCursorEffect()
{
    showCursor();
}

bool HideCursorEffect::supported()
{
    return effects->waylandDisplay();
}

void HideCursorEffect::reconfigure(ReconfigureFlags flags)
{
    HideCursorConfig::self()->read();
    m_inactivityDuration = HideCursorConfig::inactivityDuration() * 1000;
    m_hideOnTyping = HideCursorConfig::hideOnTyping();

    m_inactivityTimer.stop();
    showCursor();
    if (m_inactivityDuration > 0) {
        m_inactivityTimer.start(m_inactivityDuration);
    }
}

bool HideCursorEffect::isActive() const
{
    return false;
}

void HideCursorEffect::activity()
{
    showCursor();
    if (m_inactivityDuration > 0) {
        m_inactivityTimer.start(m_inactivityDuration);
    }
}

void HideCursorEffect::pointerMotion(PointerMotionEvent *event)
{
    activity();
}

void HideCursorEffect::pointerButton(PointerButtonEvent *event)
{
    activity();
}

void HideCursorEffect::tabletToolProximityEvent(TabletEvent *event)
{
    activity();
}

void HideCursorEffect::tabletToolAxisEvent(TabletEvent *event)
{
    activity();
}

void HideCursorEffect::tabletToolTipEvent(TabletEvent *event)
{
    activity();
}

void HideCursorEffect::keyboardKey(KeyboardKeyEvent *event)
{
    // All functional keys have a Qt key code greater than 0x01000000
    // https://doc.qt.io/qt-6/qt.html#Key-enum
    // We don't want to hide the cursor when the user presses a functional key, since they are
    // usually interleaved with mouse movements.
    if (m_hideOnTyping && !m_cursorHidden && event->state == KeyboardKeyState::Pressed && event->key < 0x01000000
        && (event->modifiers == Qt::NoModifier || event->modifiers == Qt::ShiftModifier)) {
        hideCursor();
    }
}

void HideCursorEffect::showCursor()
{
    if (m_cursorHidden) {
        effects->showCursor();
        m_cursorHidden = false;
    }
}

void HideCursorEffect::hideCursor()
{
    if (!m_cursorHidden) {
        effects->hideCursor();
        m_cursorHidden = true;
    }
}

} // namespace KWin

#include "moc_hidecursor.cpp"
