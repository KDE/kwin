/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "vulkan_shader.h"
#include "vulkan_logging.h"

#include <QFile>

namespace KWin
{

VulkanShader::VulkanShader(vk::raii::ShaderEXT &&shader,
                           std::vector<vk::raii::DescriptorSetLayout> &&descriptorSetLayouts,
                           vk::raii::PipelineLayout &&pipelineLayout)
    : m_shader(std::move(shader))
    , m_descriptorSetLayouts(std::move(descriptorSetLayouts))
    , m_pipelineLayout(std::move(pipelineLayout))
{
}

VulkanShader::~VulkanShader()
{
}

const vk::raii::ShaderEXT &VulkanShader::handle() const
{
    return m_shader;
}

const std::vector<vk::raii::DescriptorSetLayout> &VulkanShader::descriptorSetLayouts() const
{
    return m_descriptorSetLayouts;
}

const vk::raii::PipelineLayout &VulkanShader::pipelineLayout() const
{
    return m_pipelineLayout;
}

std::unique_ptr<VulkanShader> VulkanShader::compile(VulkanDevice *device, QByteArrayView spirV, std::span<const vk::DescriptorSetLayoutBinding> descriptorSetLayoutBindings)
{
    std::vector<vk::raii::DescriptorSetLayout> descriptorSetLayouts;
    std::vector<vk::DescriptorSetLayout> layouts;
    for (const auto &binding : descriptorSetLayoutBindings) {
        vk::DescriptorSetLayoutCreateInfo info{vk::DescriptorSetLayoutCreateFlags{}, binding};
        auto [result, layout] = device->logicalDevice().createDescriptorSetLayout(info);
        if (result != vk::Result::eSuccess) {
            return nullptr;
        }
        layouts.push_back(*layout);
        descriptorSetLayouts.push_back(std::move(layout));
    }

    auto [layoutResult, pipelineLayout] = device->logicalDevice().createPipelineLayout(vk::PipelineLayoutCreateInfo{
        vk::PipelineLayoutCreateFlags{},
        layouts,
    });
    if (layoutResult != vk::Result::eSuccess) {
        return nullptr;
    }

    auto [result, shader] = device->logicalDevice().createShaderEXT(vk::ShaderCreateInfoEXT{
        vk::ShaderCreateFlagsEXT{},
        vk::ShaderStageFlagBits::eCompute,
        vk::ShaderStageFlags{},
        vk::ShaderCodeTypeEXT::eSpirv,
        size_t(spirV.size()),
        spirV.data(),
        "main",
        uint32_t(layouts.size()),
        layouts.data(),
    });
    if (result != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN, "Compiling Vulkan shader failed: %s", vk::to_string(result).c_str());
        return nullptr;
    }
    auto cmd = device->computeQueue()->createCommandBuffer();
    cmd.begin(vk::CommandBufferBeginInfo{
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });
    cmd.bindShadersEXT({vk::ShaderStageFlagBits::eCompute}, {*shader});
    cmd.end();
    if (!device->computeQueue()->submitBlocking(std::move(cmd))) {
        qCWarning(KWIN_VULKAN, "Compiling Vulkan shader failed!");
        return nullptr;
    }
    return std::make_unique<VulkanShader>(std::move(shader), std::move(descriptorSetLayouts), std::move(pipelineLayout));
}

std::unique_ptr<VulkanShader> VulkanShader::compileFromPath(VulkanDevice *device, const QString &path, std::span<const vk::DescriptorSetLayoutBinding> descriptorSetLayouts)
{
    QFile file(path);
    if (!file.open(QFile::OpenModeFlag::ReadOnly)) {
        qCWarning(KWIN_VULKAN, "Couldn't open shader file %s", qPrintable(path));
        return nullptr;
    }
    return compile(device, file.readAll(), descriptorSetLayouts);
}

}
