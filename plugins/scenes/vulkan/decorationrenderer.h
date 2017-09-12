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

#ifndef DECORATIONRENDERER_H
#define DECORATIONRENDERER_H

#include "scene.h"

namespace KWin
{

class VulkanScene;

class VulkanDecorationRenderer : public Decoration::Renderer
{
    Q_OBJECT

public:
    enum class DecorationPart : int {
        Left = 0,
        Top,
        Right,
        Bottom,
        Count
    };

    explicit VulkanDecorationRenderer(Decoration::DecoratedClientImpl *client, VulkanScene *scene);
    ~VulkanDecorationRenderer() override;

    VulkanScene *scene() const { return m_scene; }

    const std::shared_ptr<VulkanImage> &image() const { return m_image; }
    const std::shared_ptr<VulkanImageView> &imageView() const { return m_imageView; }
    const std::shared_ptr<VulkanDeviceMemory> &memory() const { return m_memory; }

    void render() override;
    void reparent(Deleted *deleted) override;
    void resizeAtlas();

private:
    VulkanScene *m_scene;
    std::shared_ptr<VulkanDeviceMemory> m_memory;
    std::shared_ptr<VulkanImage> m_image;
    std::shared_ptr<VulkanImageView> m_imageView;
    std::shared_ptr<VulkanFramebuffer> m_framebuffer;
};

} // namespace KWin

#endif // DECORATIONRENDERER_H
