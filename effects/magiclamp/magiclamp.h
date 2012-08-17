/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com>

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

#ifndef KWIN_MAGICLAMP_H
#define KWIN_MAGICLAMP_H

#include <kwineffects.h>

class QTimeLine;

namespace KWin
{

class MagicLampEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int animationDuration READ animationDuration)
public:
    MagicLampEffect();

    virtual void reconfigure(ReconfigureFlags);
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time);
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    virtual void postPaintScreen();
    virtual bool isActive() const;

    static bool supported();

    // for properties
    int animationDuration() const {
        return mAnimationDuration;
    }
public Q_SLOTS:
    void slotWindowDeleted(KWin::EffectWindow *w);
    void slotWindowMinimized(KWin::EffectWindow *w);
    void slotWindowUnminimized(KWin::EffectWindow *w);

private:
    QHash< EffectWindow*, QTimeLine* > mTimeLineWindows;
    int mActiveAnimations;
    int mAnimationDuration;
    int mShadowOffset[4];

    enum IconPosition {
        Top,
        Bottom,
        Left,
        Right
    };
};

} // namespace

#endif
