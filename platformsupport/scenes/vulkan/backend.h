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

#ifndef BACKEND_H
#define BACKEND_H

#include <memory>

class QByteArray;
class QSize;

namespace KWin
{

class OverlayWindow;
class VulkanInstance;
class VulkanSurface;
class VulkanPhysicalDevice;

class VulkanBackend
{
public:
    /**
     * Destroys the Vulkan backend
     */
    virtual ~VulkanBackend();

    /**
     * Returns a pointer to the overlay window, or nullptr if the backend does
     * not use an overlay window.
     */
    virtual OverlayWindow *overlayWindow();

    /**
     * Returns true if the backend uses an overlay window, and false otherwise.
     */
    virtual bool usesOverlayWindow() const = 0;

    /**
     * Shows the overlay window.
     */
    virtual void showOverlay();

    /**
     * Returns true if each screen has a separate surface, and false otherwise.
     */
    virtual bool perScreenRendering() const;

    /**
     * Notifies the backend that the screen has been resized.
     */
    virtual void screenGeometryChanged(const QSize &size);

    /**
     * Sets the Vulkan instance that will be used to create Vulkan surfaces.
     */
    void setInstance(VulkanInstance *instance) { m_instance = instance; }

    /**
     * Returns the name of the platform surface extension for the backend, i.e.
     * VK_KHR_xcb_surface, VK_KHR_wayland_surface etc.
     */
    virtual QByteArray platformSurfaceExtensionName() const = 0;

    /**
     * Returns true if the given queue family in the given physical device supports
     * presenting images to the surface, and false otherwise.
     */
    virtual bool getPhysicalDevicePresentationSupport(VulkanPhysicalDevice physicalDevice,
                                                      uint32_t queueFamilyIndex) const = 0;

    /**
     * Creates the Vulkan surface(s).
     *
     * Returns true if successful, and false otherwise.
     */
    virtual bool createSurfaces() = 0;

    /**
     * Returns a handle to the Vulkan surface.
     */
    virtual std::shared_ptr<VulkanSurface> surface() const = 0;

    /**
     * Returns true if the backend is valid, and false otherwise.
     */
    virtual bool isValid() const;

protected:
    /**
     * Creates the Vulkan backend.
     */
    VulkanBackend();

protected:
    VulkanInstance *m_instance = nullptr;
};

} // namespace KWin

#endif // BACKEND_H
