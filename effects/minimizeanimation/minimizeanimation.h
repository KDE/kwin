/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

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

#ifndef KWIN_MINIMIZEANIMATION_H
#define KWIN_MINIMIZEANIMATION_H

// Include with base class for effects.
#include <kwineffects.h>

class QTimeLine;

namespace KWin
{

/**
 * Animates minimize/unminimize
 **/
class MinimizeAnimationEffect
    : public Effect
{
    Q_OBJECT
public:
    MinimizeAnimationEffect();

    void prePaintScreen(ScreenPrePaintData& data, int time) override;
    void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time) override;
    void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data) override;
    void postPaintScreen() override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override {
        return 50;
    }

public Q_SLOTS:
    void slotWindowDeleted(KWin::EffectWindow *w);
    void slotWindowMinimized(KWin::EffectWindow *w);
    void slotWindowUnminimized(KWin::EffectWindow *w);

private:
    QHash< EffectWindow*, QTimeLine* > mTimeLineWindows;
    int mActiveAnimations;
};

} // namespace

#endif
