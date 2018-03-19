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
    : QObject(parent)
{
}

VirtualOutput::VirtualOutput(const VirtualOutput &o)
    : m_geo(o.m_geo),
      m_outputScale(o.m_outputScale),
      m_gammaSize(o.m_gammaSize),
      m_gammaResult(o.m_gammaResult)
{
}

VirtualOutput::~VirtualOutput()
{
}

}
