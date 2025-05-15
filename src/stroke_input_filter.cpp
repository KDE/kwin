/*
    SPDX-FileCopyrightText: 2024, 2025 Jakob Petsovits <jpetso@petsovits.com>
    SPDX-FileCopyrightText: 2023 Daniel Kondor <kondor.dani@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later AND MIT
*/

#include "stroke_input_filter.h"

#include "effect/effecthandler.h" // (global) EffectsHandler *effects
#include "input_event.h"
#include "pointer_input.h" // KWin::input()->pointer()
#include "utils/common.h"

#include <chrono>
#include <cmath> // std::hypot

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

namespace KWin
{

StrokeInputFilter::StrokeInputFilter(const KConfigGroup &config, StrokeGestures *gestures)
    : InputEventFilter(InputFilterOrder::Stroke)
    , m_gestures(gestures)
{
    m_buttonlessStrokeTimer.setSingleShot(true);
    QObject::connect(&m_buttonlessStrokeTimer, &QTimer::timeout, this, [this] {
        endStroke(std::chrono::milliseconds(QDateTime::currentMSecsSinceEpoch()));
    });

    reconfigure(config);
}

StrokeInputFilter::~StrokeInputFilter()
{
}

void StrokeInputFilter::reconfigure(const KConfigGroup &config)
{
    m_startButtonlessStrokeTimeout = std::chrono::milliseconds(config.readEntry("StartButtonlessStrokeTimeoutMsec", 0));
    m_endButtonlessStrokeTimeout = std::chrono::milliseconds(config.readEntry("EndButtonlessStrokeTimeoutMsec", 0));
    if (m_endButtonlessStrokeTimeout == 0ms) {
        m_endButtonlessStrokeTimeout = m_startButtonlessStrokeTimeout;
    }

    const KConfigGroup perDeviceConfigs = config.group(u"Device"_s);
    m_activationButton.clear();

    for (const QString &deviceName : perDeviceConfigs.groupList()) {
        const KConfigGroup deviceConfig = perDeviceConfigs.group(deviceName);
        if (deviceConfig.hasKey("ActivationButton")) {
            m_activationButton[deviceName] = static_cast<Qt::MouseButton>(deviceConfig.readEntry("ActivationButton", qToUnderlying(Qt::MouseButton::NoButton)));
        }
    }
}

StrokeGestures *StrokeInputFilter::gestures()
{
    return m_gestures;
}

bool StrokeInputFilter::pointerButton(PointerButtonEvent *event)
{
    switch (event->state) {
    case PointerButtonState::Pressed:
        return pointerButtonPressed(event);
    case PointerButtonState::Released:
        return pointerButtonReleased(event);
    default:
        return false;
    }
}

bool StrokeInputFilter::pointerButtonPressed(PointerButtonEvent *event)
{
    if (!event->device) {
        return false;
    }

    auto grabIt = m_buttonGrabs.find(event->device);
    if (grabIt != m_buttonGrabs.end()) {
        // Once we've started a button grab, pressing another button will cancel a possible stroke
        if (event->nativeButton != grabIt->nativeButton) {
            if (m_activeGrabDevice == event->device) {
                endStroke(event->timestamp);
            } else {
                releaseButtonGrab(event->device, *grabIt);
            }
        }
        // Also pass through an emulated activation button press that never turned "active"
        return false;
    }

    // Multiple input devices can be grabbed/inhibited, but once any of them moves far enough,
    // we enter "active" stroke recognition and discard any other attempts at starting a new one
    if (m_activeGrabDevice != nullptr) {
        return false;
    }

    // Don't grab the mouse button if no gestures are registered to begin with
    if (m_gestures->isEmpty(event->modifiers)) {
        return false;
    }

    // Consider looking for strokes when *only* the activation button is pressed
    Qt::MouseButton activationButton = m_activationButton.value(event->device->name(), Qt::MouseButton::NoButton);
    if (event->button == activationButton && (event->buttons & ~activationButton) == 0) {
        m_buttonGrabs.insert(event->device,
                             ButtonGrab{
                                 .points = {event->position},
                                 .nativeButton = event->nativeButton,
                                 .modifiers = event->modifiers,
                                 .lastTimestamp = event->timestamp,
                             });
        QObject::connect(event->device, &QObject::destroyed, this, &StrokeInputFilter::onInputDeviceDestroyed, Qt::UniqueConnection);
        return true; // inhibit button event
    }

    return false;
}

void StrokeInputFilter::onInputDeviceDestroyed(QObject *device)
{
    if (m_activeGrabDevice == device) {
        m_buttonlessStrokeTimer.stop();
        m_activeGrabDevice = nullptr;

        qCDebug(KWIN_CORE) << "Input device removed, aborting stroke recognition";
        StrokeGestureCancelEvent cancelEvent{.time = std::chrono::milliseconds(QDateTime::currentMSecsSinceEpoch())};
        Q_EMIT effects->strokeGestureCancelled(&cancelEvent);
    }
    m_buttonGrabs.remove(static_cast<InputDevice *>(device));
}

bool StrokeInputFilter::pointerButtonReleased(PointerButtonEvent *event)
{
    auto grabIt = m_buttonGrabs.find(event->device);
    if (grabIt == m_buttonGrabs.end()) {
        return false;
    }

    if (event->nativeButton == grabIt->nativeButton) {
        if (grabIt->releasing) {
            // Emulated button release after ending stroke recognition
            QObject::disconnect(event->device, &QObject::destroyed, this, &StrokeInputFilter::onInputDeviceDestroyed);
            m_buttonGrabs.erase(grabIt);
            return false;
        }
        grabIt->lastTimestamp = event->timestamp;

        if (m_startButtonlessStrokeTimeout > 0ms && m_activeGrabDevice == nullptr) {
            m_buttonlessStrokeTimer.stop();
            m_buttonlessStrokeTimer.start(m_startButtonlessStrokeTimeout);
        } else {
            endStroke(event->timestamp);
        }
        return true; // inhibit button event
    }

    return false;
}

bool StrokeInputFilter::pointerMotion(PointerMotionEvent *event)
{
    auto grabIt = m_buttonGrabs.find(event->device);
    if (grabIt == m_buttonGrabs.end()) {
        return false;
    }
    if (!effects) {
        // No stroke recognition will initiate if we can't emit effects->strokeGestureBegin() et al.
        return false;
    }

    if (m_activeGrabDevice == nullptr) {
        // Start stroke recognition if the mouse moves far enough from the button-pressed starting point.
        const auto delta = event->position - grabIt->points.first();
        const auto deltaDistance = std::hypot(delta.x(), delta.y());

        if (deltaDistance >= m_activationDistance) {
            m_activeGrabDevice = event->device;
            releaseInactiveButtonGrabs();
            beginStroke(grabIt.value(), event);
        }
    } else {
        updateStroke(grabIt.value(), event);
    }

    // Extend the duration of buttonless stroke timeouts if that's what we started with
    if (m_buttonlessStrokeTimer.isActive()) {
        m_buttonlessStrokeTimer.stop();
        m_buttonlessStrokeTimer.start(m_endButtonlessStrokeTimeout);

        // Timestamp for emulating a mouse click that doesn't get to "active" stroke recognition stage
        grabIt->lastTimestamp = event->timestamp;
    }

    return false;
}

namespace
{
qreal distance(const QPointF &vec)
{
    return std::hypot(vec.x(), vec.y());
}
struct PerpendicularDistanceResult
{
    qreal distanceFromLine;
    bool isLeftOfLine;
};
PerpendicularDistanceResult perpendicularDistanceFromLine(const QPointF &p, const QPointF &lineOrigin, const QPointF &lineVec)
{
    QPointF vecOriginToP = p - lineOrigin;
    double vectorsCrossProduct = lineVec.x() * vecOriginToP.y() - lineVec.y() * vecOriginToP.x();
    double paralellogramArea = abs(vectorsCrossProduct);
    return PerpendicularDistanceResult{
        .distanceFromLine = paralellogramArea / distance(lineVec),
        .isLeftOfLine = vectorsCrossProduct > 0,
    };
}
}

void StrokeInputFilter::beginStroke(ButtonGrab &grab, PointerMotionEvent *event)
{
    qCDebug(KWIN_CORE) << "Starting stroke recognition -" << m_activeGrabDevice;
    StrokeGestureBeginEvent beginEvent{
        .modifiers = grab.modifiers,
        .origin = grab.points.first(),
        .latestPos = grab.points.last(),
        .time = event->timestamp,
    };
    Q_EMIT effects->strokeGestureBegin(&beginEvent);

    grab.points.resize(2);
    grab.points[1] = event->position;
    m_simplificationOriginPoint = grab.points[0];
    m_simplificationToleranceVector = grab.points[1] - grab.points[0];
    m_simplificationTurningPointCandidate = TurningPointCandidate{
        .point = grab.points[1],
        .score = 0.0,
        .isLeftOfRay = std::nullopt,
    };
}

void StrokeInputFilter::updateStroke(ButtonGrab &grab, PointerMotionEvent *event)
{
    // All of this stroke tracking and matching business happens on KWin's main thread.
    // If we collect all the stroke points and match them without simplifying the line first,
    // we'll block when matching any non-trivial stroke.
    //
    // This function uses a modified Opheim line simplification algorithm to simplify the line that
    // gets displayed and captured. The modification allow continuous simplification, so we can
    // split up the work evenly across movement event don't have to do it all at once at the end.
    // This also gives the real-time visualization a cleaner look.
    //
    // Algorithm overview - a trailing asterisk (*) marks steps not in the original algorithm:
    // * Initialize with:
    //     * initial origin := points[0]
    //     * initial tolerance ray vector: from origin to first point >= m_activationDistance
    //     * turning point candidate: first point >= m_activationDistance from origin
    // * For each new point:
    //     * Cast a tolerance ray originating from origin in vector direction, width: 2 * m_simplificationPerpendicularDistanceTolerance
    //     * Don't record any points within tolerance ray or radial distance from origin point.
    //     * In addition to origin and tolerance ray vector, always retain two points (*):
    //         1. Current pointer position (line end)
    //         2. Turning point candidate (possibly on opposite side of the tolerance ray)
    //     * Set new origin and tolerance ray if either:
    //         * the current pointer position exits tolerance ray, or
    //         * the current pointer position backtracks a certain distance from the turning point candidate (*).
    //
    // The turning point candidate should provide a good origin point for the next ray cast,
    // and also attempt to capture the furthest-most point before the stroke changes direction.
    // If the current pointer position moves past the turning point candidate on the same side
    // of the tolerance ray, it will become the new turning point candidate. We try to avoid
    // replacing the candidate with just a lateral move to the other side of the tolerance ray,
    // so the final stroke will preserve its sharp angles as good as possible.

    const auto currentDeltaFromOrigin = event->position - m_simplificationOriginPoint;
    const auto currentDistanceFromOrigin = distance(currentDeltaFromOrigin);

    if (currentDistanceFromOrigin > m_simplificationRadialDistanceTolerance) {
        if (m_simplificationToleranceVector == QPointF()) {
            // First new turning point candidate in this line segment. By definition, it lies
            // within the tolerance ray and is not considered a segment-starting turn.
            // This candidate will be subsequently updated before getting added to the stroke,
            // the last one in a segment will become an  actual stroke point.
            m_simplificationToleranceVector = currentDeltaFromOrigin;
            m_simplificationTurningPointCandidate = TurningPointCandidate{
                .point = event->position,
                .score = currentDistanceFromOrigin,
                .isLeftOfRay = std::nullopt,
            };
        } else {
            const PerpendicularDistanceResult currentPointPerpendicular = perpendicularDistanceFromLine(event->position, m_simplificationOriginPoint, m_simplificationToleranceVector);

            // Start a new line segment if the current pointer position makes a large enough turn.
            bool startingNewSegment = false;

            // Check if current point is outside of tolerance ray.
            if (currentPointPerpendicular.distanceFromLine > m_simplificationPerpendicularDistanceTolerance) {
                startingNewSegment = true;
            }
            // Check if it's been backtracking compared to the turning point candidate.
            else if (currentDistanceFromOrigin < distance(m_simplificationTurningPointCandidate.point - m_simplificationOriginPoint) - m_simplificationRadialDistanceTolerance) {
                startingNewSegment = true;
            }

            if (startingNewSegment) {
                // First, draw a line from previous origin to the (now locked in) turning point.
                m_simplificationOriginPoint = m_simplificationTurningPointCandidate.point;
                grab.points.last() = m_simplificationTurningPointCandidate.point;

                // Then continue the line to current pointer position as end of the new line segment.
                grab.points.push_back(event->position);

                const StrokeGestureUpdateEvent updateEvent{
                    .segmentOrigin = m_simplificationOriginPoint,
                    .latestPos = grab.points.last(),
                    .startingNewSegment = true,
                    .time = event->timestamp,
                };
                Q_EMIT effects->strokeGestureUpdate(&updateEvent);

                // Re-initialize tolerance ray only after exceeding m_simplificationRadialDistanceTolerance.
                m_simplificationToleranceVector = QPointF();
                m_simplificationTurningPointCandidate = TurningPointCandidate{};
                return;
            }

            // Current position is still inside tolerance ray, not backtracking. Perhaps advancing
            // along the ray, or just moving more or less laterally. Not starting a new segment.
            // Check if we want the current pointer to be our new turning point candidate.
            //
            // A simple distance-based score means we generally keep moving the turning point
            // candidate further away from the current origin. Avoid replacements from moving just
            // laterally between both sides of the tolerance ray, by applying a little extra bias.
            const auto perpendicularDistanceBias =
                currentPointPerpendicular.isLeftOfLine == m_simplificationTurningPointCandidate.isLeftOfRay
                ? currentPointPerpendicular.distanceFromLine
                : 0.0;
            const qreal currentScore = currentDistanceFromOrigin + perpendicularDistanceBias;

            if (currentScore > m_simplificationTurningPointCandidate.score) {
                m_simplificationTurningPointCandidate = {
                    .point = event->position,
                    .score = currentScore,
                    .isLeftOfRay = currentPointPerpendicular.isLeftOfLine,
                };
            }
        }
    }

    // Update end point of the current line segment.
    grab.points.last() = event->position;
    const StrokeGestureUpdateEvent updateEvent{
        .segmentOrigin = m_simplificationOriginPoint,
        .latestPos = grab.points.last(),
        .startingNewSegment = false,
        .time = event->timestamp,
    };
    Q_EMIT effects->strokeGestureUpdate(&updateEvent);
}

void StrokeInputFilter::endStroke(std::chrono::microseconds time)
{
    m_buttonlessStrokeTimer.stop();
    releaseInactiveButtonGrabs();

    if (m_activeGrabDevice == nullptr) {
        return;
    }
    const ButtonGrab &activeButtonGrab = m_buttonGrabs[m_activeGrabDevice];
    qCDebug(KWIN_CORE) << "Ending stroke recognition -" << m_activeGrabDevice << "-" << activeButtonGrab.points.size() << "points";

    // Find the best stroke match among candidates
    double bestScore = 0.0;
    StrokeGesture *bestGesture = m_gestures->bestMatch(activeButtonGrab.modifiers, activeButtonGrab.points, bestScore);

    auto device = m_activeGrabDevice;

    // Drop any remaining data we had about the gesture
    QObject::disconnect(m_activeGrabDevice, &QObject::destroyed, this, &StrokeInputFilter::onInputDeviceDestroyed);
    m_buttonGrabs.remove(m_activeGrabDevice);
    m_activeGrabDevice = nullptr;

    if (bestGesture != nullptr) {
        qCDebug(KWIN_CORE) << "Matched stroke gesture" << bestGesture << "score:" << bestScore;
        StrokeGestureEndEvent endEvent{.triggeredAction = bestGesture->actionInfo(), .time = time};
        Q_EMIT bestGesture->triggered();
        Q_EMIT effects->strokeGestureEnd(&endEvent);
    } else {
        qCDebug(KWIN_CORE) << "No stroke matches";
        StrokeGestureCancelEvent cancelEvent{.time = time};
        Q_EMIT effects->strokeGestureCancelled(&cancelEvent);
    }
}

void StrokeInputFilter::releaseInactiveButtonGrabs()
{
    for (auto grabIt = m_buttonGrabs.begin(); grabIt != m_buttonGrabs.end(); ++grabIt) {
        if (grabIt.key() != m_activeGrabDevice) {
            releaseButtonGrab(grabIt.key(), *grabIt);
        }
    }
}

void StrokeInputFilter::releaseButtonGrab(InputDevice *device, ButtonGrab &grab)
{
    grab.releasing = true;

    QTimer::singleShot(0, this, [this, dev = QPointer(device), button = grab.nativeButton, time = grab.lastTimestamp] {
        if (!dev) {
            return;
        }
        KWin::input()->pointer()->processButton(button, PointerButtonState::Pressed, time, dev);
        KWin::input()->pointer()->processButton(button, PointerButtonState::Released, time, dev);

        // Keep m_buttonGrabs[device] for a little longer to avoid infinite press/release loops.
        // We'll erase it in pointerReleasedEvent().
    });
}

} // namespace KWin

#include "moc_stroke_input_filter.cpp"
