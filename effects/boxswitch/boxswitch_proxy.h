/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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

#ifndef KWIN_BOXSWITCH_PROXY_H
#define KWIN_BOXSWITCH_PROXY_H

class QRegion;

namespace KWin
{

class BoxSwitchEffect;

class BoxSwitchEffectProxy
{
public:
    BoxSwitchEffectProxy(BoxSwitchEffect* effect);
    ~BoxSwitchEffectProxy();

    /**
    * Activates the BoxSwitch Effect. It will get deactivated in tabBoxClosed()
    * @param mode The TabBoxMode for which it should be activated.
    * @param animate switch will be animated
    * @param showText the caption of the window will be shown
    * @param positioningFactor where to put the frame: 0.0 == top, 1.0 == bottom
    */
    void activate(int mode, bool animate, bool showText, float positioningFactor);
    void paintWindowsBox(const QRegion& region);

private:
    BoxSwitchEffect* m_effect;
};

} // namespace

#endif // KWIN_BOXSWITCH_PROXY_H
