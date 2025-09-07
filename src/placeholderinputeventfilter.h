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
    PlaceholderInputEventFilter();
    bool pointerMotion(PointerMotionEvent *event) override;
    bool pointerButton(PointerButtonEvent *event) override;
    bool pointerAxis(PointerAxisEvent *event) override;
    bool keyboardKey(KeyboardKeyEvent *event) override;
    bool touchDown(TouchDownEvent *event) override;
    bool touchMotion(TouchMotionEvent *event) override;
    bool touchUp(TouchUpEvent *event) override;
};

}
