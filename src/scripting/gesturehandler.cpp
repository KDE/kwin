/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "gesturehandler.h"
#include "gestures.h"
#include "globalshortcuts.h"
#include "input.h"

namespace KWin
{

SwipeGestureHandler::SwipeGestureHandler(QObject *parent)
    : QObject(parent)
{
}

SwipeGestureHandler::~SwipeGestureHandler()
{
}

void SwipeGestureHandler::classBegin()
{
}

void SwipeGestureHandler::componentComplete()
{
    m_gesture = std::make_unique<SwipeGesture>();
    m_gesture->setDirection(SwipeDirection(m_direction));
    m_gesture->setMinimumDelta(QPointF(200, 200));
    m_gesture->setMaximumFingerCount(m_fingerCount);
    m_gesture->setMinimumFingerCount(m_fingerCount);

    connect(m_gesture.get(), &SwipeGesture::triggered, this, &SwipeGestureHandler::activated);
    connect(m_gesture.get(), &SwipeGesture::cancelled, this, &SwipeGestureHandler::cancelled);
    connect(m_gesture.get(), &SwipeGesture::progress, this, &SwipeGestureHandler::setProgress);

    switch (m_deviceType) {
    case Device::Touchpad:
        input()->shortcuts()->registerTouchpadSwipe(m_gesture.get());
        break;
    case Device::Touchscreen:
        input()->shortcuts()->registerTouchscreenSwipe(m_gesture.get());
        break;
    }
}

SwipeGestureHandler::Direction SwipeGestureHandler::direction() const
{
    return m_direction;
}

void SwipeGestureHandler::setDirection(Direction direction)
{
    if (m_direction != direction) {
        m_direction = direction;
        Q_EMIT directionChanged();
    }
}

int SwipeGestureHandler::fingerCount() const
{
    return m_fingerCount;
}

void SwipeGestureHandler::setFingerCount(int fingerCount)
{
    if (m_fingerCount != fingerCount) {
        m_fingerCount = fingerCount;
        Q_EMIT fingerCountChanged();
    }
}

qreal SwipeGestureHandler::progress() const
{
    return m_progress;
}

void SwipeGestureHandler::setProgress(qreal progress)
{
    if (m_progress != progress) {
        m_progress = progress;
        Q_EMIT progressChanged();
    }
}

SwipeGestureHandler::Device SwipeGestureHandler::deviceType() const
{
    return m_deviceType;
}

void SwipeGestureHandler::setDeviceType(Device device)
{
    if (m_deviceType != device) {
        m_deviceType = device;
        Q_EMIT deviceTypeChanged();
    }
}

PinchGestureHandler::PinchGestureHandler(QObject *parent)
    : QObject(parent)
{
}

PinchGestureHandler::~PinchGestureHandler()
{
}

void PinchGestureHandler::classBegin()
{
}

void PinchGestureHandler::componentComplete()
{
    m_gesture = std::make_unique<PinchGesture>();
    m_gesture->setDirection(PinchDirection(m_direction));
    m_gesture->setMaximumFingerCount(m_fingerCount);
    m_gesture->setMinimumFingerCount(m_fingerCount);

    connect(m_gesture.get(), &PinchGesture::triggered, this, &PinchGestureHandler::activated);
    connect(m_gesture.get(), &PinchGesture::cancelled, this, &PinchGestureHandler::cancelled);
    connect(m_gesture.get(), &PinchGesture::progress, this, &PinchGestureHandler::setProgress);

    switch (m_deviceType) {
    case Device::Touchpad:
        input()->shortcuts()->registerTouchpadPinch(m_gesture.get());
        break;
    }
}

PinchGestureHandler::Direction PinchGestureHandler::direction() const
{
    return m_direction;
}

void PinchGestureHandler::setDirection(Direction direction)
{
    if (m_direction != direction) {
        m_direction = direction;
        Q_EMIT directionChanged();
    }
}

int PinchGestureHandler::fingerCount() const
{
    return m_fingerCount;
}

void PinchGestureHandler::setFingerCount(int fingerCount)
{
    if (m_fingerCount != fingerCount) {
        m_fingerCount = fingerCount;
        Q_EMIT fingerCountChanged();
    }
}

qreal PinchGestureHandler::progress() const
{
    return m_progress;
}

void PinchGestureHandler::setProgress(qreal progress)
{
    if (m_progress != progress) {
        m_progress = progress;
        Q_EMIT progressChanged();
    }
}

PinchGestureHandler::Device PinchGestureHandler::deviceType() const
{
    return m_deviceType;
}

void PinchGestureHandler::setDeviceType(Device device)
{
    if (m_deviceType != device) {
        m_deviceType = device;
        Q_EMIT deviceTypeChanged();
    }
}

} // namespace KWin

#include "moc_gesturehandler.cpp"
