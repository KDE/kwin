/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "events.h"
#include "device.h"

#include <QSize>

namespace KWin
{
namespace LibInput
{

std::unique_ptr<Event> Event::create(libinput_event *event)
{
    if (!event) {
        return nullptr;
    }
    const auto t = libinput_event_get_type(event);
    // TODO: add touch events
    // TODO: add device notify events
    switch (t) {
    case LIBINPUT_EVENT_KEYBOARD_KEY:
        return std::make_unique<KeyEvent>(event);
    case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
    case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
    case LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS:
    case LIBINPUT_EVENT_POINTER_BUTTON:
    case LIBINPUT_EVENT_POINTER_MOTION:
    case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
        return std::make_unique<PointerEvent>(event, t);
    case LIBINPUT_EVENT_TOUCH_DOWN:
    case LIBINPUT_EVENT_TOUCH_UP:
    case LIBINPUT_EVENT_TOUCH_MOTION:
    case LIBINPUT_EVENT_TOUCH_CANCEL:
    case LIBINPUT_EVENT_TOUCH_FRAME:
        return std::make_unique<TouchEvent>(event, t);
    case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN:
    case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE:
    case LIBINPUT_EVENT_GESTURE_SWIPE_END:
        return std::make_unique<SwipeGestureEvent>(event, t);
    case LIBINPUT_EVENT_GESTURE_PINCH_BEGIN:
    case LIBINPUT_EVENT_GESTURE_PINCH_UPDATE:
    case LIBINPUT_EVENT_GESTURE_PINCH_END:
        return std::make_unique<PinchGestureEvent>(event, t);
    case LIBINPUT_EVENT_GESTURE_HOLD_BEGIN:
    case LIBINPUT_EVENT_GESTURE_HOLD_END:
        return std::make_unique<HoldGestureEvent>(event, t);
    case LIBINPUT_EVENT_TABLET_TOOL_AXIS:
    case LIBINPUT_EVENT_TABLET_TOOL_PROXIMITY:
    case LIBINPUT_EVENT_TABLET_TOOL_TIP:
        return std::make_unique<TabletToolEvent>(event, t);
    case LIBINPUT_EVENT_TABLET_TOOL_BUTTON:
        return std::make_unique<TabletToolButtonEvent>(event, t);
    case LIBINPUT_EVENT_TABLET_PAD_RING:
        return std::make_unique<TabletPadRingEvent>(event, t);
    case LIBINPUT_EVENT_TABLET_PAD_STRIP:
        return std::make_unique<TabletPadStripEvent>(event, t);
    case LIBINPUT_EVENT_TABLET_PAD_BUTTON:
        return std::make_unique<TabletPadButtonEvent>(event, t);
    case LIBINPUT_EVENT_SWITCH_TOGGLE:
        return std::make_unique<SwitchEvent>(event, t);
    default:
        return std::unique_ptr<Event>{new Event(event, t)};
    }
}

Event::Event(libinput_event *event, libinput_event_type type)
    : m_event(event)
    , m_type(type)
    , m_device(nullptr)
{
}

Event::~Event()
{
    libinput_event_destroy(m_event);
}

Device *Event::device() const
{
    if (!m_device) {
        m_device = Device::get(libinput_event_get_device(m_event));
    }
    return m_device;
}

libinput_device *Event::nativeDevice() const
{
    if (m_device) {
        return m_device->device();
    }
    return libinput_event_get_device(m_event);
}

KeyEvent::KeyEvent(libinput_event *event)
    : Event(event, LIBINPUT_EVENT_KEYBOARD_KEY)
    , m_keyboardEvent(libinput_event_get_keyboard_event(event))
{
}

KeyEvent::~KeyEvent() = default;

uint32_t KeyEvent::key() const
{
    return libinput_event_keyboard_get_key(m_keyboardEvent);
}

InputRedirection::KeyboardKeyState KeyEvent::state() const
{
    switch (libinput_event_keyboard_get_key_state(m_keyboardEvent)) {
    case LIBINPUT_KEY_STATE_PRESSED:
        return InputRedirection::KeyboardKeyPressed;
    case LIBINPUT_KEY_STATE_RELEASED:
        return InputRedirection::KeyboardKeyReleased;
    default:
        Q_UNREACHABLE();
    }
}

std::chrono::microseconds KeyEvent::time() const
{
    return std::chrono::microseconds(libinput_event_keyboard_get_time_usec(m_keyboardEvent));
}

PointerEvent::PointerEvent(libinput_event *event, libinput_event_type type)
    : Event(event, type)
    , m_pointerEvent(libinput_event_get_pointer_event(event))
{
}

PointerEvent::~PointerEvent() = default;

QPointF PointerEvent::absolutePos() const
{
    Q_ASSERT(type() == LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE);
    return QPointF(libinput_event_pointer_get_absolute_x(m_pointerEvent),
                   libinput_event_pointer_get_absolute_y(m_pointerEvent));
}

QPointF PointerEvent::absolutePos(const QSize &size) const
{
    Q_ASSERT(type() == LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE);
    return QPointF(libinput_event_pointer_get_absolute_x_transformed(m_pointerEvent, size.width()),
                   libinput_event_pointer_get_absolute_y_transformed(m_pointerEvent, size.height()));
}

QPointF PointerEvent::delta() const
{
    Q_ASSERT(type() == LIBINPUT_EVENT_POINTER_MOTION);
    return QPointF(libinput_event_pointer_get_dx(m_pointerEvent), libinput_event_pointer_get_dy(m_pointerEvent));
}

QPointF PointerEvent::deltaUnaccelerated() const
{
    Q_ASSERT(type() == LIBINPUT_EVENT_POINTER_MOTION);
    return QPointF(libinput_event_pointer_get_dx_unaccelerated(m_pointerEvent), libinput_event_pointer_get_dy_unaccelerated(m_pointerEvent));
}

std::chrono::microseconds PointerEvent::time() const
{
    return std::chrono::microseconds(libinput_event_pointer_get_time_usec(m_pointerEvent));
}

uint32_t PointerEvent::button() const
{
    Q_ASSERT(type() == LIBINPUT_EVENT_POINTER_BUTTON);
    return libinput_event_pointer_get_button(m_pointerEvent);
}

InputRedirection::PointerButtonState PointerEvent::buttonState() const
{
    Q_ASSERT(type() == LIBINPUT_EVENT_POINTER_BUTTON);
    switch (libinput_event_pointer_get_button_state(m_pointerEvent)) {
    case LIBINPUT_BUTTON_STATE_PRESSED:
        return InputRedirection::PointerButtonPressed;
    case LIBINPUT_BUTTON_STATE_RELEASED:
        return InputRedirection::PointerButtonReleased;
    default:
        Q_UNREACHABLE();
    }
}

QVector<InputRedirection::PointerAxis> PointerEvent::axis() const
{
    QVector<InputRedirection::PointerAxis> a;
    if (libinput_event_pointer_has_axis(m_pointerEvent, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL)) {
        a << InputRedirection::PointerAxisHorizontal;
    }
    if (libinput_event_pointer_has_axis(m_pointerEvent, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL)) {
        a << InputRedirection::PointerAxisVertical;
    }
    return a;
}

qreal PointerEvent::scrollValue(InputRedirection::PointerAxis axis) const
{
    const libinput_pointer_axis a = axis == InputRedirection::PointerAxisHorizontal
        ? LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL
        : LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL;
    return libinput_event_pointer_get_scroll_value(m_pointerEvent, a) * device()->scrollFactor();
}

qint32 PointerEvent::scrollValueV120(InputRedirection::PointerAxis axis) const
{
    const libinput_pointer_axis a = (axis == InputRedirection::PointerAxisHorizontal)
        ? LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL
        : LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL;
    return libinput_event_pointer_get_scroll_value_v120(m_pointerEvent, a) * device()->scrollFactor();
}

TouchEvent::TouchEvent(libinput_event *event, libinput_event_type type)
    : Event(event, type)
    , m_touchEvent(libinput_event_get_touch_event(event))
{
}

TouchEvent::~TouchEvent() = default;

std::chrono::microseconds TouchEvent::time() const
{
    return std::chrono::microseconds(libinput_event_touch_get_time_usec(m_touchEvent));
}

QPointF TouchEvent::absolutePos() const
{
    Q_ASSERT(type() == LIBINPUT_EVENT_TOUCH_DOWN || type() == LIBINPUT_EVENT_TOUCH_MOTION);
    return QPointF(libinput_event_touch_get_x(m_touchEvent),
                   libinput_event_touch_get_y(m_touchEvent));
}

QPointF TouchEvent::absolutePos(const QSize &size) const
{
    Q_ASSERT(type() == LIBINPUT_EVENT_TOUCH_DOWN || type() == LIBINPUT_EVENT_TOUCH_MOTION);
    return QPointF(libinput_event_touch_get_x_transformed(m_touchEvent, size.width()),
                   libinput_event_touch_get_y_transformed(m_touchEvent, size.height()));
}

qint32 TouchEvent::id() const
{
    Q_ASSERT(type() != LIBINPUT_EVENT_TOUCH_FRAME);

    return libinput_event_touch_get_seat_slot(m_touchEvent);
}

GestureEvent::GestureEvent(libinput_event *event, libinput_event_type type)
    : Event(event, type)
    , m_gestureEvent(libinput_event_get_gesture_event(event))
{
}

GestureEvent::~GestureEvent() = default;

std::chrono::microseconds GestureEvent::time() const
{
    return std::chrono::microseconds(libinput_event_gesture_get_time_usec(m_gestureEvent));
}

int GestureEvent::fingerCount() const
{
    return libinput_event_gesture_get_finger_count(m_gestureEvent);
}

QPointF GestureEvent::delta() const
{
    return QPointF(libinput_event_gesture_get_dx(m_gestureEvent),
                   libinput_event_gesture_get_dy(m_gestureEvent));
}

bool GestureEvent::isCancelled() const
{
    return libinput_event_gesture_get_cancelled(m_gestureEvent) != 0;
}

PinchGestureEvent::PinchGestureEvent(libinput_event *event, libinput_event_type type)
    : GestureEvent(event, type)
{
}

PinchGestureEvent::~PinchGestureEvent() = default;

qreal PinchGestureEvent::scale() const
{
    return libinput_event_gesture_get_scale(m_gestureEvent);
}

qreal PinchGestureEvent::angleDelta() const
{
    return libinput_event_gesture_get_angle_delta(m_gestureEvent);
}

SwipeGestureEvent::SwipeGestureEvent(libinput_event *event, libinput_event_type type)
    : GestureEvent(event, type)
{
}

SwipeGestureEvent::~SwipeGestureEvent() = default;

HoldGestureEvent::HoldGestureEvent(libinput_event *event, libinput_event_type type)
    : GestureEvent(event, type)
{
}

HoldGestureEvent::~HoldGestureEvent() = default;

SwitchEvent::SwitchEvent(libinput_event *event, libinput_event_type type)
    : Event(event, type)
    , m_switchEvent(libinput_event_get_switch_event(event))
{
}

SwitchEvent::~SwitchEvent() = default;

SwitchEvent::State SwitchEvent::state() const
{
    switch (libinput_event_switch_get_switch_state(m_switchEvent)) {
    case LIBINPUT_SWITCH_STATE_OFF:
        return State::Off;
    case LIBINPUT_SWITCH_STATE_ON:
        return State::On;
    default:
        Q_UNREACHABLE();
    }
    return State::Off;
}

std::chrono::microseconds SwitchEvent::time() const
{
    return std::chrono::microseconds(libinput_event_switch_get_time_usec(m_switchEvent));
}

TabletToolEvent::TabletToolEvent(libinput_event *event, libinput_event_type type)
    : Event(event, type)
    , m_tabletToolEvent(libinput_event_get_tablet_tool_event(event))
{
}

QPointF TabletToolEvent::transformedPosition(const QSize &size) const
{
    const QRectF outputArea = device()->outputArea();
    return {size.width() * outputArea.x() + libinput_event_tablet_tool_get_x_transformed(m_tabletToolEvent, size.width() * outputArea.width()),
            size.height() * outputArea.y() + libinput_event_tablet_tool_get_y_transformed(m_tabletToolEvent, size.height() * outputArea.height())};
}

TabletToolButtonEvent::TabletToolButtonEvent(libinput_event *event, libinput_event_type type)
    : Event(event, type)
    , m_tabletToolEvent(libinput_event_get_tablet_tool_event(event))
{
}

TabletPadButtonEvent::TabletPadButtonEvent(libinput_event *event, libinput_event_type type)
    : Event(event, type)
    , m_tabletPadEvent(libinput_event_get_tablet_pad_event(event))
{
}

TabletPadStripEvent::TabletPadStripEvent(libinput_event *event, libinput_event_type type)
    : Event(event, type)
    , m_tabletPadEvent(libinput_event_get_tablet_pad_event(event))
{
}

TabletPadRingEvent::TabletPadRingEvent(libinput_event *event, libinput_event_type type)
    : Event(event, type)
    , m_tabletPadEvent(libinput_event_get_tablet_pad_event(event))
{
}
}
}
