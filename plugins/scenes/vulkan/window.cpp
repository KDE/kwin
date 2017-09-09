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

#include "window.h"
#include "windowpixmap.h"


namespace KWin
{


VulkanWindow::VulkanWindow(Toplevel *toplevel, VulkanScene *scene)
    : Scene::Window(toplevel),
      m_scene(scene)
{
}


VulkanWindow::~VulkanWindow()
{
}


void VulkanWindow::performPaint(int mask, QRegion clipRegion, WindowPaintData data)
{
    Q_UNUSED(mask)
    Q_UNUSED(clipRegion)
    Q_UNUSED(data)
}


WindowPixmap *VulkanWindow::createWindowPixmap()
{
    return new VulkanWindowPixmap(this, m_scene);
}


} // namespace KWin
