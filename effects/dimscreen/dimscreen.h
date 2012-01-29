/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008, 2009 Martin Gräßlin <kde@martin-graesslin.com>

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

#ifndef KWIN_DIMSCREEN_H
#define KWIN_DIMSCREEN_H

#include <kwineffects.h>
#include <QtCore/QTimeLine>

namespace KWin
{

class DimScreenEffect
    : public Effect
{
    Q_OBJECT
public:
    DimScreenEffect();
    ~DimScreenEffect();

    virtual void reconfigure(ReconfigureFlags);
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void postPaintScreen();
    virtual void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data);
    virtual bool isActive() const;

public Q_SLOTS:
    void slotWindowActivated(KWin::EffectWindow *w);

private:
    bool mActivated;
    bool activateAnimation;
    bool deactivateAnimation;
    QTimeLine timeline;
    EffectWindow* window;
};

} // namespace

#endif
