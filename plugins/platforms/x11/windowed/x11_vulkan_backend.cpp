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

#include "x11_vulkan_backend.h"

#include "x11windowed_backend.h"
#include "kwinvulkanutils.h"

#include "screens.h"

namespace KWin
{

X11VulkanBackend::X11VulkanBackend(X11WindowedBackend *platform)
    : VulkanBackend(),
      m_platform(platform)
{
}


X11VulkanBackend::~X11VulkanBackend()
{
}


bool X11VulkanBackend::usesOverlayWindow() const
{
    return false;
}


QByteArray X11VulkanBackend::platformSurfaceExtensionName() const
{
    return QByteArrayLiteral(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
}


bool X11VulkanBackend::createSurfaces()
{
    m_surfaces.reserve(screens()->count());

    for (int i = 0; i < screens()->count(); ++i) {
        auto surface = std::make_shared<VulkanSurface>(m_instance, m_platform->connection(), m_platform->windowForScreen(i));

        if (!surface->isValid())
            surface = nullptr;

        m_surfaces.emplace_back(surface);
    }

    if (m_surfaces.empty())
        return false;

    return true;
}


std::shared_ptr<VulkanSurface> X11VulkanBackend::surface() const
{
    if (m_surfaces.empty())
        return nullptr;

    return m_surfaces.front();
}


bool X11VulkanBackend::getPhysicalDevicePresentationSupport(VulkanPhysicalDevice device, uint32_t queueFamilyIndex) const
{
    return device.getXcbPresentationSupportKHR(queueFamilyIndex, m_platform->connection(), m_platform->visual());
}


void X11VulkanBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)
}


bool X11VulkanBackend::isValid() const
{
    return true;
}


} // namespace KWin
