/*
    SPDX-FileCopyrightText: 2024 Jin Liu <m.liu.jin@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effect.h"
#include "input_event_spy.h"

#include <QTimer>

namespace KWin
{

class Cursor;

class HideCursorEffect : public Effect, public InputEventSpy
{
    Q_OBJECT

public:
    HideCursorEffect();
    ~HideCursorEffect() override;

    static bool supported();

    void reconfigure(ReconfigureFlags flags) override;
    bool isActive() const override;

    void pointerEvent(MouseEvent *event) override;
    void keyEvent(KeyEvent *event) override;

private:
    void showCursor();
    void hideCursor();

    int m_inactivityDuration;
    bool m_hideOnTyping;

    Cursor *m_cursor;
    bool m_cursorHidden = false;
    QTimer m_inactivityTimer;
};

} // namespace KWin
