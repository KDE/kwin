/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "input.h"

namespace KWin
{

class PlaceholderInputEventFilter : public InputEventFilter
{
public:
    bool pointerEvent(MouseEvent *event, quint32 nativeButton) override;
    bool wheelEvent(WheelEvent *event) override;
    bool keyEvent(KeyEvent *event) override;
    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchUp(qint32 id, std::chrono::microseconds time) override;
};

}
