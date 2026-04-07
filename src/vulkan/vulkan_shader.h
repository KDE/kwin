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

class KWIN_EXPORT VulkanShader
{
public:
    explicit VulkanShader(vk::raii::ShaderEXT &&shader,
                          std::vector<vk::raii::DescriptorSetLayout> &&descriptorSetLayouts,
                          vk::raii::PipelineLayout &&pipelineLayout);
    ~VulkanShader();

    const vk::raii::ShaderEXT &handle() const;
    const std::vector<vk::raii::DescriptorSetLayout> &descriptorSetLayouts() const;
    const vk::raii::PipelineLayout &pipelineLayout() const;

    static std::unique_ptr<VulkanShader> compile(VulkanDevice *device, QByteArrayView spirV, std::span<const vk::DescriptorSetLayoutBinding> descriptorSetLayouts);
    static std::unique_ptr<VulkanShader> compileFromPath(VulkanDevice *device, const QString &path, std::span<const vk::DescriptorSetLayoutBinding> descriptorSetLayouts);

private:
    vk::raii::ShaderEXT m_shader;
    std::vector<vk::raii::DescriptorSetLayout> m_descriptorSetLayouts;
    vk::raii::PipelineLayout m_pipelineLayout;
};

}
