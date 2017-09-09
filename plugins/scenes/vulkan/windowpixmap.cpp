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

#include "windowpixmap.h"

namespace KWin
{


VulkanWindowPixmap::VulkanWindowPixmap(Scene::Window *window, VulkanScene *scene)
    : WindowPixmap(window),
      m_scene(scene)
{
}


VulkanWindowPixmap::VulkanWindowPixmap(const QPointer<KWayland::Server::SubSurfaceInterface> &subSurface,
                                       WindowPixmap *parent,
                                       VulkanScene *scene)
    : WindowPixmap(subSurface, parent),
      m_scene(scene)
{
}


VulkanWindowPixmap::~VulkanWindowPixmap()
{
}


void VulkanWindowPixmap::create()
{
}


void VulkanWindowPixmap::aboutToRender()
{
}


WindowPixmap *VulkanWindowPixmap::createChild(const QPointer<KWayland::Server::SubSurfaceInterface> &subSurface)
{
    return new VulkanWindowPixmap(subSurface, this, m_scene);
}


bool VulkanWindowPixmap::isValid() const
{
    return WindowPixmap::isValid();
}


} // namespace KWin
