/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "input.h"

extern "C" {
struct eis_touch;
};

namespace KWin
{
class EisInputCaptureManager;

class EisInputCaptureFilter : public InputEventFilter
{
public:
    EisInputCaptureFilter(EisInputCaptureManager *m_manager);

    void clearTouches();

    bool pointerMotion(PointerMotionEvent *event) override;
    bool pointerButton(PointerButtonEvent *event) override;
    bool pointerFrame() override;
    bool pointerAxis(PointerAxisEvent *event) override;

    bool keyboardKey(KeyboardKeyEvent *event) override;

    bool touchDown(TouchDownEvent *event) override;
    bool touchMotion(TouchMotionEvent *event) override;
    bool touchUp(TouchUpEvent *event) override;
    bool touchCancel() override;
    bool touchFrame() override;

    bool pinchGestureBegin(PointerPinchGestureBeginEvent *event) override;
    bool pinchGestureUpdate(PointerPinchGestureUpdateEvent *event) override;
    bool pinchGestureEnd(PointerPinchGestureEndEvent *event) override;
    bool pinchGestureCancelled(PointerPinchGestureCancelEvent *event) override;

    bool swipeGestureBegin(PointerSwipeGestureBeginEvent *event) override;
    bool swipeGestureUpdate(PointerSwipeGestureUpdateEvent *event) override;
    bool swipeGestureEnd(PointerSwipeGestureEndEvent *event) override;
    bool swipeGestureCancelled(PointerSwipeGestureCancelEvent *event) override;

    bool holdGestureBegin(PointerHoldGestureBeginEvent *event) override;
    bool holdGestureEnd(PointerHoldGestureEndEvent *event) override;
    bool holdGestureCancelled(PointerHoldGestureCancelEvent *event) override;

private:
    EisInputCaptureManager *m_manager;
    QHash<qint32, eis_touch *> m_touches;
};
}
