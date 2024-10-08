/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "placeholderinputeventfilter.h"
#include "input_event.h"
#include "utils/keys.h"

namespace KWin
{

PlaceholderInputEventFilter::PlaceholderInputEventFilter()
    : InputEventFilter(InputFilterOrder::PlaceholderOutput)
{
}

bool PlaceholderInputEventFilter::pointerEvent(MouseEvent *event, quint32 nativeButton)
{
    return true;
}

bool PlaceholderInputEventFilter::wheelEvent(WheelEvent *event)
{
    return true;
}

bool PlaceholderInputEventFilter::keyEvent(KeyEvent *event)
{
    return !isMediaKey(event->key());
}

bool PlaceholderInputEventFilter::touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    return true;
}

bool PlaceholderInputEventFilter::touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    return true;
}

bool PlaceholderInputEventFilter::touchUp(qint32 id, std::chrono::microseconds time)
{
    return true;
}

}
