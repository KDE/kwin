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

#ifndef VULKAN_SHADOW_H
#define VULKAN_SHADOW_H

#include "scene.h"
#include "../../../shadow.h"


namespace KWin
{

class VulkanShadow : public Shadow
{
public:
    explicit VulkanShadow(Toplevel *toplevel, VulkanScene *scene);
    ~VulkanShadow() override;

    VulkanScene *scene() const { return m_scene; }

    const std::shared_ptr<VulkanImage> &image() const { return m_image; }
    const std::shared_ptr<VulkanImageView> &imageView() const { return m_imageView; }
    const std::shared_ptr<VulkanDeviceMemory> &memory() const { return m_memory; }

protected:
    void buildQuads() override final;
    bool prepareBackend() override final;

private:
    VulkanScene *m_scene;
    std::shared_ptr<VulkanImage> m_image;
    std::shared_ptr<VulkanImageView> m_imageView;
    std::shared_ptr<VulkanDeviceMemory> m_memory;
};

} // namespace KWin

#endif // VULKAN_SHADOW_H
