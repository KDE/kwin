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

#ifndef X11_VULKAN_BACKEND_H
#define X11_VULKAN_BACKEND_H

#include <memory>
#include <vector>

#include "platformsupport/scenes/vulkan/backend.h"

namespace KWin
{

class X11WindowedBackend;
class VulkanSurface;

/**
 * Vulkan backend
 **/
class X11VulkanBackend : public VulkanBackend
{
public:
    explicit X11VulkanBackend(X11WindowedBackend *platform);
    ~X11VulkanBackend() override;

    bool usesOverlayWindow() const override;
    void screenGeometryChanged(const QSize &size) override;
    QByteArray platformSurfaceExtensionName() const override;
    bool getPhysicalDevicePresentationSupport(VulkanPhysicalDevice device,
                                              uint32_t queueFamilyIndex) const override;
    bool createSurfaces() override;
    std::shared_ptr<VulkanSurface> surface() const override;
    bool isValid() const override;

private:
    X11WindowedBackend *m_platform;
    std::vector<std::shared_ptr<VulkanSurface>> m_surfaces;
};

} // namespace KWin

#endif // X11_VULKAN_BACKEND_H
