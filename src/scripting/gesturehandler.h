/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/globals.h"

#include <QQmlParserStatus>

class QAction;

namespace KWin
{

/**
 * The SwipeGestureHandler type provides a way to handle global swipe gestures.
 *
 * Example usage:
 * @code
 * SwipeGestureHandler {
 *     direction: SwipeGestureHandler.Direction.Up
 *     fingerCount: 3
 *     onActivated: console.log("activated")
 * }
 * @endcode
 */
class SwipeGestureHandler : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    Q_PROPERTY(Direction direction READ direction WRITE setDirection NOTIFY directionChanged)
    Q_PROPERTY(int fingerCount READ fingerCount WRITE setFingerCount NOTIFY fingerCountChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(Device deviceType READ deviceType WRITE setDeviceType NOTIFY deviceTypeChanged)

public:
    explicit SwipeGestureHandler(QObject *parent = nullptr);

    // Matches SwipeDirection.
    enum class Direction {
        Invalid,
        Down,
        Left,
        Up,
        Right,
    };
    Q_ENUM(Direction)

    enum class Device {
        Touchpad,
        Touchscreen,
    };
    Q_ENUM(Device)

    void classBegin() override;
    void componentComplete() override;

    Direction direction() const;
    void setDirection(Direction direction);

    int fingerCount() const;
    void setFingerCount(int fingerCount);

    qreal progress() const;
    void setProgress(qreal progress);

    Device deviceType() const;
    void setDeviceType(Device device);

Q_SIGNALS:
    void activated();
    void progressChanged();
    void directionChanged();
    void fingerCountChanged();
    void deviceTypeChanged();

private:
    QAction *m_action = nullptr;
    Direction m_direction = Direction::Invalid;
    Device m_deviceType = Device::Touchpad;
    qreal m_progress = 0;
    int m_fingerCount = 3;
};

/**
 * The PinchGestureHandler type provides a way to handle global pinch gestures.
 *
 * Example usage:
 * @code
 * PinchGestureHandler {
 *     direction: PinchGestureHandler.Direction.Contracting
 *     fingerCount: 3
 *     onActivated: console.log("activated")
 * }
 * @endcode
 */
class PinchGestureHandler : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    Q_PROPERTY(Direction direction READ direction WRITE setDirection NOTIFY directionChanged)
    Q_PROPERTY(int fingerCount READ fingerCount WRITE setFingerCount NOTIFY fingerCountChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)

public:
    explicit PinchGestureHandler(QObject *parent = nullptr);

    // Matches PinchDirection.
    enum class Direction {
        Expanding,
        Contracting,
    };
    Q_ENUM(Direction)

    enum class Device {
        Touchpad,
    };
    Q_ENUM(Device)

    void classBegin() override;
    void componentComplete() override;

    Direction direction() const;
    void setDirection(Direction direction);

    int fingerCount() const;
    void setFingerCount(int fingerCount);

    qreal progress() const;
    void setProgress(qreal progress);

    Device deviceType() const;
    void setDeviceType(Device device);

Q_SIGNALS:
    void activated();
    void progressChanged();
    void directionChanged();
    void fingerCountChanged();
    void deviceTypeChanged();

private:
    QAction *m_action = nullptr;
    Direction m_direction = Direction::Contracting;
    Device m_deviceType = Device::Touchpad;
    qreal m_progress = 0;
    int m_fingerCount = 3;
};

} // namespace KWin
