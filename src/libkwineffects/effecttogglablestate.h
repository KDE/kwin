/*
    SPDX-FileCopyrightText: 2023 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "libkwineffects/kwineffects_export.h"
#include <QAction>
#include <QObject>
#include <kwinglobals.h>

namespace KWin
{

class Effect;

class KWINEFFECTS_EXPORT EffectTogglableState : public QObject
{
    Q_OBJECT
public:
    enum class Status {
        Inactive,
        Activating,
        Deactivating,
        Active
    };
    Q_ENUM(Status);

    EffectTogglableState(Effect *parent);

    bool inProgress() const;
    void setInProgress(bool gesture);

    qreal partialActivationFactor() const
    {
        return m_partialActivationFactor;
    }
    void setPartialActivationFactor(qreal factor);

    QAction *activateAction() const
    {
        return m_activateAction.get();
    }
    QAction *deactivateAction() const
    {
        return m_deactivateAction.get();
    }
    QAction *toggleAction() const
    {
        return m_toggleAction.get();
    }

    void activate();
    void deactivate();
    void toggle();
    void setStatus(Status status);
    Status status() const
    {
        return m_status;
    }

Q_SIGNALS:
    void inProgressChanged();
    void partialActivationFactorChanged();
    void activated();
    void deactivated();
    void statusChanged(Status status);

protected:
    std::function<void(qreal progress)> progressCallback();
    std::function<void(qreal progress)> regressCallback();
    void setProgress(qreal progress);

    /// regress being the progress when on an active state
    void setRegress(qreal regress);

private:
    void partialActivate(qreal factor);
    void partialDeactivate(qreal factor);

    std::unique_ptr<QAction> m_deactivateAction;
    std::unique_ptr<QAction> m_activateAction;
    std::unique_ptr<QAction> m_toggleAction;
    Status m_status = Status::Inactive;
    bool m_inProgress = false;
    qreal m_partialActivationFactor = 0;

    friend class EffectTogglableGesture;
    friend class EffectTogglableTouchBorder;
};

class KWINEFFECTS_EXPORT EffectTogglableGesture : public QObject
{
public:
    EffectTogglableGesture(EffectTogglableState *state);

    void addTouchpadPinchGesture(PinchDirection dir, uint fingerCount);
    void addTouchpadSwipeGesture(SwipeDirection dir, uint fingerCount);
    void addTouchscreenSwipeGesture(SwipeDirection direction, uint fingerCount);

private:
    EffectTogglableState *const m_state;
};

class KWINEFFECTS_EXPORT EffectTogglableTouchBorder : public QObject
{
public:
    EffectTogglableTouchBorder(EffectTogglableState *state);
    ~EffectTogglableTouchBorder();

    void setBorders(const QList<int> &borders);

private:
    QList<ElectricBorder> m_touchBorderActivate;
    EffectTogglableState *const m_state;
};

}
