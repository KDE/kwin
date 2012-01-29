/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

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

#ifndef KWIN_DIMINACTIVE_H
#define KWIN_DIMINACTIVE_H

// Include with base class for effects.
#include <kwineffects.h>
#include <QtCore/QTimeLine>


namespace KWin
{

class DimInactiveEffect
    : public Effect
{
    Q_OBJECT
public:
    DimInactiveEffect();
    virtual void reconfigure(ReconfigureFlags);
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);

public Q_SLOTS:
    void slotWindowActivated(KWin::EffectWindow* c);
    void slotWindowDeleted(KWin::EffectWindow *w);

private:
    bool dimWindow(const EffectWindow* w) const;
    QTimeLine timeline;
    EffectWindow* active;
    EffectWindow* previousActive;
    QTimeLine previousActiveTimeline;
    int dim_strength; // reduce saturation and brightness by this percentage
    bool dim_panels; // do/don't dim also all panels
    bool dim_desktop; // do/don't dim the desktop
    bool dim_keepabove; // do/don't dim keep-above windows
    bool dim_by_group; // keep visible all windows from the active window's group or only the active window
};

} // namespace

#endif
