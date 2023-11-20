/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "gesturehandler.h"
#include "effect/effecthandler.h"

#include <QAction>

#include <functional>

namespace KWin
{

SwipeGestureHandler::SwipeGestureHandler(QObject *parent)
    : QObject(parent)
{
}

void SwipeGestureHandler::classBegin()
{
}

void SwipeGestureHandler::componentComplete()
{
    m_action = new QAction(this);
    connect(m_action, &QAction::triggered, this, &SwipeGestureHandler::activated);

    std::function<void (EffectsHandler *, SwipeDirection dir, uint fingerCount, QAction *onUp, std::function<void(qreal)> progressCallback)> registrator;
    switch (m_deviceType) {
    case Device::Touchpad:
        registrator = std::mem_fn(&EffectsHandler::registerTouchpadSwipeShortcut);
        break;
    case Device::Touchscreen:
        registrator = std::mem_fn(&EffectsHandler::registerTouchscreenSwipeShortcut);
        break;
    }

    registrator(effects, SwipeDirection(m_direction), m_fingerCount, m_action, [this](qreal progress) {
        setProgress(progress);
    });
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

void PinchGestureHandler::classBegin()
{
}

void PinchGestureHandler::componentComplete()
{
    m_action = new QAction(this);
    connect(m_action, &QAction::triggered, this, &PinchGestureHandler::activated);

    std::function<void (EffectsHandler *, PinchDirection dir, uint fingerCount, QAction *onUp, std::function<void(qreal)> progressCallback)> registrator;
    switch (m_deviceType) {
    case Device::Touchpad:
        registrator = std::mem_fn(&EffectsHandler::registerTouchpadPinchShortcut);
        break;
    }

    registrator(effects, PinchDirection(m_direction), m_fingerCount, m_action, [this](qreal progress) {
        setProgress(progress);
    });
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
