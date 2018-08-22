/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2019 Roman Gilg <subdiff@gmail.com>

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
#include "xwayland_interface.h"

namespace KWin
{

XwaylandInterface *s_self = nullptr;

XwaylandInterface *XwaylandInterface::self()
{
    return s_self;
}

XwaylandInterface::XwaylandInterface(QObject *parent)
    : QObject(parent)
{
    s_self = this;
}

XwaylandInterface::~XwaylandInterface()
{
    s_self = nullptr;
}

}
