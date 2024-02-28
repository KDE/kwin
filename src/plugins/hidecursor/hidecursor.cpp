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

void HideCursorEffect::pointerEvent(MouseEvent *event)
{
    showCursor();
    if (m_inactivityDuration > 0) {
        m_inactivityTimer.start(m_inactivityDuration);
    }
}

void HideCursorEffect::keyEvent(KeyEvent *event)
{
    if (m_hideOnTyping && event->type() == QEvent::KeyPress) {
        auto key = event->key();
        switch (key) {
        case Qt::Key_Shift:
        case Qt::Key_Control:
        case Qt::Key_Meta:
        case Qt::Key_Alt:
        case Qt::Key_AltGr:
        case Qt::Key_Super_L:
        case Qt::Key_Super_R:
        case Qt::Key_Hyper_L:
        case Qt::Key_Hyper_R:
        case Qt::Key_Escape:
            break;

        default:
            hideCursor();
        }
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
