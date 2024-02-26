/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effecttogglablestate.h"
#include "effect/quickeffect.h"

namespace KWin
{

class VirtualDesktop;

class OverviewEffect : public QuickSceneEffect
{
    Q_OBJECT
    Q_PROPERTY(int animationDuration READ animationDuration NOTIFY animationDurationChanged)
    Q_PROPERTY(bool ignoreMinimized READ ignoreMinimized NOTIFY ignoreMinimizedChanged)
    Q_PROPERTY(bool filterWindows READ filterWindows NOTIFY filterWindowsChanged)
    Q_PROPERTY(bool organizedGrid READ organizedGrid NOTIFY organizedGridChanged)
    Q_PROPERTY(qreal overviewPartialActivationFactor READ overviewPartialActivationFactor NOTIFY overviewPartialActivationFactorChanged)
    // More efficient from a property binding pov rather than checking if partialActivationFactor is strictly between 0 and 1
    Q_PROPERTY(bool overviewGestureInProgress READ overviewGestureInProgress NOTIFY overviewGestureInProgressChanged)
    Q_PROPERTY(qreal transitionPartialActivationFactor READ transitionPartialActivationFactor NOTIFY transitionPartialActivationFactorChanged)
    Q_PROPERTY(bool transitionGestureInProgress READ transitionGestureInProgress NOTIFY transitionGestureInProgressChanged)
    Q_PROPERTY(qreal gridPartialActivationFactor READ gridPartialActivationFactor NOTIFY gridPartialActivationFactorChanged)
    Q_PROPERTY(bool gridGestureInProgress READ gridGestureInProgress NOTIFY gridGestureInProgressChanged)
    Q_PROPERTY(QPointF desktopOffset READ desktopOffset NOTIFY desktopOffsetChanged)
    Q_PROPERTY(QString searchText MEMBER m_searchText NOTIFY searchTextChanged)

public:
    OverviewEffect();
    ~OverviewEffect() override;

    bool ignoreMinimized() const;
    bool organizedGrid() const;

    bool filterWindows() const;
    void setFilterWindows(bool filterWindows);

    int animationDuration() const;
    void setAnimationDuration(int duration);

    qreal overviewPartialActivationFactor() const;
    bool overviewGestureInProgress() const;
    qreal transitionPartialActivationFactor() const;
    bool transitionGestureInProgress() const;
    qreal gridPartialActivationFactor() const;
    bool gridGestureInProgress() const;
    QPointF desktopOffset() const;

    int requestedEffectChainPosition() const override;
    bool borderActivated(ElectricBorder border) override;
    void reconfigure(ReconfigureFlags flags) override;
    void grabbedKeyboardEvent(QKeyEvent *keyEvent) override;

    Q_INVOKABLE void swapDesktops(KWin::VirtualDesktop *from, KWin::VirtualDesktop *to);

Q_SIGNALS:
    void animationDurationChanged();
    void overviewPartialActivationFactorChanged();
    void overviewGestureInProgressChanged();
    void transitionPartialActivationFactorChanged();
    void transitionGestureInProgressChanged();
    void gridPartialActivationFactorChanged();
    void gridGestureInProgressChanged();
    void ignoreMinimizedChanged();
    void filterWindowsChanged();
    void organizedGridChanged();
    void desktopOffsetChanged();
    void searchTextChanged();

public Q_SLOTS:
    void activate();
    void deactivate();

private:
    void realDeactivate();
    void cycle();
    void reverseCycle();

    EffectTogglableState *const m_overviewState;
    EffectTogglableState *const m_transitionState;
    EffectTogglableState *const m_gridState;
    EffectTogglableTouchBorder *const m_border;
    EffectTogglableTouchBorder *const m_gridBorder;

    QTimer *m_shutdownTimer;
    QList<QKeySequence> m_cycleShortcut;
    QList<QKeySequence> m_reverseCycleShortcut;
    QList<QKeySequence> m_overviewShortcut;
    QList<QKeySequence> m_gridShortcut;
    QList<ElectricBorder> m_borderActivate;
    QList<ElectricBorder> m_gridBorderActivate;
    QString m_searchText;
    QPointF m_desktopOffset;
    bool m_filterWindows = true;
    int m_animationDuration = 400;
};

} // namespace KWin
