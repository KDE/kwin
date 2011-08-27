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

#ifndef KWIN_SNAPHELPER_H
#define KWIN_SNAPHELPER_H

#include <kwineffects.h>
#include <QtCore/QTimeLine>

namespace KWin
{

class SnapHelperEffect
    : public Effect
{
    Q_OBJECT
public:
    SnapHelperEffect();
    ~SnapHelperEffect();

    virtual void reconfigure(ReconfigureFlags);

    virtual void prePaintScreen(ScreenPrePaintData &data, int time);
    virtual void postPaintScreen();
    virtual bool isActive() const;

    static bool supported();

public Q_SLOTS:
    void slotWindowClosed(EffectWindow *w);
    void slotWindowStartUserMovedResized(EffectWindow *w);
    void slotWindowFinishUserMovedResized(EffectWindow *w);

private:
    bool m_active;
    EffectWindow* m_window;
    QTimeLine m_timeline;
    //GC m_gc;
};

} // namespace

#endif
