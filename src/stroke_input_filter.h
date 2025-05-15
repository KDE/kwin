/*
    SPDX-FileCopyrightText: 2024, 2025 Jakob Petsovits <jpetso@petsovits.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "input.h"
#include "stroke_gestures.h"

#include <QMap>
#include <QTimer>

#include <chrono>
#include <optional>
#include <vector>

namespace KWin
{

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
    void beginStroke(ButtonGrab &grab, PointerMotionEvent *event);
    void updateStroke(ButtonGrab &grab, PointerMotionEvent *event);

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

    struct TurningPointCandidate
    {
        QPointF point = QPointF{};
        qreal score = 0.0;
        std::optional<bool> isLeftOfRay = std::nullopt;
    };
    QPointF m_simplificationOriginPoint;
    QPointF m_simplificationToleranceVector;
    TurningPointCandidate m_simplificationTurningPointCandidate;
    const qreal m_simplificationRadialDistanceTolerance = 16.0;
    const qreal m_simplificationPerpendicularDistanceTolerance = 8.0;

    StrokeGestures *m_gestures;

    // runtime stroke tracking
    struct ButtonGrab
    {
        QList<QPointF> points;
        quint32 nativeButton = 0;
        Qt::KeyboardModifiers modifiers = Qt::NoModifier;
        std::chrono::microseconds lastTimestamp;
        bool releasing = false;
    };
    QMap<InputDevice *, ButtonGrab> m_buttonGrabs;
    InputDevice *m_activeGrabDevice = nullptr;
    QTimer m_buttonlessStrokeTimer;
};

} // namespace KWin
