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

#ifndef WINDOWPIXMAP_H
#define WINDOWPIXMAP_H

#include "../../../scene.h"
#include "kwinvulkanutils_funcs.h"

#include <QPointer>
#include <memory>

namespace KWayland {
    namespace Server {
        class SubSurfaceInterface;
    }
}

struct wl_shm_buffer;

namespace KWin
{

class VulkanScene;
class VulkanImage;
class VulkanImageView;
class VulkanDeviceMemory;

class VulkanWindowPixmap : public WindowPixmap
{
public:
    explicit VulkanWindowPixmap(Scene::Window *window, VulkanScene *scene);

    explicit VulkanWindowPixmap(const QPointer<KWayland::Server::SubSurfaceInterface> &subSurface,
                                WindowPixmap *parent,
                                VulkanScene *scene);

    ~VulkanWindowPixmap() override final;

    void create() override final;

    VulkanScene *scene() const { return m_scene; }

    WindowPixmap *createChild(const QPointer<KWayland::Server::SubSurfaceInterface> &subSurface);

    bool isValid() const override final;

    void aboutToRender();

    const std::shared_ptr<VulkanImage> &image() const { return m_image; }
    const std::shared_ptr<VulkanImageView> &imageView() const { return m_imageView; }
    const std::shared_ptr<VulkanDeviceMemory> &memory() const { return m_memory; }

    VkImageLayout imageLayout() const { return m_imageLayout; }

protected:
    void createTexture(VkFormat format, uint32_t width, uint32_t height, const VkComponentMapping &swizzle);
    void updateTexture(wl_shm_buffer *buffer, const QRegion &damage, VkImageLayout layout);

private:
    VulkanScene *m_scene;
    std::shared_ptr<VulkanImage> m_image;
    std::shared_ptr<VulkanImageView> m_imageView;
    std::shared_ptr<VulkanDeviceMemory> m_memory;
    VkImageLayout m_imageLayout;
    uint32_t m_bufferFormat;
    uint32_t m_bitsPerPixel;
};

} // namespace KWin

#endif // WINDOWPIXMAP_H
