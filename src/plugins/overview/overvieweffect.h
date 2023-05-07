/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "libkwineffects/kwinquickeffect.h"

namespace KWin
{

class TogglableState : public QObject
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

    TogglableState(QObject *parent);

    bool inProgress() const;
    void setInProgress(bool gesture);

    qreal partialActivationFactor() const
    {
        return m_partialActivationFactor;
    }
    void setPartialActivationFactor(qreal factor);

    QAction *activateAction() const
    {
        return m_activateAction;
    }
    QAction *deactivateAction() const
    {
        return m_deactivateAction;
    }
    QAction *toggleAction() const
    {
        return m_toggleAction;
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
    void setProgress(qreal progress);

    /// regress being the progress when on an active state
    void setRegress(qreal regress);

private:
    void partialActivate(qreal factor);
    void partialDeactivate(qreal factor);

    QAction *m_deactivateAction = nullptr;
    QAction *m_activateAction = nullptr;
    QAction *m_toggleAction = nullptr;
    Status m_status = Status::Inactive;
    bool m_inProgress = false;
    qreal m_partialActivationFactor = 0;

    friend class TogglableGesture;
    friend class TogglableTouchBorder;
};

class TogglableGesture : public QObject
{
public:
    TogglableGesture(TogglableState *state);

    void addTouchpadGesture(PinchDirection dir, uint fingerCount);
    void addTouchscreenSwipeGesture(SwipeDirection direction, uint fingerCount);

private:
    TogglableState *const m_state;
};

class TogglableTouchBorder : public QObject
{
public:
    TogglableTouchBorder(TogglableState *state);
    ~TogglableTouchBorder();

    void setBorders(const QList<int> &borders);

private:
    QList<ElectricBorder> m_touchBorderActivate;
    TogglableState *const m_state;
};

class OverviewEffect : public QuickSceneEffect
{
    Q_OBJECT
    Q_PROPERTY(int animationDuration READ animationDuration NOTIFY animationDurationChanged)
    Q_PROPERTY(int layout READ layout NOTIFY layoutChanged)
    Q_PROPERTY(bool ignoreMinimized READ ignoreMinimized NOTIFY ignoreMinimizedChanged)
    Q_PROPERTY(qreal partialActivationFactor READ partialActivationFactor NOTIFY partialActivationFactorChanged)
    // More efficient from a property binding pov rather than binding to partialActivationFactor !== 0
    Q_PROPERTY(bool gestureInProgress READ gestureInProgress NOTIFY gestureInProgressChanged)
    Q_PROPERTY(QString searchText MEMBER m_searchText NOTIFY searchTextChanged)

public:
    OverviewEffect();
    ~OverviewEffect() override;

    int layout() const;
    void setLayout(int layout);

    bool ignoreMinimized() const;

    int animationDuration() const;
    void setAnimationDuration(int duration);

    qreal partialActivationFactor() const
    {
        return m_state->partialActivationFactor();
    }

    bool gestureInProgress() const
    {
        return m_state->inProgress();
    }

    int requestedEffectChainPosition() const override;
    bool borderActivated(ElectricBorder border) override;
    void reconfigure(ReconfigureFlags flags) override;
    void grabbedKeyboardEvent(QKeyEvent *keyEvent) override;

Q_SIGNALS:
    void animationDurationChanged();
    void layoutChanged();
    void partialActivationFactorChanged();
    void gestureInProgressChanged();
    void ignoreMinimizedChanged();
    void searchTextChanged();

public Q_SLOTS:
    void activate();
    void deactivate();

private:
    void realDeactivate();

    TogglableState *const m_state;
    TogglableTouchBorder *const m_border;

    QTimer *m_shutdownTimer;
    QList<QKeySequence> m_toggleShortcut;
    QList<ElectricBorder> m_borderActivate;
    QString m_searchText;
    int m_animationDuration = 400;
    int m_layout = 1;
};

} // namespace KWin
