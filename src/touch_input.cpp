/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "touch_input.h"

#include "config-kwin.h"

#include "decorations/decoratedwindow.h"
#include "input_event.h"
#include "input_event_spy.h"
#include "pointer_input.h"
#include "wayland/display.h"
#include "wayland/seat.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
// KDecoration
#include <KDecoration3/Decoration>
// screenlocker
#if KWIN_BUILD_SCREENLOCKER
#include <KScreenLocker/KsldApp>
#endif
// Qt
#include <QHoverEvent>
#include <QWindow>

namespace KWin
{

TouchInputRedirection::TouchInputRedirection(InputRedirection *parent)
    : InputDeviceHandler(parent)
{
}

TouchInputRedirection::~TouchInputRedirection() = default;

void TouchInputRedirection::init()
{
    Q_ASSERT(!inited());
    waylandServer()->seat()->setHasTouch(input()->hasTouch());
    connect(input(), &InputRedirection::hasTouchChanged,
            waylandServer()->seat(), &SeatInterface::setHasTouch);

    setInited(true);
    InputDeviceHandler::init();

#if KWIN_BUILD_SCREENLOCKER
    if (kwinApp()->supportsLockScreen()) {
        connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged, this, [this]() {
            cancel();
            // position doesn't matter
            update();
        });
    }
#endif
    connect(workspace(), &QObject::destroyed, this, [this] {
        setInited(false);
    });
    connect(waylandServer(), &QObject::destroyed, this, [this] {
        setInited(false);
    });
}

bool TouchInputRedirection::focusUpdatesBlocked()
{
    if (m_windowUpdatedInCycle) {
        return true;
    }
    m_windowUpdatedInCycle = true;
    if (waylandServer()->seat()->isDragTouch()) {
        return true;
    }
    if (m_activeTouchPoints.count() > 1) {
        // first touch defines focus
        return true;
    }
    return false;
}

bool TouchInputRedirection::positionValid() const
{
    // we can only determine a position with at least one touch point
    return !m_activeTouchPoints.isEmpty();
}

void TouchInputRedirection::focusUpdate(Window *focusOld, Window *focusNow)
{
    // TODO: handle pointer grab aka popups

    if (focusOld && focusOld->isClient()) {
        focusOld->pointerLeaveEvent();
    }
    if (focusNow && focusNow->isClient()) {
        focusNow->pointerEnterEvent(m_lastPosition);
    }
}

void TouchInputRedirection::cleanupDecoration(Decoration::DecoratedWindowImpl *old, Decoration::DecoratedWindowImpl *now)
{
    // nothing to do
}

void TouchInputRedirection::processDown(qint32 id, const QPointF &pos, std::chrono::microseconds time, InputDevice *device)
{
    if (!inited()) {
        return;
    }
    m_lastPosition = pos;
    m_windowUpdatedInCycle = false;
    m_activeTouchPoints.insert(id);
    if (m_activeTouchPoints.count() == 1) {
        update();
        workspace()->setActiveOutput(pos);
    }
    input()->setLastInputHandler(this);

    TouchDownEvent event{
        .id = id,
        .pos = pos,
        .time = time,
    };

    input()->processSpies(&InputEventSpy::touchDown, &event);
    input()->processFilters(&InputEventFilter::touchDown, &event);
    m_windowUpdatedInCycle = false;
    input()->setLastInteractionSerial(waylandServer()->seat()->display()->serial());
    if (auto f = focus()) {
        f->setLastUsageSerial(waylandServer()->seat()->display()->serial());
    }
}

void TouchInputRedirection::processUp(qint32 id, std::chrono::microseconds time, InputDevice *device)
{
    if (!inited()) {
        return;
    }
    if (!m_activeTouchPoints.remove(id)) {
        return;
    }
    input()->setLastInputHandler(this);

    TouchUpEvent event{
        .id = id,
        .time = time,
    };

    m_windowUpdatedInCycle = false;
    input()->processSpies(&InputEventSpy::touchUp, &event);
    input()->processFilters(&InputEventFilter::touchUp, &event);
    m_windowUpdatedInCycle = false;
    if (m_activeTouchPoints.count() == 0) {
        update();
    }
}

void TouchInputRedirection::processMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time, InputDevice *device)
{
    if (!inited()) {
        return;
    }
    if (!m_activeTouchPoints.contains(id)) {
        return;
    }
    input()->setLastInputHandler(this);
    m_lastPosition = pos;

    TouchMotionEvent event{
        .id = id,
        .pos = pos,
        .time = time,
    };

    m_windowUpdatedInCycle = false;
    input()->processSpies(&InputEventSpy::touchMotion, &event);
    input()->processFilters(&InputEventFilter::touchMotion, &event);
    m_windowUpdatedInCycle = false;
}

void TouchInputRedirection::cancel()
{
    if (!inited()) {
        return;
    }
    // If the touch sequence is artificially cancelled by the compositor, touch motion and touch
    // up events will be silently ignored and won't be passed down through the event filter chain.
    // If the touch sequence is cancelled because we received a TOUCH_CANCEL event from libinput,
    // the compositor will not receive any TOUCH_MOTION or TOUCH_UP events for that slot.
    if (!m_activeTouchPoints.isEmpty()) {
        m_activeTouchPoints.clear();
        input()->processFilters(&InputEventFilter::touchCancel);
    }
}

void TouchInputRedirection::frame()
{
    if (!inited() || !waylandServer()->seat()->hasTouch()) {
        return;
    }
    input()->processFilters(&InputEventFilter::touchFrame);
}

}

#include "moc_touch_input.cpp"
