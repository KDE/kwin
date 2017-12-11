/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2017 Roman Gilg <subdiff@gmail.com>

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
#ifndef KWIN_GAMMARAMP_H
#define KWIN_GAMMARAMP_H

namespace KWin
{

namespace ColorCorrect
{

struct GammaRamp {
    GammaRamp(int _size) {
        size = _size;
        red = new uint16_t[3 * _size];
        green = red + _size;
        blue = green + _size;
    }
    ~GammaRamp() {
        delete[] red;
        red = green = blue = nullptr;
    }

    uint32_t size = 0;
    uint16_t *red = nullptr;
    uint16_t *green = nullptr;
    uint16_t *blue = nullptr;
};

}
}

#endif // KWIN_GAMMARAMP_H
