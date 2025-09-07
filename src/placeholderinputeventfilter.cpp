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

bool PlaceholderInputEventFilter::pointerMotion(PointerMotionEvent *event)
{
    return true;
}

bool PlaceholderInputEventFilter::pointerButton(PointerButtonEvent *event)
{
    return true;
}

bool PlaceholderInputEventFilter::pointerAxis(PointerAxisEvent *event)
{
    return true;
}

bool PlaceholderInputEventFilter::keyboardKey(KeyboardKeyEvent *event)
{
    return !isMediaKey(event->key);
}

bool PlaceholderInputEventFilter::touchDown(TouchDownEvent *event)
{
    return true;
}

bool PlaceholderInputEventFilter::touchMotion(TouchMotionEvent *event)
{
    return true;
}

bool PlaceholderInputEventFilter::touchUp(TouchUpEvent *event)
{
    return true;
}

}
