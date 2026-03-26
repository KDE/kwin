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

VulkanShader::VulkanShader(VulkanDevice *device, vk::raii::ShaderEXT &&shader,
                           Descriptor &&image, Descriptor &&buffer, Descriptor &&texturesDescriptor,
                           vk::raii::PipelineLayout &&pipelineLayout)
    : m_device(device)
    , m_shader(std::move(shader))
    , m_imageDescriptor(std::move(image))
    , m_bufferDescriptor(std::move(buffer))
    , m_texturesDescriptor(std::move(texturesDescriptor))
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

const vk::raii::DescriptorSet &VulkanShader::imageSet() const
{
    return m_imageDescriptor.set;
}

const vk::raii::DescriptorSet &VulkanShader::bufferSet() const
{
    return m_bufferDescriptor.set;
}

const vk::raii::DescriptorSet &VulkanShader::textureSet() const
{
    return m_texturesDescriptor.set;
}

const vk::raii::PipelineLayout &VulkanShader::pipelineLayout() const
{
    return m_pipelineLayout;
}

std::unique_ptr<VulkanShader> VulkanShader::compile(VulkanDevice *device, QByteArrayView spirV)
{
    // TODO the layouts are hardcoded for the composite shader
    vk::DescriptorSetLayoutBinding imageLayoutBinding(0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute);
    vk::DescriptorSetLayoutCreateInfo imageLayoutInfo{vk::DescriptorSetLayoutCreateFlags{}, imageLayoutBinding};
    auto [imageResult, imageLayout] = device->logicalDevice().createDescriptorSetLayout(imageLayoutInfo);
    if (imageResult != vk::Result::eSuccess) {
        return nullptr;
    }
    vk::DescriptorSetLayoutBinding bufferLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute);
    vk::DescriptorSetLayoutCreateInfo bufferLayoutInfo{vk::DescriptorSetLayoutCreateFlags{}, bufferLayoutBinding};
    auto [bufferResult, bufferLayout] = device->logicalDevice().createDescriptorSetLayout(bufferLayoutInfo);
    if (bufferResult != vk::Result::eSuccess) {
        return nullptr;
    }
    vk::DescriptorSetLayoutBinding textureLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1000, vk::ShaderStageFlagBits::eCompute);
    vk::DescriptorSetLayoutCreateInfo textureLayoutInfo{vk::DescriptorSetLayoutCreateFlags{}, textureLayoutBinding};
    auto [textureResult, textureLayout] = device->logicalDevice().createDescriptorSetLayout(textureLayoutInfo);
    if (textureResult != vk::Result::eSuccess) {
        return nullptr;
    }

    std::vector<vk::DescriptorSetLayout> descriptorLayouts{
        *imageLayout,
        *bufferLayout,
        *textureLayout,
    };
    vk::PushConstantRange pushConstants{
        vk::ShaderStageFlagBits::eCompute,
        0,
        sizeof(uint32_t),
    };
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
        1,
        &pushConstants,
    });
    if (result != vk::Result::eSuccess) {
        return nullptr;
    }

    vk::DescriptorPoolSize imagePoolSize(vk::DescriptorType::eStorageImage, 1);
    vk::DescriptorPoolCreateInfo imagePoolInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1, imagePoolSize);
    auto [imagePoolResult, imagePool] = device->logicalDevice().createDescriptorPool(imagePoolInfo);
    if (imagePoolResult != vk::Result::eSuccess) {
        return nullptr;
    }
    auto [imageSetResult, imageSet] = device->logicalDevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
        *imagePool,
        *imageLayout,
    });
    if (imageSetResult != vk::Result::eSuccess) {
        return nullptr;
    }

    vk::DescriptorPoolSize bufferPoolSize(vk::DescriptorType::eStorageBuffer, 1);
    vk::DescriptorPoolCreateInfo bufferPoolInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1, bufferPoolSize);
    auto [bufferPoolResult, bufferPool] = device->logicalDevice().createDescriptorPool(bufferPoolInfo);
    if (bufferPoolResult != vk::Result::eSuccess) {
        return nullptr;
    }
    auto [bufferSetResult, bufferSet] = device->logicalDevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
        *bufferPool,
        *bufferLayout,
    });
    if (bufferSetResult != vk::Result::eSuccess) {
        return nullptr;
    }

    vk::DescriptorPoolSize texturePoolSize(vk::DescriptorType::eCombinedImageSampler, 1000);
    vk::DescriptorPoolCreateInfo texturePoolInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1, texturePoolSize);
    auto [texturePoolResult, texturePool] = device->logicalDevice().createDescriptorPool(texturePoolInfo);
    if (bufferPoolResult != vk::Result::eSuccess) {
        return nullptr;
    }
    auto [textureSetResult, textureSet] = device->logicalDevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
        *texturePool,
        *textureLayout,
    });
    if (textureSetResult != vk::Result::eSuccess) {
        return nullptr;
    }

    std::vector<vk::DescriptorSetLayout> layouts{
        *imageLayout,
        *bufferLayout,
        *textureLayout,
    };
    auto [layoutResult, layout] = device->logicalDevice().createPipelineLayout(vk::PipelineLayoutCreateInfo{
        vk::PipelineLayoutCreateFlags{},
        layouts,
        pushConstants,
    });
    if (layoutResult != vk::Result::eSuccess) {
        return nullptr;
    }

    return std::make_unique<VulkanShader>(device, std::move(shader),
                                          Descriptor{std::move(imageLayout), std::move(imagePool), std::move(imageSet.front())},
                                          Descriptor{std::move(bufferLayout), std::move(bufferPool), std::move(bufferSet.front())},
                                          Descriptor{std::move(textureLayout), std::move(texturePool), std::move(textureSet.front())},
                                          std::move(layout));
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
