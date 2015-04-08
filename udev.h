/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_UDEV_H
#define KWIN_UDEV_H

struct udev;

namespace KWin
{

class Udev
{
public:
    Udev();
    ~Udev();

    bool isValid() const {
        return m_udev != nullptr;
    }
    operator udev*() const {
        return m_udev;
    }
    operator udev*() {
        return m_udev;
    }
private:
    struct udev *m_udev;
};

}

#endif
