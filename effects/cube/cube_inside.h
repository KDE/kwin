/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <mgraesslin@kde.org>

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

#ifndef KWIN_CUBE_INSIDE_H
#define KWIN_CUBE_INSIDE_H
#include <kwineffects.h>

namespace KWin
{

class CubeInsideEffect : public Effect
{
public:
    CubeInsideEffect() {}
    ~CubeInsideEffect() override {}

    virtual void paint() = 0;
    virtual void setActive(bool active) = 0;
};

} // namespace

#endif // KWIN_CUBE_INSIDE_H
