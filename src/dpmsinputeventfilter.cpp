/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "dpmsinputeventfilter.h"
#include "core/output.h"
#include "core/outputbackend.h"
#include "input_event.h"
#include "main.h"
#include "utils/keys.h"
#include "wayland_server.h"
#include "workspace.h"

#include <QGuiApplication>
#include <QKeyEvent>

#include <QProximitySensor>

namespace KWin
{

DpmsInputEventFilter::DpmsInputEventFilter()
    : InputEventFilter(InputFilterOrder::Dpms)
{
    KSharedConfig::Ptr kwinSettings = kwinApp()->config();
    m_enableDoubleTap = kwinSettings->group(QStringLiteral("Wayland")).readEntry<bool>("DoubleTapWakeup", true);

    if (m_enableDoubleTap) {
        m_sensor = std::make_unique<QProximitySensor>();
        connect(m_sensor.get(), &QProximitySensor::readingChanged, this, &DpmsInputEventFilter::updateProximitySensor, Qt::UniqueConnection);
        m_sensor->start();
        updateProximitySensor();
    }
}

DpmsInputEventFilter::~DpmsInputEventFilter()
{
    if (m_enableDoubleTap) {
        m_sensor->stop();
        m_proximityClose = false;
    }
}

void DpmsInputEventFilter::updateProximitySensor()
{
    // change proximity value only if there is valid sensor backend is connected
    if (m_sensor->isConnectedToBackend() && !m_sensor->sensorsForType(m_sensor->type()).isEmpty()) {
        m_proximityClose = m_sensor->reading()->close();
    }
}

bool DpmsInputEventFilter::pointerMotion(PointerMotionEvent *event)
{
    if (!event->warp) {
        // The intention is to wake the screen on user interactions
        // warp events aren't user interactions, so ignore them.
        notify();
    }
    return true;
}

bool DpmsInputEventFilter::pointerButton(PointerButtonEvent *event)
{
    notify();
    return true;
}

bool DpmsInputEventFilter::pointerAxis(PointerAxisEvent *event)
{
    notify();
    return true;
}

bool DpmsInputEventFilter::keyboardKey(KeyboardKeyEvent *event)
{
    if (isMediaKey(event->key)) {
        // don't wake up the screens for media or volume keys
        return false;
    }
    if (event->state == KeyboardKeyState::Pressed) {
        notify();
    } else if (event->state == KeyboardKeyState::Released) {
        return false;
    }
    return true;
}

bool DpmsInputEventFilter::touchDown(TouchDownEvent *event)
{
    if (m_enableDoubleTap) {
        if (m_touchPoints.isEmpty()) {
            if (!m_doubleTapTimer.isValid()) {
                // this is the first tap
                m_doubleTapTimer.start();
            } else {
                if (m_doubleTapTimer.elapsed() < qApp->doubleClickInterval()) {
                    m_secondTap = true;
                } else {
                    // took too long. Let's consider it a new click
                    m_doubleTapTimer.restart();
                }
            }
        } else {
            // not a double tap
            m_doubleTapTimer.invalidate();
            m_secondTap = false;
        }
        m_touchPoints << event->id;
    }
    return true;
}

bool DpmsInputEventFilter::touchUp(TouchUpEvent *event)
{
    if (m_enableDoubleTap) {
        m_touchPoints.removeAll(event->id);
        if (m_touchPoints.isEmpty() && m_doubleTapTimer.isValid() && m_secondTap) {
            // if device in pocket, do not wake device up
            if (m_doubleTapTimer.elapsed() < qApp->doubleClickInterval() && !m_proximityClose) {
                notify();
            }
            m_doubleTapTimer.invalidate();
            m_secondTap = false;
        }
    }
    return true;
}

bool DpmsInputEventFilter::touchMotion(TouchMotionEvent *event)
{
    // ignore the event
    return true;
}

bool DpmsInputEventFilter::tabletToolProximityEvent(TabletToolProximityEvent *event)
{
    return true;
}

bool DpmsInputEventFilter::tabletToolAxisEvent(TabletToolAxisEvent *event)
{
    return true;
}

bool DpmsInputEventFilter::tabletToolTipEvent(TabletToolTipEvent *event)
{
    if (event->type == TabletToolTipEvent::Press) {
        // Only wake when the tool is actually pressed down not just hovered over the tablet
        notify();
    }
    return true;
}

bool DpmsInputEventFilter::tabletToolButtonEvent(TabletToolButtonEvent *event)
{
    if (event->pressed) {
        notify();
    }
    return true;
}

bool DpmsInputEventFilter::tabletPadButtonEvent(TabletPadButtonEvent *event)
{
    if (event->pressed) {
        notify();
    }
    return true;
}

bool DpmsInputEventFilter::tabletPadStripEvent(TabletPadStripEvent *event)
{
    notify();
    return true;
}

bool DpmsInputEventFilter::tabletPadRingEvent(TabletPadRingEvent *event)
{
    notify();
    return true;
}

bool DpmsInputEventFilter::tabletPadDialEvent(TabletPadDialEvent *event)
{
    notify();
    return true;
}

void DpmsInputEventFilter::notify()
{
    const QList<Output *> outputs = workspace()->outputs();
    for (Output *output : outputs) {
        output->setDpmsMode(Output::DpmsMode::On);
    }
}

}
#include "moc_dpmsinputeventfilter.cpp"
