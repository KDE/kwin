/*
    SPDX-FileCopyrightText: 2024, 2025 Jakob Petsovits <jpetso@petsovits.com>
    SPDX-FileCopyrightText: 2023 Daniel Kondor <kondor.dani@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later AND MIT
*/

#include "strokegestures.h"

#include "input.h"
#include "input_event.h"
#include "pointer_input.h" // KWin::input()->pointer()
#include "utils/common.h"

#include <chrono>
#include <cmath> // std::hypot

using namespace std::chrono_literals;

namespace KWin
{

StrokeGesture::StrokeGesture(QObject *parent)
    : QObject(parent)
{
}

StrokeGesture::~StrokeGesture() = default;

const Stroke &StrokeGesture::stroke() const
{
    return m_stroke;
}

void StrokeGesture::setStroke(Stroke &&stroke)
{
    m_stroke = std::move(stroke);
}

bool StrokeGestures::isEmpty() const
{
    return m_gestures.empty();
}

const std::vector<StrokeGesture *> &StrokeGestures::list() const
{
    return m_gestures;
}

void StrokeGestures::add(StrokeGesture *gesture)
{
    m_gestures.push_back(gesture);
    QObject::connect(gesture, &StrokeGesture::destroyed, this, [this, gesture] {
        remove(gesture);
    });
}

void StrokeGestures::remove(StrokeGesture *gesture)
{
    QObject::disconnect(gesture, &StrokeGesture::destroyed, this, nullptr);
    std::erase(m_gestures, gesture);
}

StrokeGesture *StrokeGestures::bestMatch(const Stroke &stroke, double &bestScore) const
{
    StrokeGesture *bestGesture = nullptr;

    for (StrokeGesture *candidate : m_gestures) {
        double score;
        if (!Stroke::compare(stroke, candidate->stroke(), score) || score < Stroke::min_matching_score()) {
            continue;
        }
        if (score > bestScore) {
            bestScore = score;
            bestGesture = candidate;
        }
    }

    return bestGesture;
}

//
// StrokeInputFilter

StrokeInputFilter::StrokeInputFilter(StrokeGestures *gestures)
    : InputEventFilter(InputFilterOrder::Stroke)
    , m_gestures(gestures)
{
    m_buttonlessStrokeTimer.setSingleShot(true);
    QObject::connect(&m_buttonlessStrokeTimer, &QTimer::timeout, this, [this] {
        endStroke(std::chrono::milliseconds(QDateTime::currentMSecsSinceEpoch()));
    });
}

StrokeInputFilter::~StrokeInputFilter()
{
}

StrokeGestures *StrokeInputFilter::gestures()
{
    return m_gestures;
}

/*
TODO: how to customize? load from config? expose setter for effects to set?
void StrokeInputFilter::reconfigure()
{
    m_startButtonlessStrokeTimeout = std::chrono::milliseconds(StrokeGestureConfig::startButtonlessStrokeTimeoutMsec());
    m_endButtonlessStrokeTimeout = std::chrono::milliseconds(StrokeGestureConfig::endButtonlessStrokeTimeoutMsec());
    if (m_endButtonlessStrokeTimeout == 0ms) {
        m_endButtonlessStrokeTimeout = m_startButtonlessStrokeTimeout;
    }
}
*/

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
    if (m_gestures->isEmpty()) {
        return false;
    }

    // Consider looking for strokes when *only* the activation button is pressed
    if (event->button == m_activationButton && (event->buttons & ~m_activationButton) == 0) {
        m_buttonGrabs.insert(event->device,
                             ButtonGrab{
                                 .points = {event->position},
                                 .nativeButton = event->nativeButton,
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
        Q_EMIT strokeGestureCancelled(std::chrono::milliseconds(QDateTime::currentMSecsSinceEpoch()));
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
    if (m_activeGrabDevice != nullptr && m_activeGrabDevice != event->device) {
        return false;
    }

    auto grabIt = m_buttonGrabs.find(event->device);
    if (grabIt == m_buttonGrabs.end()) {
        return false;
    }

    // Start stroke recognition if the mouse moves far enough from the button-pressed starting point
    const auto delta = grabIt->points[0] - event->position;
    const auto distance = std::hypot(delta.x(), delta.y());

    if (m_activeGrabDevice == nullptr && distance >= m_activationDistance) {
        m_activeGrabDevice = event->device;
        releaseInactiveButtonGrabs();

        qCDebug(KWIN_CORE) << "Starting stroke recognition -" << m_activeGrabDevice;
        KWin::input()->pointer()->processStrokeGestureBegin(grabIt->points, event->timestamp, m_activeGrabDevice);
    }

    // Track stroke points in order to draw and match them later
    grabIt->points.push_back(event->position);

    if (m_activeGrabDevice != nullptr) {
        KWin::input()->pointer()->processStrokeGestureUpdate(grabIt->points, event->timestamp, m_activeGrabDevice);
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

void StrokeInputFilter::endStroke(std::chrono::microseconds time)
{
    m_buttonlessStrokeTimer.stop();
    releaseInactiveButtonGrabs();

    if (m_activeGrabDevice == nullptr) {
        return;
    }
    qCDebug(KWIN_CORE) << "Ending stroke recognition -" << m_activeGrabDevice << "-" << m_buttonGrabs[m_activeGrabDevice].points.size() << "points";

    // Find the best stroke match among candidates
    double bestScore = 0.0;
    StrokeGesture *bestGesture = m_gestures->bestMatch(Stroke{m_buttonGrabs[m_activeGrabDevice].points}, bestScore);

    auto device = m_activeGrabDevice;

    // Drop any remaining data we had about the gesture
    QObject::disconnect(m_activeGrabDevice, &QObject::destroyed, this, &StrokeInputFilter::onInputDeviceDestroyed);
    m_buttonGrabs.remove(m_activeGrabDevice);
    m_activeGrabDevice = nullptr;

    if (bestGesture != nullptr) {
        qCDebug(KWIN_CORE) << "Matched stroke gesture" << bestGesture << "score:" << bestScore;
        bestGesture->triggered();
        KWin::input()->pointer()->processStrokeGestureEnd(time, device);
    } else {
        qCDebug(KWIN_CORE) << "No stroke matches";
        KWin::input()->pointer()->processStrokeGestureCancelled(time, device);
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

#include "moc_strokegestures.cpp"
