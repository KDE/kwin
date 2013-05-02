/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (c) 2003, Karol Szwed <kszwed@kde.org>

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

#ifndef KWIN_GEOMETRY_TIP_H
#define KWIN_GEOMETRY_TIP_H

#include <QLabel>
#include <X11/Xutil.h>
#include <fixx11h.h>

namespace KWin
{

class GeometryTip: public QLabel
{
    Q_OBJECT
public:
    GeometryTip(const XSizeHints* xSizeHints);
    ~GeometryTip();
    void setGeometry(const QRect& geom);

private:
    const XSizeHints* sizeHints;
};

} // namespace

#endif
