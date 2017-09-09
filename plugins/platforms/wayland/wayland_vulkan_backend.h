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

#ifndef VULKAN_WAYLAND_BACKEND_H
#define VULKAN_WAYLAND_BACKEND_H

#include "platformsupport/scenes/vulkan/backend.h"

#include <memory>

namespace KWin
{

namespace Wayland {
    class WaylandBackend;
}

class VulkanSurface;

/**
 * Vulkan backend
 **/
class WaylandVulkanBackend : public VulkanBackend
{
public:
    explicit WaylandVulkanBackend(Wayland::WaylandBackend *platform);
    ~WaylandVulkanBackend() override;

    bool usesOverlayWindow() const override;
    void screenGeometryChanged(const QSize &size) override;
    QByteArray platformSurfaceExtensionName() const override;
    bool getPhysicalDevicePresentationSupport(VulkanPhysicalDevice device,
                                              uint32_t queueFamilyIndex) const override;
    bool createSurfaces() override;
    std::shared_ptr<VulkanSurface> surface() const override;
    bool isValid() const override;

private:
    Wayland::WaylandBackend *m_platform;
    std::shared_ptr<VulkanSurface> m_surface;
};

} // namespace KWin

#endif // X11VULKANBACKEND_H
