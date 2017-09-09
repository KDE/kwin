/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright © 2017-2018 Fredrik Höglund <fredrik@kde.org>

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

#include "shadow.h"

namespace KWin
{


VulkanShadow::VulkanShadow(Toplevel *toplevel, VulkanScene *scene)
    : Shadow(toplevel),
      m_scene(scene)
{
}


VulkanShadow::~VulkanShadow()
{
}


void VulkanShadow::buildQuads()
{
}


bool VulkanShadow::prepareBackend()
{
    return true;
}


} // namespace KWin
