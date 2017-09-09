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

#include "vulkanbackend.h"
#include "kwinvulkanutils.h"

#include "x11_platform.h"
#include "overlaywindow_x11.h"
#include "logging.h"
#include "screens.h"

namespace KWin
{


X11VulkanBackend::X11VulkanBackend(X11StandalonePlatform *platform)
    : VulkanBackend(),
      m_platform(platform),
      m_overlayWindow(platform->createOverlayWindow())
{
}


X11VulkanBackend::~X11VulkanBackend()
{
    if (m_window)
        xcb_destroy_window(connection(), m_window);

    overlayWindow()->destroy();
    delete m_overlayWindow;
}


OverlayWindow *X11VulkanBackend::overlayWindow()
{
    return m_overlayWindow;
}


bool X11VulkanBackend::usesOverlayWindow() const
{
    return true;
}


void X11VulkanBackend::showOverlay()
{
    if (m_overlayWindow && m_overlayWindow->window())
        m_overlayWindow->show();
}


QByteArray X11VulkanBackend::platformSurfaceExtensionName() const
{
    return QByteArrayLiteral(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
}


bool X11VulkanBackend::createSurfaces()
{
    if (!overlayWindow()->create()) {
        qCCritical(KWIN_X11STANDALONE) << "Failed to create overlay window";
        return false;
    }

    xcb_connection_t * const c = connection();
    const QSize size = screens()->size();

    m_window = xcb_generate_id(c);
    xcb_create_window(c, XCB_COPY_FROM_PARENT, m_window, overlayWindow()->window(),
                      0, 0, size.width(), size.height(), 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      XCB_COPY_FROM_PARENT, 0, nullptr);

    m_surface = std::make_shared<VulkanSurface>(m_instance, c, m_window);
    if (!m_surface->isValid()) {
        qCCritical(KWIN_X11STANDALONE) << "Failed to create Vulkan surface";
        return false;
    }

    overlayWindow()->setup(m_window);
    return true;
}


std::shared_ptr<VulkanSurface> X11VulkanBackend::surface() const
{
    return m_surface;
}


bool X11VulkanBackend::getPhysicalDevicePresentationSupport(VulkanPhysicalDevice device, uint32_t queueFamilyIndex) const
{
    return device.getXcbPresentationSupportKHR(queueFamilyIndex, connection(), defaultScreen()->root_visual);
}


void X11VulkanBackend::screenGeometryChanged(const QSize &size)
{
    // Resize the X window
    const uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    const uint32_t values[] = { 0, 0, (uint32_t) size.width(), (uint32_t) size.height() };

    xcb_configure_window(connection(), m_window, mask, values);
    overlayWindow()->setup(m_window);
}


bool X11VulkanBackend::isValid() const
{
    return true;
}

} // namespace KWin
