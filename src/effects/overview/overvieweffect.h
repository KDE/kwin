/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwineffects.h>
#include <kwineffectquickview.h>

#include "expolayout.h"

namespace KWin
{

class OverviewEffect;

class OverviewScreenView : public EffectQuickScene
{
    Q_OBJECT

public:
    OverviewScreenView(EffectScreen *screen, QWindow *renderWindow, OverviewEffect *effect);

    bool isDirty() const;
    void markDirty();
    void resetDirty();

public Q_SLOTS:
    void stop();
    void scheduleRepaint();

private:
    bool m_dirty = false;
};

class OverviewEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int animationDuration READ animationDuration NOTIFY animationDurationChanged)
    Q_PROPERTY(ExpoLayout::LayoutMode layout READ layout NOTIFY layoutChanged)
    Q_PROPERTY(bool blurBackground READ blurBackground NOTIFY blurBackgroundChanged)

public:
    OverviewEffect();
    ~OverviewEffect() override;

    ExpoLayout::LayoutMode layout() const;
    void setLayout(ExpoLayout::LayoutMode layout);

    int animationDuration() const;
    void setAnimationDuration(int duration);

    bool blurBackground() const;
    void setBlurBackground(bool blur);

    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    void postPaintScreen() override;
    bool isActive() const override;
    int requestedEffectChainPosition() const override;
    bool borderActivated(ElectricBorder border) override;
    void reconfigure(ReconfigureFlags flags) override;

    void windowInputMouseEvent(QEvent *event) override;
    void grabbedKeyboardEvent(QKeyEvent *keyEvent) override;

    static bool supported();

Q_SIGNALS:
    void animationDurationChanged();
    void layoutChanged();
    void blurBackgroundChanged();

public Q_SLOTS:
    void activate();
    void deactivate();
    void quickDeactivate();
    void toggle();

private:
    void handleScreenAdded(EffectScreen *screen);
    void handleScreenRemoved(EffectScreen *screen);

    void realDeactivate();
    void createScreenView(EffectScreen *screen);

    QScopedPointer<QWindow> m_dummyWindow;
    QTimer *m_shutdownTimer;
    QHash<EffectScreen *, OverviewScreenView *> m_screenViews;
    EffectScreen *m_paintedScreen;
    QAction *m_toggleAction = nullptr;
    QList<QKeySequence> m_toggleShortcut;
    QList<ElectricBorder> m_borderActivate;
    QList<ElectricBorder> m_touchBorderActivate;
    bool m_blurBackground = false;
    bool m_activated = false;
    int m_animationDuration = 200;
    ExpoLayout::LayoutMode m_layout = ExpoLayout::LayoutNatural;
};

} // namespace KWin
