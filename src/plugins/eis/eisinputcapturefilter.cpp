/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "eisinputcapturefilter.h"

#include "eisinputcapture.h"
#include "eisinputcapturemanager.h"

#include "input_event.h"

#include <libeis.h>

namespace KWin
{
EisInputCaptureFilter::EisInputCaptureFilter(EisInputCaptureManager *manager)
    : InputEventFilter(InputFilterOrder::EisInput)
    , m_manager(manager)
{
}

void EisInputCaptureFilter::clearTouches()
{
    for (const auto touch : m_touches) {
        eis_touch_unref(touch);
    }
    m_touches.clear();
}

bool EisInputCaptureFilter::pointerMotion(PointerMotionEvent *event)
{
    if (!m_manager->activeCapture()) {
        return false;
    }
    if (const auto pointer = m_manager->activeCapture()->pointer()) {
        eis_device_pointer_motion(pointer, event->delta.x(), event->delta.y());
    }
    return true;
}

bool EisInputCaptureFilter::pointerButton(PointerButtonEvent *event)
{
    if (!m_manager->activeCapture()) {
        return false;
    }
    if (const auto pointer = m_manager->activeCapture()->pointer()) {
        eis_device_button_button(pointer, event->nativeButton, event->state == PointerButtonState::Pressed);
    }
    return true;
}

bool EisInputCaptureFilter::pointerFrame()
{
    if (!m_manager->activeCapture()) {
        return false;
    }
    if (const auto pointer = m_manager->activeCapture()->pointer()) {
        eis_device_frame(pointer, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    return true;
}

bool EisInputCaptureFilter::pointerAxis(PointerAxisEvent *event)
{
    if (!m_manager->activeCapture()) {
        return false;
    }
    if (const auto pointer = m_manager->activeCapture()->pointer()) {
        if (event->delta) {
            if (event->deltaV120) {
                if (event->orientation == Qt::Horizontal) {
                    eis_device_scroll_discrete(pointer, event->deltaV120, 0);
                } else {
                    eis_device_scroll_discrete(pointer, 0, event->deltaV120);
                }
            } else {
                if (event->orientation == Qt::Horizontal) {
                    eis_device_scroll_delta(pointer, event->delta, 0);
                } else {
                    eis_device_scroll_delta(pointer, 0, event->delta);
                }
            }
        } else {
            if (event->orientation == Qt::Horizontal) {
                eis_device_scroll_stop(pointer, true, false);
            } else {
                eis_device_scroll_stop(pointer, false, true);
            }
        }
    }
    return true;
}

bool EisInputCaptureFilter::keyboardKey(KeyboardKeyEvent *event)
{
    if (!m_manager->activeCapture()) {
        return false;
    }
    if (const auto keyboard = m_manager->activeCapture()->keyboard()) {
        eis_device_keyboard_key(keyboard, event->nativeScanCode, event->state != KeyboardKeyState::Released);
        eis_device_frame(keyboard, std::chrono::duration_cast<std::chrono::milliseconds>(event->timestamp).count());
    }
    return true;
}

bool EisInputCaptureFilter::touchDown(TouchDownEvent *event)
{
    if (!m_manager->activeCapture()) {
        return false;
    }
    if (const auto abs = m_manager->activeCapture()->absoluteDevice()) {
        auto touch = eis_device_touch_new(abs);
        m_touches.insert(event->id, touch);
        eis_touch_down(touch, event->pos.x(), event->pos.y());
    }
    return true;
}

bool EisInputCaptureFilter::touchMotion(TouchMotionEvent *event)
{
    if (!m_manager->activeCapture()) {
        return false;
    }
    if (auto touch = m_touches.value(event->id)) {
        eis_touch_motion(touch, event->pos.x(), event->pos.y());
    }
    return true;
}

bool EisInputCaptureFilter::touchUp(TouchUpEvent *event)
{
    if (!m_manager->activeCapture()) {
        return false;
    }
    if (auto touch = m_touches.take(event->id)) {
        eis_touch_up(touch);
        eis_touch_unref(touch);
    }
    return false;
}
bool EisInputCaptureFilter::touchCancel()
{
    if (!m_manager->activeCapture()) {
        return false;
    }
    return true;
}
bool EisInputCaptureFilter::touchFrame()
{
    if (!m_manager->activeCapture()) {
        return false;
    }
    if (const auto abs = m_manager->activeCapture()->absoluteDevice()) {
        eis_device_frame(abs, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    return true;
}

bool EisInputCaptureFilter::pinchGestureBegin(PointerPinchGestureBeginEvent *event)
{
    if (!m_manager->activeCapture()) {
        return false;
    }
    return true;
}
bool EisInputCaptureFilter::pinchGestureUpdate(PointerPinchGestureUpdateEvent *event)
{
    if (!m_manager->activeCapture()) {
        return false;
    }
    return true;
}

bool EisInputCaptureFilter::pinchGestureEnd(PointerPinchGestureEndEvent *event)
{
    if (!m_manager->activeCapture()) {
        return false;
    }
    return true;
}
bool EisInputCaptureFilter::pinchGestureCancelled(PointerPinchGestureCancelEvent *event)
{
    if (!m_manager->activeCapture()) {
        return false;
    }
    return true;
}

bool EisInputCaptureFilter::swipeGestureBegin(PointerSwipeGestureBeginEvent *event)
{
    if (!m_manager->activeCapture()) {
        return false;
    }
    return true;
}
bool EisInputCaptureFilter::swipeGestureUpdate(PointerSwipeGestureUpdateEvent *event)
{
    if (!m_manager->activeCapture()) {
        return false;
    }
    return true;
}
bool EisInputCaptureFilter::swipeGestureEnd(PointerSwipeGestureEndEvent *event)
{
    if (!m_manager->activeCapture()) {
        return false;
    }
    return true;
}
bool EisInputCaptureFilter::swipeGestureCancelled(PointerSwipeGestureCancelEvent *event)
{
    if (!m_manager->activeCapture()) {
        return false;
    }
    return true;
}

bool EisInputCaptureFilter::holdGestureBegin(PointerHoldGestureBeginEvent *event)
{
    if (!m_manager->activeCapture()) {
        return false;
    }
    return true;
}
bool EisInputCaptureFilter::holdGestureEnd(PointerHoldGestureEndEvent *event)
{
    if (!m_manager->activeCapture()) {
        return false;
    }
    return true;
}
bool EisInputCaptureFilter::holdGestureCancelled(PointerHoldGestureCancelEvent *event)
{
    if (!m_manager->activeCapture()) {
        return false;
    }
    return true;
}
}
