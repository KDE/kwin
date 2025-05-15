/*
    SPDX-FileCopyrightText: 2024, 2025 Jakob Petsovits <jpetso@petsovits.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "input.h"
#include "stroke.h"

#include <QMap>
#include <QTimer>

#include <chrono>
#include <vector>

namespace KWin
{

// not derived from Gesture because we can't offer started() & cancelled() on a per-stroke basis
class StrokeGesture : public QObject
{
    Q_OBJECT

public:
    explicit StrokeGesture(QObject *parent = nullptr);
    ~StrokeGesture() override;

    const Stroke &stroke() const;
    void setStroke(Stroke &&stroke);

Q_SIGNALS:
    /**
     * Gesture matching ended and this Gesture matched.
     */
    void triggered();

private:
    Stroke m_stroke;
};

class StrokeGestures : public QObject
{
    Q_OBJECT

public:
    bool isEmpty() const;
    const std::vector<StrokeGesture *> &list() const;

    void add(StrokeGesture *gesture);
    void remove(StrokeGesture *gesture);

    StrokeGesture *bestMatch(const Stroke &stroke, double &bestScore) const;

private:
    std::vector<StrokeGesture *> m_gestures;
};

class KWIN_EXPORT StrokeInputFilter : public QObject, public InputEventFilter
{
    Q_OBJECT

public:
    StrokeInputFilter(StrokeGestures *gestures);
    ~StrokeInputFilter() override;

    StrokeGestures *gestures();

    bool pointerButton(PointerButtonEvent *event) override;
    bool pointerMotion(PointerMotionEvent *event) override;

private:
    bool pointerButtonPressed(PointerButtonEvent *event);
    bool pointerButtonReleased(PointerButtonEvent *event);

    struct ButtonGrab;
    void releaseInactiveButtonGrabs();
    void releaseButtonGrab(InputDevice *device, ButtonGrab &grab);

private Q_SLOTS:
    void endStroke(std::chrono::microseconds time);
    void onInputDeviceDestroyed(QObject *device);

private:
    // configuration
    // TODO: maybe all three of these should be per-device
    std::chrono::milliseconds m_startButtonlessStrokeTimeout = std::chrono::milliseconds(0);
    std::chrono::milliseconds m_endButtonlessStrokeTimeout = std::chrono::milliseconds(0);

    Qt::MouseButton m_activationButton = Qt::RightButton;

    /**
     * Minimum distance for pointer movement (in KWin::MouseEvent::position() units) at which
     * strokes start getting recognized.
     */
    qreal m_activationDistance = 16.0;

    StrokeGestures *m_gestures;

    // runtime stroke tracking
    struct ButtonGrab
    {
        std::vector<QPointF> points;
        quint32 nativeButton = 0;
        std::chrono::microseconds lastTimestamp;
        bool releasing = false;
    };
    QMap<InputDevice *, ButtonGrab> m_buttonGrabs;
    InputDevice *m_activeGrabDevice = nullptr;
    QTimer m_buttonlessStrokeTimer;
};

} // namespace KWin
