/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"
#include "vulkan_include.h"

#include <QHash>
#include <QObject>
#include <QVector>
#include <memory>
#include <optional>

namespace KWin
{

class VulkanTexture;
class GraphicsBuffer;

class KWIN_EXPORT VulkanDevice : public QObject
{
    Q_OBJECT
public:
    explicit VulkanDevice(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device logicalDevice, uint32_t queueFamilyIndex, vk::Queue queue,
                          std::optional<dev_t> primaryNode, std::optional<dev_t> renderNode);
    explicit VulkanDevice(VulkanDevice &&other);
    VulkanDevice(const VulkanDevice &) = delete;
    ~VulkanDevice();

    std::shared_ptr<VulkanTexture> importClientBuffer(GraphicsBuffer *buffer);
    std::optional<VulkanTexture> importDmabuf(GraphicsBuffer *buffer) const;

    QHash<uint32_t, QVector<uint64_t>> supportedFormats() const;
    vk::Device logicalDevice() const;
    uint32_t queueFamilyIndex() const;
    vk::Queue renderQueue() const;

    std::optional<dev_t> primaryNode() const;
    std::optional<dev_t> renderNode() const;

    vk::UniqueCommandBuffer allocateCommandBuffer();
    vk::UniqueDeviceMemory allocateMemory(vk::Buffer buffer, vk::MemoryPropertyFlags memoryProperties) const;
    vk::UniqueDeviceMemory allocateMemory(vk::Image image, vk::MemoryPropertyFlags memoryProperties) const;
    bool submitCommandBufferBlocking(vk::CommandBuffer cmd);

private:
    QHash<uint32_t, QVector<uint64_t>> queryFormats(vk::ImageUsageFlags flags) const;
    std::optional<uint32_t> findMemoryType(uint32_t typeBits, vk::MemoryPropertyFlags memoryPropertyFlags) const;

    vk::PhysicalDevice m_physical;
    vk::Device m_logical;
    uint32_t m_queueFamilyIndex;
    vk::Queue m_queue;
    std::optional<dev_t> m_primaryNode;
    std::optional<dev_t> m_renderNode;
    QHash<uint32_t, QVector<uint64_t>> m_formats;
    vk::PhysicalDeviceMemoryProperties m_memoryProperties;
    vk::UniqueCommandPool m_commandPool;

    vk::DispatchLoaderDynamic m_loader;
    QHash<GraphicsBuffer *, std::shared_ptr<VulkanTexture>> m_importedTextures;
};

}
