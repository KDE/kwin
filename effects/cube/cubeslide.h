/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

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
public:
    CubeSlideEffect();
    ~CubeSlideEffect();
    virtual void reconfigure(ReconfigureFlags);
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void paintScreen(int mask, QRegion region, ScreenPaintData& data);
    virtual void postPaintScreen();
    virtual void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time);
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    virtual bool isActive() const;

    static bool supported();

private Q_SLOTS:
    void slotDesktopChanged(int old, int current);
    void slotWindowStepUserMovedResized(KWin::EffectWindow *w);
    void slotWindowFinishUserMovedResized(KWin::EffectWindow *w);

private:
    enum RotationDirection {
        Left,
        Right,
        Upwards,
        Downwards
    };
    void paintSlideCube(int mask, QRegion region, ScreenPaintData& data);
    void windowMovingChanged(float progress, RotationDirection direction);
    bool cube_painting;
    int front_desktop;
    int painting_desktop;
    int other_desktop;
    bool firstDesktop;
    QTimeLine timeLine;
    QQueue<RotationDirection> slideRotations;
    QSet<EffectWindow*> panels;
    QSet<EffectWindow*> stickyWindows;
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
