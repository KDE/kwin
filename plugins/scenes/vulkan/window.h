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

#ifndef VULKAN_WINDOW_H
#define VULKAN_WINDOW_H

#include "scene.h"

namespace KWin
{

class TextureDescriptorSet;
class CrossFadeDescriptorSet;

class VulkanWindow : public Scene::Window
{
    /**
     * Texture is a tuple of an image, an image-view, a device memory object and an image layout.
     */
    class Texture
    {
    public:
        Texture() = default;

        Texture(const std::shared_ptr<VulkanImage> &image,
                const std::shared_ptr<VulkanImageView> &imageView,
                const std::shared_ptr<VulkanDeviceMemory> &memory,
                VkImageLayout imageLayout)
            : m_image(image),
              m_imageView(imageView),
              m_memory(memory),
              m_imageLayout(imageLayout)
        {
        }

        operator bool () const { return m_image && m_imageView && m_memory; }

        std::shared_ptr<VulkanImage> image() const { return m_image; }
        std::shared_ptr<VulkanImageView> imageView() const { return m_imageView; }
        std::shared_ptr<VulkanDeviceMemory> memory() const { return m_memory; }

        VkImageLayout imageLayout() const { return m_imageLayout; }

    private:
        std::shared_ptr<VulkanImage> m_image;
        std::shared_ptr<VulkanImageView> m_imageView;
        std::shared_ptr<VulkanDeviceMemory> m_memory;
        VkImageLayout m_imageLayout;
    };

public:
    explicit VulkanWindow(Toplevel *toplevel, VulkanScene *scene);
    ~VulkanWindow() override;

    void performPaint(int mask, QRegion region, WindowPaintData data) override;

    VulkanScene *scene() const { return m_scene; }

protected:
    virtual WindowPixmap *createWindowPixmap();

    VulkanPipelineManager *pipelineManager() { return scene()->pipelineManager(); }
    VulkanUploadManager *uploadManager() { return scene()->uploadManager(); }

    QMatrix4x4 windowMatrix(int mask, const WindowPaintData &data) const;
    QMatrix4x4 modelViewProjectionMatrix(int mask, const WindowPaintData &data) const;

    Texture getContentTexture();
    Texture getPreviousContentTexture();
    Texture getDecorationTexture() const;

private:
    VulkanScene *m_scene;
    std::shared_ptr<TextureDescriptorSet> m_decorationDescriptorSet;
    std::shared_ptr<TextureDescriptorSet> m_contentDescriptorSet;
    std::shared_ptr<CrossFadeDescriptorSet> m_crossFadeDescriptorSet;
};

} // namespace KWin

#endif // VULKAN_WINDOW_H
