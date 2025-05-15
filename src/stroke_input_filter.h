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

class KConfigGroup;

namespace KWin
{

class KWIN_EXPORT StrokeInputFilter : public QObject, public InputEventFilter
{
    Q_OBJECT

public:
    StrokeInputFilter(const KConfigGroup &config, StrokeGestures *gestures);
    ~StrokeInputFilter() override;

    void reconfigure(const KConfigGroup &config);

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

    /**
     * For touchpads, it's easier to first click the activation button, and draw a stroke shape
     * only after releasing it. If this is set to a value greater than 0, pointer movements
     * will be recognized as a stroke if the stroke is started within this many milliseconds
     * after clicking the activation button.
     */
    std::chrono::milliseconds m_startButtonlessStrokeTimeout = std::chrono::milliseconds(0);

    /**
     * If a buttonless stroke was started, it will take this many milliseconds without further
     * mouse movement in order to end stroke recognition. When this is left unchanged at the
     * default value, the time to end buttonless strokes will be the same as the time to start them.
     */
    std::chrono::milliseconds m_endButtonlessStrokeTimeout = std::chrono::milliseconds(0);

    /**
     * The activation button of a pointer device will initiate stroke recognition.
     * For regular activation, the button needs to be held and moved by the activation distance.
     * For buttonless activation, the button needs to be clicked and moved by the
     * activation distance within a certain timeout.
     *
     * We require activation buttons to be configured per device, because we can only really rely
     * on the RMB to be similar for all devices, whereas extra buttons could be located anywhere.
     */
    QHash<QString, Qt::MouseButton> m_activationButton;

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
