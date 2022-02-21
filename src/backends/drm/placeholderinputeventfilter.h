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
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override;
    bool wheelEvent(QWheelEvent *event) override;
    bool keyEvent(QKeyEvent *event) override;
    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchUp(qint32 id, quint32 time) override;
};

}
