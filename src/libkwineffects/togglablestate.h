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

class KWINEFFECTS_EXPORT TogglableState : public QObject
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

    TogglableState(Effect *parent);

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

    friend class TogglableGesture;
    friend class TogglableTouchBorder;
};

class KWINEFFECTS_EXPORT TogglableGesture : public QObject
{
public:
    TogglableGesture(TogglableState *state);

    void addTouchpadPinchGesture(PinchDirection dir, uint fingerCount);
    void addTouchpadSwipeGesture(SwipeDirection dir, uint fingerCount);
    void addTouchscreenSwipeGesture(SwipeDirection direction, uint fingerCount);

private:
    TogglableState *const m_state;
};

class KWINEFFECTS_EXPORT TogglableTouchBorder : public QObject
{
public:
    TogglableTouchBorder(TogglableState *state);
    ~TogglableTouchBorder();

    void setBorders(const QList<int> &borders);

private:
    QList<ElectricBorder> m_touchBorderActivate;
    TogglableState *const m_state;
};

}
