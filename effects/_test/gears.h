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

#ifndef KWIN_GEARS_H
#define KWIN_GEARS_H
#include "../cube/cube_inside.h"
#include <kwinglutils.h>

/*
* This is a demo effect to show how to paint something (in this case the gears of glxgears)
* inside the cube.
*/

namespace KWin
{

class GearsEffect : public CubeInsideEffect
{
public:
    GearsEffect();
    ~GearsEffect();

    virtual void paint();
    virtual void setActive(bool active);
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void postPaintScreen();

    static bool supported();
private:
    void gear(float inner_radius, float outer_radius, float width, int teeth, float tooth_depth);
    void paintGears();
    void initGears();
    void endGears();

    bool m_active;
    GLuint m_gear1;
    GLuint m_gear2;
    GLuint m_gear3;
    float m_contentRotation;
    float m_angle;
};

} //namespace

#endif // KWIN_GEARS_H
