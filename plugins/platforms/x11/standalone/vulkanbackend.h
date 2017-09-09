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

#include <xcb/xproto.h>
#include "platformsupport/scenes/vulkan/backend.h"


namespace KWin
{

class X11StandalonePlatform;
class OverlayWindow;

/**
 * Vulkan backend
 **/
class X11VulkanBackend : public VulkanBackend
{
public:
    explicit X11VulkanBackend(X11StandalonePlatform *platform);
    ~X11VulkanBackend() override;

    OverlayWindow* overlayWindow() override;
    bool usesOverlayWindow() const override;
    void showOverlay() override;
    void screenGeometryChanged(const QSize &size) override;
    QByteArray platformSurfaceExtensionName() const override;
    bool getPhysicalDevicePresentationSupport(VulkanPhysicalDevice device,
                                              uint32_t queueFamilyIndex) const override;
    bool createSurfaces() override;
    std::shared_ptr<VulkanSurface> surface() const override;
    bool isValid() const override;

private:
    X11StandalonePlatform *m_platform;
    OverlayWindow *m_overlayWindow;
    std::shared_ptr<VulkanSurface> m_surface;
    xcb_window_t m_window;
};

} // namespace KWin

#endif // X11_VULKAN_BACKEND_H
