/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2010 Thomas Lübking <thomas.luebking@web.de>

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

#ifndef WINDOWGEOMETRY_H
#define WINDOWGEOMETRY_H

#include <kwineffects.h>

namespace KWin
{

class WindowGeometry : public Effect
{
    Q_OBJECT
public:
    WindowGeometry();
    ~WindowGeometry();

    inline bool provides(Effect::Feature ef) {
        return ef == Effect::GeometryTip;
    }
    void reconfigure(ReconfigureFlags);
    void paintScreen(int mask, QRegion region, ScreenPaintData &data);
    virtual bool isActive() const;

private slots:
    void toggle();
    void slotWindowStartUserMovedResized(KWin::EffectWindow *w);
    void slotWindowFinishUserMovedResized(KWin::EffectWindow *w);
    void slotWindowStepUserMovedResized(KWin::EffectWindow *w, const QRect &geometry);
private:
    EffectWindow *myResizeWindow;
    EffectFrame *myMeasure[3];
    QRect myOriginalGeometry, myCurrentGeometry;
    bool iAmActive, iAmActivated, iHandleMoves, iHandleResizes;
    QString myCoordString[2], myResizeString;
};

} // namespace

#endif
