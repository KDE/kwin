/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// kwineffects
#include "effect/effect.h"
#include "effect/effectwindow.h"
#include "effect/springmotion.h"
#include "effect/timeline.h"

namespace KWin
{

/*
 * How it Works:
 *
 * This effect doesn't change the current desktop, only receives changes from the VirtualDesktopManager.
 * The only visually apparent inputs are desktopChanged() and desktopChanging().
 *
 * When responding to desktopChanging(), the draw position is only affected by what's received from there.
 * After desktopChanging() is done, or without desktopChanging() having been called at all, desktopChanged() is called.
 * The desktopChanged() function configures the m_startPos and m_endPos for the animation, and the duration.
 *
 * m_currentPosition and everything else not labeled "drawCoordinate" uses desktops as a unit.
 * Exmp: 1.2 means the desktop at index 1 shifted over by .2 desktops.
 * All coords must be positive.
 *
 * For the wrapping effect, the render loop has to handle desktop coordinates larger than the total grid's width.
 * 1. It uses modulus to keep the desktop coords in the range [0, gridWidth].
 * 2. It will draw the desktop at index 0 at index gridWidth if it has to.
 * I will not draw any thing farther outside the range than that.
 *
 * I've put an explanation of all the important private vars down at the bottom.
 *
 * Good luck :)
 */

class SlideEffect;

class SlideEffectScreen
{
public:
    SlideEffectScreen(SlideEffect *parent, LogicalOutput *screen);
    ~SlideEffectScreen();
    void reconfigure();

    void prePaintScreen(ScreenPrePaintData &data);
    void paintScreen();
    void postPaintScreen();

    bool isActive() const;
    void desktopChanged(VirtualDesktop *old, VirtualDesktop *current, EffectWindow *with);
    void desktopChanging(VirtualDesktop *old, QPointF desktopOffset, EffectWindow *with);
    void desktopChangingCancelled();
    void windowAdded(EffectWindow *w);
    void windowDeleted(EffectWindow *w);
    void finishedSwitching();

private:
    QPoint getDrawCoords(QPointF pos, LogicalOutput *screen);
    bool isTranslated(const EffectWindow *w) const;
    bool willBePainted(const EffectWindow *w) const;
    bool shouldElevate(const EffectWindow *w) const;
    QPointF moveInsideDesktopGrid(QPointF p);
    QPointF constrainToDrawableRange(QPointF p);
    QPointF forcePositivePosition(QPointF p) const;
    void optimizePath(); // Find the best path to target desktop

    void startAnimation(const QPointF &oldPos, VirtualDesktop *current, EffectWindow *movingWindow = nullptr);
    void prepareSwitching();

    void prePaintItems();

private:
    enum class State {
        Inactive,
        ActiveAnimation,
        ActiveGesture,
    };

    State m_state = State::Inactive;
    SpringMotion m_motionX;
    SpringMotion m_motionY;

    // When the desktop isn't desktopChanging(), these two variables are used to control the animation path.
    // They use desktops as a unit.
    QPointF m_startPos;
    QPointF m_endPos;

    QPointF m_gesturePos;

    EffectWindow *m_movingWindow = nullptr;
    AnimationClock m_clock;

    struct
    {
        QPointF position;
        bool wrap;
        QList<VirtualDesktop *> visibleDesktops;
    } m_paintCtx;

    struct WindowData
    {
        std::unique_ptr<Item> transformItem;
        std::vector<std::unique_ptr<Item>> mirrorItems;
    };

    QList<EffectWindow *> m_elevatedWindows;
    std::unordered_map<EffectWindow *, WindowData> m_windowData;
    SlideEffect *m_parent;
    LogicalOutput *m_screen;
};

class SlideEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int horizontalGap READ horizontalGap)
    Q_PROPERTY(int verticalGap READ verticalGap)
    Q_PROPERTY(bool slideBackground READ slideBackground)

public:
    SlideEffect();
    ~SlideEffect() override;

    void reconfigure(ReconfigureFlags) override;

    void prePaintScreen(ScreenPrePaintData &data) override;
    void postPaintScreen() override;

    bool isActive() const override;
    int requestedEffectChainPosition() const override;

    static bool supported();

    int horizontalGap() const;
    int verticalGap() const;
    bool slideBackground() const;

private Q_SLOTS:
    void desktopChanged(VirtualDesktop *old, VirtualDesktop *current, EffectWindow *with, LogicalOutput *output);
    void desktopChanging(VirtualDesktop *old, QPointF desktopOffset, EffectWindow *with, LogicalOutput *output);
    void desktopChangingCancelled();
    void windowAdded(EffectWindow *w);
    void windowDeleted(EffectWindow *w);

private:
    void finishedSwitching();
    SlideEffectScreen *getSlideEffectScreen(LogicalOutput *screen);

private:
    int m_hGap;
    int m_vGap;
    bool m_slideBackground;

    bool m_switchingActivity = false;
    std::unordered_map<LogicalOutput *, SlideEffectScreen> m_slideEffectScreens;
};

inline int SlideEffect::horizontalGap() const
{
    return m_hGap;
}

inline int SlideEffect::verticalGap() const
{
    return m_vGap;
}

inline bool SlideEffect::slideBackground() const
{
    return m_slideBackground;
}

inline bool SlideEffect::isActive() const
{
    return std::ranges::any_of(m_slideEffectScreens | std::views::values, &SlideEffectScreen::isActive);
}

inline bool SlideEffectScreen::isActive() const
{
    return m_state != State::Inactive;
}

inline int SlideEffect::requestedEffectChainPosition() const
{
    return 50;
}

} // namespace KWin
