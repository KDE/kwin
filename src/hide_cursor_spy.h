/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "input_event_spy.h"

namespace KWin
{

class HideCursorSpy : public InputEventSpy
{
public:
    void pointerMotion(KWin::PointerMotionEvent *event) override;
    void pointerButton(KWin::PointerButtonEvent *event) override;
    void pointerAxis(KWin::PointerAxisEvent *event) override;
    void touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    void tabletToolProximityEvent(KWin::TabletToolProximityEvent *event) override;

private:
    void showCursor();
    void hideCursor();

    bool m_cursorHidden = false;
};

}
