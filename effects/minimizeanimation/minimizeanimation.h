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

namespace KWin
{

/**
 * Animates minimize/unminimize
 **/
class MinimizeAnimationEffect : public Effect
{
    Q_OBJECT

public:
    MinimizeAnimationEffect();

    void reconfigure(ReconfigureFlags flags) override;

    virtual void prePaintScreen(ScreenPrePaintData &data, int time);
    virtual void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time);
    virtual void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data);
    virtual void postPaintScreen();
    virtual bool isActive() const;

    int requestedEffectChainPosition() const override {
        return 50;
    }

    static bool supported();

private Q_SLOTS:
    void windowDeleted(EffectWindow *w);
    void windowMinimized(EffectWindow *w);
    void windowUnminimized(EffectWindow *w);

private:
    std::chrono::milliseconds m_duration;
    QHash<const EffectWindow*, TimeLine> m_animations;
};

} // namespace

#endif
