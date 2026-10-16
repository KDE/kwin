/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"

#include <vulkan/vulkan_raii.hpp>

namespace KWin
{

class VulkanDevice;
class VulkanTexture;

class KWIN_EXPORT VulkanDescriptor
{
public:
    explicit VulkanDescriptor(vk::raii::DescriptorPool &&pool, vk::raii::DescriptorSet &&set, vk::DescriptorType type);
    VulkanDescriptor(VulkanDescriptor &&move) = default;

    const vk::raii::DescriptorSet &set() const;

    void write(VulkanTexture *texture);

    static std::optional<VulkanDescriptor> allocate(VulkanDevice *device, const vk::raii::DescriptorSetLayout &layout,
                                                    vk::DescriptorType type, uint32_t descriptorCount,
                                                    vk::ShaderStageFlags shaderStages = vk::ShaderStageFlagBits::eAll);

private:
    vk::raii::DescriptorPool m_pool;
    vk::raii::DescriptorSet m_set;
    vk::DescriptorType m_type;
};

}
