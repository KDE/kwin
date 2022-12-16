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
#include <kwineffects.h>

#include "springmotion.h"

namespace KWin
{

/*
 * How it Works:
 *
 * This effect doesn't change the current desktop, only recieves changes from the VirtualDesktopManager.
 * The only visually aparent inputs are desktopChanged() and desktopChanging().
 *
 * When responding to desktopChanging(), the draw position is only affected by what's recieved from there.
 * After desktopChanging() is done, or without desktopChanging() having been called at all, desktopChanged() is called.
 * The desktopChanged() function configures the m_startPos and m_endPos for the animation, and the duration.
 *
 * m_currentPosition and everything else not labeled "drawCoordinate" uses desktops as a unit.
 * Exmp: 1.2 means the dekstop at index 1 shifted over by .2 desktops.
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

    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    void postPaintScreen() override;

    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;

    bool isActive() const override;
    int requestedEffectChainPosition() const override;

    static bool supported();

    int horizontalGap() const;
    int verticalGap() const;
    bool slideBackground() const;

private Q_SLOTS:
    void desktopChanged(int old, int current, EffectWindow *with);
    void desktopChanging(uint old, QPointF desktopOffset, EffectWindow *with);
    void desktopChangingCancelled();
    void windowAdded(EffectWindow *w);
    void windowDeleted(EffectWindow *w);

private:
    QPoint getDrawCoords(QPointF pos, EffectScreen *screen);
    bool isTranslated(const EffectWindow *w) const;
    bool willBePainted(const EffectWindow *w) const;
    bool shouldElevate(const EffectWindow *w) const;
    QPointF moveInsideDesktopGrid(QPointF p);
    QPointF constrainToDrawableRange(QPointF p);
    QPointF forcePositivePosition(QPointF p) const;
    void optimizePath(); // Find the best path to target desktop

    void startAnimation(int old, int current, EffectWindow *movingWindow = nullptr);
    void prepareSwitching();
    void finishedSwitching();

private:
    int m_hGap;
    int m_vGap;
    bool m_slideBackground;

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

    EffectWindow *m_movingWindow = nullptr;
    std::chrono::milliseconds m_lastPresentTime = std::chrono::milliseconds::zero();
    QPointF m_currentPosition; // Should always be kept up to date with where on the grid we're seeing.

    struct
    {
        bool wrap;
        QVector<int> visibleDesktops;
    } m_paintCtx;

    struct WindowData
    {
        EffectWindowVisibleRef visibilityRef;
    };

    EffectWindowList m_elevatedWindows;
    QHash<EffectWindow *, WindowData> m_windowData;
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
    return m_state != State::Inactive;
}

inline int SlideEffect::requestedEffectChainPosition() const
{
    return 50;
}

} // namespace KWin
