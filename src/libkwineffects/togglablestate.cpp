/*
    SPDX-FileCopyrightText: 2023 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "togglablestate.h"
#include "kwineffects.h"

namespace KWin
{

TogglableState::TogglableState(Effect *effect)
    : QObject(effect)
    , m_deactivateAction(std::make_unique<QAction>())
    , m_activateAction(std::make_unique<QAction>())
    , m_toggleAction(std::make_unique<QAction>())
{
    connect(m_activateAction.get(), &QAction::triggered, this, [this]() {
        if (m_status == Status::Activating) {
            if (m_partialActivationFactor > 0.5) {
                activate();
                Q_EMIT activated();
            } else {
                deactivate();
                Q_EMIT deactivated();
            }
        }
    });
    connect(m_deactivateAction.get(), &QAction::triggered, this, [this]() {
        if (m_status == Status::Deactivating) {
            if (m_partialActivationFactor < 0.5) {
                deactivate();
                Q_EMIT deactivated();
            } else {
                activate();
                Q_EMIT activated();
            }
        }
    });
    connect(m_toggleAction.get(), &QAction::triggered, this, &TogglableState::toggle);
}

void TogglableState::activate()
{
    setStatus(Status::Active);
    setInProgress(false);
    setPartialActivationFactor(0.0);
}

void TogglableState::setPartialActivationFactor(qreal factor)
{
    if (m_partialActivationFactor != factor) {
        m_partialActivationFactor = factor;
        Q_EMIT partialActivationFactorChanged();
    }
}

void TogglableState::deactivate()
{
    setInProgress(false);
    setPartialActivationFactor(0.0);
}

bool TogglableState::inProgress() const
{
    return m_inProgress;
}

void TogglableState::setInProgress(bool gesture)
{
    if (m_inProgress != gesture) {
        m_inProgress = gesture;
        Q_EMIT inProgressChanged();
    }
}

void TogglableState::setStatus(Status status)
{
    if (m_status != status) {
        m_status = status;
        Q_EMIT statusChanged(status);
    }
}

void TogglableState::partialActivate(qreal factor)
{
    if (effects->isScreenLocked()) {
        return;
    }

    setStatus(Status::Activating);
    setPartialActivationFactor(factor);
    setInProgress(true);
}

void TogglableState::partialDeactivate(qreal factor)
{
    setStatus(Status::Deactivating);
    setPartialActivationFactor(1.0 - factor);
    setInProgress(true);
}

void TogglableState::toggle()
{
    if (m_status == Status::Inactive || m_partialActivationFactor > 0.5) {
        activate();
        Q_EMIT activated();
    } else {
        deactivate();
        Q_EMIT deactivated();
    }
}

void TogglableState::setProgress(qreal progress)
{
    if (!effects->hasActiveFullScreenEffect() || effects->activeFullScreenEffect() == parent()) {
        switch (m_status) {
        case Status::Inactive:
        case Status::Activating:
            partialActivate(progress);
            break;
        default:
            break;
        }
    }
}

void TogglableState::setRegress(qreal regress)
{
    if (!effects->hasActiveFullScreenEffect() || effects->activeFullScreenEffect() == parent()) {
        switch (m_status) {
        case Status::Active:
        case Status::Deactivating:
            partialDeactivate(regress);
            break;
        default:
            break;
        }
    }
}

TogglableGesture::TogglableGesture(TogglableState *state)
    : QObject(state)
    , m_state(state)
{
}

static PinchDirection opposite(PinchDirection direction)
{
    switch (direction) {
    case PinchDirection::Contracting:
        return PinchDirection::Expanding;
    case PinchDirection::Expanding:
        return PinchDirection::Contracting;
    }
    return PinchDirection::Expanding;
}

static SwipeDirection opposite(SwipeDirection direction)
{
    switch (direction) {
    case SwipeDirection::Invalid:
        return SwipeDirection::Invalid;
    case SwipeDirection::Down:
        return SwipeDirection::Up;
    case SwipeDirection::Up:
        return SwipeDirection::Down;
    case SwipeDirection::Left:
        return SwipeDirection::Right;
    case SwipeDirection::Right:
        return SwipeDirection::Left;
    }
    return SwipeDirection::Invalid;
}

std::function<void(qreal progress)> TogglableState::progressCallback()
{
    return [this](qreal progress) {
        setProgress(progress);
    };
}

std::function<void(qreal progress)> TogglableState::regressCallback()
{
    return [this](qreal progress) {
        setRegress(progress);
    };
}

void TogglableGesture::addTouchpadPinchGesture(PinchDirection direction, uint fingerCount)
{
    effects->registerTouchpadPinchShortcut(direction, fingerCount, m_state->activateAction(), m_state->progressCallback());
    effects->registerTouchpadPinchShortcut(opposite(direction), fingerCount, m_state->deactivateAction(), m_state->regressCallback());
}

void TogglableGesture::addTouchpadSwipeGesture(SwipeDirection direction, uint fingerCount)
{
    effects->registerTouchpadSwipeShortcut(direction, fingerCount, m_state->activateAction(), m_state->progressCallback());
    effects->registerTouchpadSwipeShortcut(opposite(direction), fingerCount, m_state->deactivateAction(), m_state->regressCallback());
}

void TogglableGesture::addTouchscreenSwipeGesture(SwipeDirection direction, uint fingerCount)
{
    effects->registerTouchscreenSwipeShortcut(direction, fingerCount, m_state->activateAction(), m_state->progressCallback());
    effects->registerTouchscreenSwipeShortcut(opposite(direction), fingerCount, m_state->deactivateAction(), m_state->regressCallback());
}

TogglableTouchBorder::TogglableTouchBorder(TogglableState *state)
    : QObject(state)
    , m_state(state)
{
}

TogglableTouchBorder::~TogglableTouchBorder()
{
    for (const ElectricBorder &border : std::as_const(m_touchBorderActivate)) {
        effects->unregisterTouchBorder(border, m_state->toggleAction());
    }
}

void TogglableTouchBorder::setBorders(const QList<int> &touchActivateBorders)
{
    for (const ElectricBorder &border : std::as_const(m_touchBorderActivate)) {
        effects->unregisterTouchBorder(border, m_state->toggleAction());
    }
    m_touchBorderActivate.clear();

    for (const int &border : touchActivateBorders) {
        m_touchBorderActivate.append(ElectricBorder(border));
        effects->registerRealtimeTouchBorder(ElectricBorder(border), m_state->toggleAction(), [this](ElectricBorder border, const QPointF &deltaProgress, const EffectScreen *screen) {
            if (m_state->status() == TogglableState::Status::Active) {
                return;
            }
            const int maxDelta = 500; // Arbitrary logical pixels value seems to behave better than scaledScreenSize
            qreal progress = 0;
            if (border == ElectricTop || border == ElectricBottom) {
                progress = std::min(1.0, std::abs(deltaProgress.y()) / maxDelta);
            } else {
                progress = std::min(1.0, std::abs(deltaProgress.x()) / maxDelta);
            }
            m_state->setProgress(progress);
        });
    }
}

}
