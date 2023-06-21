/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
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

class VulkanDevice : public QObject
{
    Q_OBJECT
public:
    VulkanDevice(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device logicalDevice, uint32_t queueFamilyIndex, vk::Queue queue,
                 std::optional<dev_t> primaryNode, std::optional<dev_t> renderNode);
    VulkanDevice(const VulkanDevice &) = delete;
    VulkanDevice(VulkanDevice &&other);
    ~VulkanDevice();

    std::shared_ptr<VulkanTexture> importClientBuffer(GraphicsBuffer *buffer);
    std::optional<VulkanTexture> importDmabuf(GraphicsBuffer *buffer) const;

    QHash<uint32_t, QVector<uint64_t>> supportedFormats() const;
    vk::Device logicalDevice() const;
    uint32_t queueFamilyIndex() const;

    std::optional<dev_t> primaryNode() const;
    std::optional<dev_t> renderNode() const;

private:
    QHash<uint32_t, QVector<uint64_t>> queryFormats(vk::ImageUsageFlags flags) const;
    std::optional<int> findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeBits, vk::MemoryPropertyFlags memoryPropertyFlags) const;

    vk::PhysicalDevice m_physical;
    vk::Device m_logical;
    uint32_t m_queueFamilyIndex;
    vk::Queue m_queue;
    std::optional<dev_t> m_primaryNode;
    std::optional<dev_t> m_renderNode;
    QHash<uint32_t, QVector<uint64_t>> m_formats;

    vk::DispatchLoaderDynamic m_loader;
    QHash<GraphicsBuffer *, std::shared_ptr<VulkanTexture>> m_importedTextures;
};

}
