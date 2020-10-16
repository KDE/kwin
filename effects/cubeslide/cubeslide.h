/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_CUBESLIDE_H
#define KWIN_CUBESLIDE_H

#include <kwineffects.h>
#include <kwinglutils.h>
#include <QQueue>
#include <QSet>
#include <QTimeLine>

namespace KWin
{
class CubeSlideEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int rotationDuration READ configuredRotationDuration)
    Q_PROPERTY(bool dontSlidePanels READ isDontSlidePanels)
    Q_PROPERTY(bool dontSlideStickyWindows READ isDontSlideStickyWindows)
    Q_PROPERTY(bool usePagerLayout READ isUsePagerLayout)
    Q_PROPERTY(bool useWindowMoving READ isUseWindowMoving)
public:
    CubeSlideEffect();
    ~CubeSlideEffect() override;
    void reconfigure(ReconfigureFlags) override;
    void prePaintScreen(ScreenPrePaintData& data, int time) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData& data) override;
    void postPaintScreen() override;
    void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time) override;
    void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data) override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override {
        return 50;
    }

    static bool supported();

    // for properties
    int configuredRotationDuration() const {
        return rotationDuration;
    }
    bool isDontSlidePanels() const {
        return dontSlidePanels;
    }
    bool isDontSlideStickyWindows() const {
        return dontSlideStickyWindows;
    }
    bool isUsePagerLayout() const {
        return usePagerLayout;
    }
    bool isUseWindowMoving() const {
        return useWindowMoving;
    }
private Q_SLOTS:
    void slotWindowAdded(EffectWindow* w);
    void slotWindowDeleted(EffectWindow* w);

    void slotDesktopChanged(int old, int current, EffectWindow* w);
    void slotWindowStepUserMovedResized(KWin::EffectWindow *w);
    void slotWindowFinishUserMovedResized(KWin::EffectWindow *w);
    void slotNumberDesktopsChanged();

private:
    enum RotationDirection {
        Left,
        Right,
        Upwards,
        Downwards
    };
    void paintSlideCube(int mask, QRegion region, ScreenPaintData& data);
    void windowMovingChanged(float progress, RotationDirection direction);

    bool shouldAnimate(const EffectWindow* w) const;
    void startAnimation();

    bool cube_painting;
    int front_desktop;
    int painting_desktop;
    int other_desktop;
    bool firstDesktop;
    bool stickyPainting;
    QSet<EffectWindow*> staticWindows;
    QTimeLine timeLine;
    QQueue<RotationDirection> slideRotations;
    bool dontSlidePanels;
    bool dontSlideStickyWindows;
    bool usePagerLayout;
    int rotationDuration;
    bool useWindowMoving;
    bool windowMoving;
    bool desktopChangedWhileMoving;
    double progressRestriction;
};
}

#endif
