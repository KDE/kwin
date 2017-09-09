/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Fredrik Höglund <fredrik@kde.org>

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

#include "wayland_vulkan_backend.h"
#include "wayland_backend.h"

#include <KWayland/Client/surface.h>

#include "kwinvulkanutils.h"

namespace KWin
{


WaylandVulkanBackend::WaylandVulkanBackend(Wayland::WaylandBackend *platform)
    : VulkanBackend(),
      m_platform(platform)
{
}


WaylandVulkanBackend::~WaylandVulkanBackend()
{
}


bool WaylandVulkanBackend::usesOverlayWindow() const
{
    return false;
}


QByteArray WaylandVulkanBackend::platformSurfaceExtensionName() const
{
    return QByteArrayLiteral(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
}


bool WaylandVulkanBackend::createSurfaces()
{
    m_surface = std::make_shared<VulkanSurface>(m_instance, m_platform->display(), *m_platform->surface());
    return m_surface->isValid();
}


std::shared_ptr<VulkanSurface> WaylandVulkanBackend::surface() const
{
    return m_surface;
}


bool WaylandVulkanBackend::getPhysicalDevicePresentationSupport(VulkanPhysicalDevice device, uint32_t queueFamilyIndex) const
{
    return device.getWaylandPresentationSupportKHR(queueFamilyIndex, m_platform->display());
}


void WaylandVulkanBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)
}


bool WaylandVulkanBackend::isValid() const
{
    return true;
}


} // namespace KWin
