/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "vulkan/vulkan_device.h"

namespace KWin
{

class VulkanShader
{
public:
    struct Descriptor
    {
        vk::raii::DescriptorSetLayout layout;
        vk::raii::DescriptorPool pool;
        vk::raii::DescriptorSet set;
    };

    explicit VulkanShader(VulkanDevice *device, vk::raii::ShaderEXT &&shader,
                          Descriptor &&image, Descriptor &&buffer, Descriptor &&texturesDescriptor,
                          vk::raii::PipelineLayout &&pipelineLayout);
    ~VulkanShader();

    const vk::raii::ShaderEXT &handle() const;
    const vk::raii::DescriptorSet &imageSet() const;
    const vk::raii::DescriptorSet &bufferSet() const;
    const vk::raii::DescriptorSet &textureSet() const;
    const vk::raii::PipelineLayout &pipelineLayout() const;

    static std::unique_ptr<VulkanShader> compile(VulkanDevice *device, QByteArrayView spirV);
    static std::unique_ptr<VulkanShader> compileFromPath(VulkanDevice *device, const QString &path);

private:
    VulkanDevice *const m_device;
    vk::raii::ShaderEXT m_shader;
    Descriptor m_imageDescriptor;
    Descriptor m_bufferDescriptor;
    Descriptor m_texturesDescriptor;
    vk::raii::PipelineLayout m_pipelineLayout;
};

}
