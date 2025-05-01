/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QQmlParserStatus>

class QAction;

namespace KWin
{

class PinchGesture;
class SwipeGesture;

/*!
 * \qmltype SwipeGestureHandler
 * \inqmlmodule org.kde.kwin
 *
 * \brief The SwipeGestureHandler type provides a way to handle global swipe gestures.
 *
 * Example usage:
 * \code
 * SwipeGestureHandler {
 *     direction: SwipeGestureHandler.Direction.Up
 *     fingerCount: 3
 *     onActivated: console.log("activated")
 * }
 * \endcode
 */
class SwipeGestureHandler : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    /*!
     * \qmlproperty enumeration SwipeGestureHandler::direction
     * \qmlenumeratorsfrom KWin::SwipeGestureHandler::Direction
     *
     * This property specifies the direction of the swipe gesture. The default value is
     * Direction::Invalid.
     */
    Q_PROPERTY(Direction direction READ direction WRITE setDirection NOTIFY directionChanged)

    /*!
     * \qmlproperty int SwipeGestureHandler::fingerCount
     *
     * This property specifies the required number of fingers for swipe recognition.
     */
    Q_PROPERTY(int fingerCount READ fingerCount WRITE setFingerCount NOTIFY fingerCountChanged)

    /*!
     * \qmlproperty real SwipeGestureHandler::progress
     *
     * This property specifies the progress of the swipe gesture. The progress ranges from
     * 0.0 to 1.0.
     */
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)

    /*!
     * \qmlproperty enumeration SwipeGestureHandler::deviceType
     * \qmlenumeratorsfrom KWin::SwipeGestureHandler::Device
     *
     * This property specifies the input device that can trigger the swipe gesture.
     */
    Q_PROPERTY(Device deviceType READ deviceType WRITE setDeviceType NOTIFY deviceTypeChanged)

public:
    explicit SwipeGestureHandler(QObject *parent = nullptr);
    ~SwipeGestureHandler() override;

    // Matches SwipeDirection.
    /*!
     * The Direction type specifies the direction of the swipe gesture.
     *
     * \value Invalid No direction.
     * \value Down Swipe downward.
     * \value Left Swipe to the left.
     * \value Up Swipe upward.
     * \value Right Swipe to the right.
     */
    enum class Direction {
        Invalid,
        Down,
        Left,
        Up,
        Right,
    };
    Q_ENUM(Direction)

    /*!
     * The Device type specifies the input device that triggers the swipe gesture.
     *
     * \value Touchpad The gesture is triggered by a touchpad input device.
     * \value Touchscreen The gesture is triggered by a touchscreen input device.
     */
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
    /*!
     * This signal is emitted when the swipe gesture is triggered, i.e. the progress() reaches 1.0.
     */
    void activated();

    /*!
     * This signal is emitted when the swipe gesture is cancelled. A swipe gesture can be cancelled
     * if the user lifts their fingers or moves the fingers in a different direction, etc.
     */
    void cancelled();

    void progressChanged();
    void directionChanged();
    void fingerCountChanged();
    void deviceTypeChanged();

private:
    std::unique_ptr<SwipeGesture> m_gesture;
    Direction m_direction = Direction::Invalid;
    Device m_deviceType = Device::Touchpad;
    qreal m_progress = 0;
    int m_fingerCount = 3;
};

/*!
 * \qmltype PinchGestureHandler
 * \inqmlmodule org.kde.kwin
 *
 * \brief The PinchGestureHandler type provides a way to handle global pinch gestures.
 *
 * Example usage:
 * \code
 * PinchGestureHandler {
 *     direction: PinchGestureHandler.Direction.Contracting
 *     fingerCount: 3
 *     onActivated: console.log("activated")
 * }
 * \endcode
 */
class PinchGestureHandler : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    /*!
     * \qmlproperty enumeration PinchGestureHandler::direction
     * \qmlenumeratorsfrom KWin::PinchGestureHandler::Direction
     *
     * This property specifies whether the fingers should contract or expand. The default
     * value is Direction::Contracting.
     */
    Q_PROPERTY(Direction direction READ direction WRITE setDirection NOTIFY directionChanged)

    /*!
     * \qmlproperty int PinchGestureHandler::fingerCount
     *
     * This property specifies the required number of fingers for pinch recognition.
     */
    Q_PROPERTY(int fingerCount READ fingerCount WRITE setFingerCount NOTIFY fingerCountChanged)

    /*!
     * \qmlproperty real PinchGestureHandler::progress
     *
     * This property specifies the progress of the pinch gesture. The progress ranges from
     * 0.0 to 1.0.
     */
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)

public:
    explicit PinchGestureHandler(QObject *parent = nullptr);
    ~PinchGestureHandler() override;

    // Matches PinchDirection.
    /*!
     * The Direction type specifies whether fingers should contract or expand.
     *
     * \value Expanding Fingers should expand for the pinch gesture.
     * \value Contracting Fingers should contract for the pinch gesture.
     */
    enum class Direction {
        Expanding,
        Contracting,
    };
    Q_ENUM(Direction)

    /*!
     * The Device type specifies the input device that triggers the pinch gesture.
     *
     * \value Touchpad The pinch gesture is triggered by a touchpad input device.
     */
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
    /*!
     * This signal is emitted when the pinch gesture is triggered, i.e. the progress() reaches 1.0.
     */
    void activated();

    /*!
     * This signal is emitted when the pinch gesture is cancelled. A pinch gesture can be cancelled
     * if the user lifts their fingers or moves the fingers in a different direction, etc.
     */
    void cancelled();

    void progressChanged();
    void directionChanged();
    void fingerCountChanged();
    void deviceTypeChanged();

private:
    std::unique_ptr<PinchGesture> m_gesture;
    Direction m_direction = Direction::Contracting;
    Device m_deviceType = Device::Touchpad;
    qreal m_progress = 0;
    int m_fingerCount = 3;
};

} // namespace KWin
