/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "placeholderinputeventfilter.h"

namespace KWin
{

bool PlaceholderInputEventFilter::pointerEvent(QMouseEvent *event, quint32 nativeButton)
{
    Q_UNUSED(event)
    Q_UNUSED(nativeButton)
    return true;
}

bool PlaceholderInputEventFilter::wheelEvent(QWheelEvent *event)
{
    Q_UNUSED(event)
    return true;
}

bool PlaceholderInputEventFilter::keyEvent(QKeyEvent *event)
{
    Q_UNUSED(event)
    return true;
}

bool PlaceholderInputEventFilter::touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(pos)
    Q_UNUSED(time)
    return true;
}

bool PlaceholderInputEventFilter::touchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(pos)
    Q_UNUSED(time)
    return true;
}

bool PlaceholderInputEventFilter::touchUp(qint32 id, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(time)
    return true;
}

}
