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
#ifndef KWIN_XWL_XWAYLAND_INTERFACE
#define KWIN_XWL_XWAYLAND_INTERFACE

#include <kwinglobals.h>

#include <QObject>

namespace KWin
{

class KWIN_EXPORT XwaylandInterface : public QObject
{
    Q_OBJECT
public:
    static XwaylandInterface *self();

protected:
    explicit XwaylandInterface(QObject *parent = nullptr);
    virtual ~XwaylandInterface();
};

inline XwaylandInterface *xwayland() {
    return XwaylandInterface::self();
}

}

#endif
