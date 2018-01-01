/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>

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

#ifndef KWIN_SLIDE_H
#define KWIN_SLIDE_H

#include <kwineffects.h>
#include <QObject>
#include <QTimeLine>

namespace KWin
{

class SlideEffect
    : public Effect
{
    Q_OBJECT
public:
    SlideEffect();
    virtual void reconfigure(ReconfigureFlags);
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void paintScreen(int mask, QRegion region, ScreenPaintData& data);
    virtual void postPaintScreen();
    virtual void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time);
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    virtual bool isActive() const;

    int requestedEffectChainPosition() const override {
        return 50;
    }

    static bool supported();

private Q_SLOTS:
    void slotDesktopChanged(int old, int current, KWin::EffectWindow* with);

private:
    void windowAdded(EffectWindow* w);
    void windowDeleted(EffectWindow* w);
    bool shouldForceBackgroundContrast(const EffectWindow* w) const;
    QList< EffectWindow* > m_backgroundContrastForcedBefore;
    QRect desktopRect(int desktop) const;
    QTimeLine mTimeLine;
    int painting_desktop;
    bool slide;
    QPoint slide_start_pos;
    bool slide_painting_sticky;
    bool slide_painting_keep_above;
    QPoint slide_painting_diff;
    EffectWindow* m_movingWindow = nullptr;

};

} // namespace

#endif
