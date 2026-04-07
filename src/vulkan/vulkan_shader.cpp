/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "vulkan_shader.h"

#include <QFile>

namespace KWin
{

VulkanShader::VulkanShader(vk::raii::ShaderEXT &&shader)
    : m_shader(std::move(shader))
{
}

VulkanShader::~VulkanShader()
{
}

const vk::raii::ShaderEXT &VulkanShader::handle() const
{
    return m_shader;
}

std::unique_ptr<VulkanShader> VulkanShader::compile(VulkanDevice *device, QByteArrayView spirV)
{
    std::span<vk::DescriptorSetLayout> descriptorLayouts;
    auto [result, shader] = device->logicalDevice().createShaderEXT(vk::ShaderCreateInfoEXT{
        vk::ShaderCreateFlagsEXT{},
        vk::ShaderStageFlagBits::eCompute,
        vk::ShaderStageFlags{},
        vk::ShaderCodeTypeEXT::eSpirv,
        size_t(spirV.size()),
        spirV.data(),
        "main",
        uint32_t(descriptorLayouts.size()),
        descriptorLayouts.data(),
    });
    if (result != vk::Result::eSuccess) {
        return nullptr;
    }
    auto cmd = device->createCommandBuffer();
    cmd.begin(vk::CommandBufferBeginInfo{
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });
    cmd.bindShadersEXT({vk::ShaderStageFlagBits::eCompute}, {*shader});
    cmd.end();
    // NOTE this is blocking with the assumption that shader compilation
    // is a one-time setup task. If this changes, fix this.
    if (!device->submitBlocking(std::move(cmd))) {
        return nullptr;
    }
    return std::make_unique<VulkanShader>(std::move(shader));
}

std::unique_ptr<VulkanShader> VulkanShader::compileFromPath(VulkanDevice *device, const QString &path)
{
    QFile file(path);
    if (!file.open(QFile::OpenModeFlag::ReadOnly)) {
        return nullptr;
    }
    return compile(device, file.readAll());
}

}
