/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "input.h"

#include <libinput.h>

namespace KWin
{
namespace LibInput
{

class Device;

class Event
{
public:
    virtual ~Event();

    libinput_event_type type() const;
    Device *device() const;
    libinput_device *nativeDevice() const;

    operator libinput_event *()
    {
        return m_event;
    }
    operator libinput_event *() const
    {
        return m_event;
    }

    static std::unique_ptr<Event> create(libinput_event *event);

protected:
    Event(libinput_event *event, libinput_event_type type);

private:
    libinput_event *m_event;
    libinput_event_type m_type;
    mutable Device *m_device;
};

class KeyEvent : public Event
{
public:
    KeyEvent(libinput_event *event);
    ~KeyEvent() override;

    uint32_t key() const;
    InputRedirection::KeyboardKeyState state() const;
    std::chrono::microseconds time() const;

    operator libinput_event_keyboard *()
    {
        return m_keyboardEvent;
    }
    operator libinput_event_keyboard *() const
    {
        return m_keyboardEvent;
    }

private:
    libinput_event_keyboard *m_keyboardEvent;
};

class PointerEvent : public Event
{
public:
    PointerEvent(libinput_event *event, libinput_event_type type);
    ~PointerEvent() override;

    QPointF absolutePos() const;
    QPointF absolutePos(const QSize &size) const;
    QPointF delta() const;
    QPointF deltaUnaccelerated() const;
    uint32_t button() const;
    InputRedirection::PointerButtonState buttonState() const;
    std::chrono::microseconds time() const;
    QVector<InputRedirection::PointerAxis> axis() const;
    qreal scrollValue(InputRedirection::PointerAxis a) const;
    qint32 scrollValueV120(InputRedirection::PointerAxis axis) const;

    operator libinput_event_pointer *()
    {
        return m_pointerEvent;
    }
    operator libinput_event_pointer *() const
    {
        return m_pointerEvent;
    }

private:
    libinput_event_pointer *m_pointerEvent;
};

class TouchEvent : public Event
{
public:
    TouchEvent(libinput_event *event, libinput_event_type type);
    ~TouchEvent() override;

    std::chrono::microseconds time() const;
    QPointF absolutePos() const;
    QPointF absolutePos(const QSize &size) const;
    qint32 id() const;

    operator libinput_event_touch *()
    {
        return m_touchEvent;
    }
    operator libinput_event_touch *() const
    {
        return m_touchEvent;
    }

private:
    libinput_event_touch *m_touchEvent;
};

class GestureEvent : public Event
{
public:
    ~GestureEvent() override;

    std::chrono::microseconds time() const;
    int fingerCount() const;

    QPointF delta() const;

    bool isCancelled() const;

    operator libinput_event_gesture *()
    {
        return m_gestureEvent;
    }
    operator libinput_event_gesture *() const
    {
        return m_gestureEvent;
    }

protected:
    GestureEvent(libinput_event *event, libinput_event_type type);
    libinput_event_gesture *m_gestureEvent;
};

class PinchGestureEvent : public GestureEvent
{
public:
    PinchGestureEvent(libinput_event *event, libinput_event_type type);
    ~PinchGestureEvent() override;

    qreal scale() const;
    qreal angleDelta() const;
};

class SwipeGestureEvent : public GestureEvent
{
public:
    SwipeGestureEvent(libinput_event *event, libinput_event_type type);
    ~SwipeGestureEvent() override;
};

class HoldGestureEvent : public GestureEvent
{
public:
    HoldGestureEvent(libinput_event *event, libinput_event_type type);
    ~HoldGestureEvent() override;
};

class SwitchEvent : public Event
{
public:
    SwitchEvent(libinput_event *event, libinput_event_type type);
    ~SwitchEvent() override;

    enum class State {
        Off,
        On
    };
    State state() const;

    std::chrono::microseconds time() const;

private:
    libinput_event_switch *m_switchEvent;
};

class TabletToolEvent : public Event
{
public:
    TabletToolEvent(libinput_event *event, libinput_event_type type);

    std::chrono::microseconds time() const
    {
        return std::chrono::microseconds(libinput_event_tablet_tool_get_time_usec(m_tabletToolEvent));
    }
    bool xHasChanged() const
    {
        return libinput_event_tablet_tool_x_has_changed(m_tabletToolEvent);
    }
    bool yHasChanged() const
    {
        return libinput_event_tablet_tool_y_has_changed(m_tabletToolEvent);
    }
    bool pressureHasChanged() const
    {
        return libinput_event_tablet_tool_pressure_has_changed(m_tabletToolEvent);
    }
    bool distanceHasChanged() const
    {
        return libinput_event_tablet_tool_distance_has_changed(m_tabletToolEvent);
    }
    bool tiltXHasChanged() const
    {
        return libinput_event_tablet_tool_tilt_x_has_changed(m_tabletToolEvent);
    }
    bool tiltYHasChanged() const
    {
        return libinput_event_tablet_tool_tilt_y_has_changed(m_tabletToolEvent);
    }
    bool rotationHasChanged() const
    {
        return libinput_event_tablet_tool_rotation_has_changed(m_tabletToolEvent);
    }
    bool sliderHasChanged() const
    {
        return libinput_event_tablet_tool_slider_has_changed(m_tabletToolEvent);
    }

    // uncomment when depending on libinput 1.14 or when implementing totems
    //     bool sizeMajorHasChanged() const { return
    //     libinput_event_tablet_tool_size_major_has_changed(m_tabletToolEvent); } bool
    //     sizeMinorHasChanged() const { return
    //     libinput_event_tablet_tool_size_minor_has_changed(m_tabletToolEvent); }
    bool wheelHasChanged() const
    {
        return libinput_event_tablet_tool_wheel_has_changed(m_tabletToolEvent);
    }
    QPointF position() const
    {
        return {libinput_event_tablet_tool_get_x(m_tabletToolEvent),
                libinput_event_tablet_tool_get_y(m_tabletToolEvent)};
    }
    QPointF delta() const
    {
        return {libinput_event_tablet_tool_get_dx(m_tabletToolEvent),
                libinput_event_tablet_tool_get_dy(m_tabletToolEvent)};
    }
    qreal pressure() const
    {
        return libinput_event_tablet_tool_get_pressure(m_tabletToolEvent);
    }
    qreal distance() const
    {
        return libinput_event_tablet_tool_get_distance(m_tabletToolEvent);
    }
    int xTilt() const
    {
        return libinput_event_tablet_tool_get_tilt_x(m_tabletToolEvent);
    }
    int yTilt() const
    {
        return libinput_event_tablet_tool_get_tilt_y(m_tabletToolEvent);
    }
    qreal rotation() const
    {
        return libinput_event_tablet_tool_get_rotation(m_tabletToolEvent);
    }
    qreal sliderPosition() const
    {
        return libinput_event_tablet_tool_get_slider_position(m_tabletToolEvent);
    }
    // Uncomment when depending on libinput 1.14 or when implementing totems
    // qreal sizeMajor() const { return
    //     libinput_event_tablet_tool_get_size_major(m_tabletToolEvent); }
    // qreal sizeMinor() const {
    //     return libinput_event_tablet_tool_get_size_minor(m_tabletToolEvent); }
    qreal wheelDelta() const
    {
        return libinput_event_tablet_tool_get_wheel_delta(m_tabletToolEvent);
    }
    int wheelDeltaDiscrete() const
    {
        return libinput_event_tablet_tool_get_wheel_delta_discrete(m_tabletToolEvent);
    }

    bool isTipDown() const
    {
        const auto state = libinput_event_tablet_tool_get_tip_state(m_tabletToolEvent);
        return state == LIBINPUT_TABLET_TOOL_TIP_DOWN;
    }
    bool isNearby() const
    {
        const auto state = libinput_event_tablet_tool_get_proximity_state(m_tabletToolEvent);
        return state == LIBINPUT_TABLET_TOOL_PROXIMITY_STATE_IN;
    }

    QPointF transformedPosition(const QSize &size) const;

    struct libinput_tablet_tool *tool()
    {
        return libinput_event_tablet_tool_get_tool(m_tabletToolEvent);
    }

private:
    libinput_event_tablet_tool *m_tabletToolEvent;
};

class TabletToolButtonEvent : public Event
{
public:
    TabletToolButtonEvent(libinput_event *event, libinput_event_type type);

    uint buttonId() const
    {
        return libinput_event_tablet_tool_get_button(m_tabletToolEvent);
    }

    bool isButtonPressed() const
    {
        const auto state = libinput_event_tablet_tool_get_button_state(m_tabletToolEvent);
        return state == LIBINPUT_BUTTON_STATE_PRESSED;
    }

    struct libinput_tablet_tool *tool()
    {
        return libinput_event_tablet_tool_get_tool(m_tabletToolEvent);
    }

    std::chrono::microseconds time() const
    {
        return std::chrono::microseconds(libinput_event_tablet_tool_get_time_usec(m_tabletToolEvent));
    }

private:
    libinput_event_tablet_tool *m_tabletToolEvent;
};

class TabletPadRingEvent : public Event
{
public:
    TabletPadRingEvent(libinput_event *event, libinput_event_type type);

    int position() const
    {
        return libinput_event_tablet_pad_get_ring_position(m_tabletPadEvent);
    }
    int number() const
    {
        return libinput_event_tablet_pad_get_ring_number(m_tabletPadEvent);
    }
    libinput_tablet_pad_ring_axis_source source() const
    {
        return libinput_event_tablet_pad_get_ring_source(m_tabletPadEvent);
    }
    std::chrono::microseconds time() const
    {
        return std::chrono::microseconds(libinput_event_tablet_pad_get_time_usec(m_tabletPadEvent));
    }

private:
    libinput_event_tablet_pad *m_tabletPadEvent;
};

class TabletPadStripEvent : public Event
{
public:
    TabletPadStripEvent(libinput_event *event, libinput_event_type type);

    int position() const
    {
        return libinput_event_tablet_pad_get_strip_position(m_tabletPadEvent);
    }
    int number() const
    {
        return libinput_event_tablet_pad_get_strip_number(m_tabletPadEvent);
    }
    libinput_tablet_pad_strip_axis_source source() const
    {
        return libinput_event_tablet_pad_get_strip_source(m_tabletPadEvent);
    }
    std::chrono::microseconds time() const
    {
        return std::chrono::microseconds(libinput_event_tablet_pad_get_time_usec(m_tabletPadEvent));
    }

private:
    libinput_event_tablet_pad *m_tabletPadEvent;
};

class TabletPadButtonEvent : public Event
{
public:
    TabletPadButtonEvent(libinput_event *event, libinput_event_type type);

    uint buttonId() const
    {
        return libinput_event_tablet_pad_get_button_number(m_tabletPadEvent);
    }
    bool isButtonPressed() const
    {
        const auto state = libinput_event_tablet_pad_get_button_state(m_tabletPadEvent);
        return state == LIBINPUT_BUTTON_STATE_PRESSED;
    }
    std::chrono::microseconds time() const
    {
        return std::chrono::microseconds(libinput_event_tablet_pad_get_time_usec(m_tabletPadEvent));
    }

private:
    libinput_event_tablet_pad *m_tabletPadEvent;
};

inline libinput_event_type Event::type() const
{
    return m_type;
}

}
}
