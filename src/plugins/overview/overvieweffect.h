/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "libkwineffects/kwinquickeffect.h"
#include "libkwineffects/togglablestate.h"

namespace KWin
{

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
