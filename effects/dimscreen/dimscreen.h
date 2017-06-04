/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008, 2009 Martin Gräßlin <mgraesslin@kde.org>

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
#include <QTimeLine>

namespace KWin
{

class DimScreenEffect
    : public Effect
{
    Q_OBJECT
public:
    DimScreenEffect();
    ~DimScreenEffect();

    void reconfigure(ReconfigureFlags) override;
    void prePaintScreen(ScreenPrePaintData& data, int time) override;
    void postPaintScreen() override;
    void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;
    bool isActive() const override;

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
