/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwinquickeffect.h>

#include "expolayout.h"

namespace KWin
{

class OverviewEffect : public QuickSceneEffect
{
    Q_OBJECT
    Q_PROPERTY(int animationDuration READ animationDuration NOTIFY animationDurationChanged)
    Q_PROPERTY(ExpoLayout::LayoutMode layout READ layout NOTIFY layoutChanged)
    Q_PROPERTY(bool blurBackground READ blurBackground NOTIFY blurBackgroundChanged)
    Q_PROPERTY(qreal partialActivationFactor READ partialActivationFactor NOTIFY partialActivationFactorChanged)
    // More efficient from a property binding pov rather than binding to partialActivationFactor !== 0
    Q_PROPERTY(bool gestureInProgress READ gestureInProgress NOTIFY gestureInProgressChanged)

public:
    enum class Status {
        Inactive,
        Activating,
        Active
    };
    OverviewEffect();
    ~OverviewEffect() override;

    ExpoLayout::LayoutMode layout() const;
    void setLayout(ExpoLayout::LayoutMode layout);

    int animationDuration() const;
    void setAnimationDuration(int duration);

    bool blurBackground() const;
    void setBlurBackground(bool blur);

    qreal partialActivationFactor() const;
    bool gestureInProgress() const;

    int requestedEffectChainPosition() const override;
    bool borderActivated(ElectricBorder border) override;
    void reconfigure(ReconfigureFlags flags) override;
    void grabbedKeyboardEvent(QKeyEvent *keyEvent) override;

Q_SIGNALS:
    void animationDurationChanged();
    void layoutChanged();
    void blurBackgroundChanged();
    void partialActivationFactorChanged();
    void gestureInProgressChanged();

public Q_SLOTS:
    void activate();
    void partialActivate();
    void deactivate();
    void quickDeactivate();
    void toggle();

protected:
    QVariantMap initialProperties(EffectScreen *screen) override;

private:
    void realDeactivate();

    QTimer *m_shutdownTimer;
    QAction *m_toggleAction = nullptr;
    QList<QKeySequence> m_toggleShortcut;
    QList<ElectricBorder> m_borderActivate;
    QList<ElectricBorder> m_touchBorderActivate;
    qreal m_partialActivationFactor = 0;
    bool m_blurBackground = false;
    Status m_status = Status::Inactive;
    int m_animationDuration = 200;
    ExpoLayout::LayoutMode m_layout = ExpoLayout::LayoutNatural;
};

} // namespace KWin
