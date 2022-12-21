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
    void pointerEvent(KWin::MouseEvent *event) override;
    void wheelEvent(KWin::WheelEvent *event) override;
    void touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    void tabletToolEvent(TabletEvent *event) override;

private:
    void showCursor();
    void hideCursor();

    bool m_cursorHidden = false;
};

}
