/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <mgraesslin@kde.org>

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

namespace KWin
{

class MagicLampEffect
    : public Effect
{
    Q_OBJECT

public:
    MagicLampEffect();

    virtual void reconfigure(ReconfigureFlags);
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time);
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    virtual void postPaintScreen();
    virtual bool isActive() const;

    int requestedEffectChainPosition() const override {
        return 50;
    }

    static bool supported();

public Q_SLOTS:
    void slotWindowDeleted(KWin::EffectWindow *w);
    void slotWindowMinimized(KWin::EffectWindow *w);
    void slotWindowUnminimized(KWin::EffectWindow *w);

private:
    std::chrono::milliseconds m_duration;
    QHash<const EffectWindow*, TimeLine> m_animations;

    enum IconPosition {
        Top,
        Bottom,
        Left,
        Right
    };
};

} // namespace

#endif
