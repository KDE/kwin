/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

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

#ifndef KWIN_FADEDESKTOP_H
#define KWIN_FADEDESKTOP_H

#include <kwineffects.h>
#include <QObject>
#include <QtCore/QTimeLine>

namespace KWin
{

class FadeDesktopEffect
    : public Effect
{
    Q_OBJECT
public:
    FadeDesktopEffect();
    virtual void reconfigure(ReconfigureFlags);
    virtual void prePaintScreen(ScreenPrePaintData &data, int time);
    virtual void postPaintScreen();
    virtual void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time);
    virtual void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data);
    virtual bool isActive() const;

private Q_SLOTS:
    void slotDesktopChanged(int old);

private:
    bool m_fading;
    QTimeLine m_timeline;
    int m_oldDesktop;
};

} // namespace

#endif
