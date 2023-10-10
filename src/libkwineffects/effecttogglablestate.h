/*
    SPDX-FileCopyrightText: 2023 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "libkwineffects/kwinglobals.h"

#include <QAction>
#include <QObject>

namespace KWin
{

class Effect;

/**
 * It's common to have effects that get activated and deactivated.
 * This class helps us simplify this process, especially in the cases where we want activation to happen
 * progressively, like through a touch our touchpad events.
 */
class KWIN_EXPORT EffectTogglableState : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal partialActivationFactor READ partialActivationFactor NOTIFY partialActivationFactorChanged)
    Q_PROPERTY(QAction *activateAction READ activateAction CONSTANT)
    Q_PROPERTY(QAction *deactivateAction READ deactivateAction CONSTANT)
    Q_PROPERTY(QAction *toggleAction READ toggleAction CONSTANT)
    Q_PROPERTY(Status status READ status WRITE setStatus NOTIFY statusChanged)
public:
    enum class Status {
        Inactive,
        Activating,
        Deactivating,
        Active,
        Stopped
    };
    Q_ENUM(Status)

    /** Constructs the object, passes the effect as the parent. */
    EffectTogglableState(Effect *parent = nullptr);

    bool inProgress() const;

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
    void stop();
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
    void setInProgress(bool gesture);
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

class KWIN_EXPORT EffectTogglableGesture : public QObject
{
public:
    /**
     * Allows specifying which gestures toggle the state.
     *
     * The gesture will activate it and once enabled the opposite will disable it back.
     *
     * @param state the state we care about. This state will become the parent object and will take care to clean it up.
     *
     * If the @p state is nullptr, it will use the parent as the state
     */
    EffectTogglableGesture(EffectTogglableState *state = nullptr);

    Q_INVOKABLE void addTouchpadPinchGesture(PinchDirection dir, uint fingerCount);
    Q_INVOKABLE void addTouchpadSwipeGesture(SwipeDirection dir, uint fingerCount);
    Q_INVOKABLE void addTouchscreenSwipeGesture(SwipeDirection direction, uint fingerCount);
};

class KWIN_EXPORT EffectTogglableTouchBorder : public QObject
{
public:
    /**
     * Allows specifying which boarders get to toggle the state.
     *
     * @param state the state we care about. This state will become the parent object and will take care to clean it up.
     */
    EffectTogglableTouchBorder(EffectTogglableState *state);
    ~EffectTogglableTouchBorder();

    void setBorders(const QList<int> &borders);

private:
    QList<ElectricBorder> m_touchBorderActivate;
    EffectTogglableState *const m_state;
};

}
