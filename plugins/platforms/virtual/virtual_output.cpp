/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2018 Roman Gilg <subdiff@gmail.com>

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
#include "virtual_output.h"

namespace KWin
{

VirtualOutput::VirtualOutput(QObject *parent)
    : AbstractWaylandOutput()
{
    Q_UNUSED(parent);

    setScale(1.);
}

VirtualOutput::~VirtualOutput()
{
}

QSize VirtualOutput::pixelSize() const
{
    return m_pixelSize;
}

void VirtualOutput::setGeometry(const QRect &geo)
{
    m_pixelSize = geo.size();
    setRawPhysicalSize(m_pixelSize);
    setGlobalPos(geo.topLeft());
}

}
