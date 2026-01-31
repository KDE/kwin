/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023-2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "vulkan_device.h"
#include "core/gpumanager.h"
#include "core/graphicsbuffer.h"
#include "vulkan_logging.h"
#include "vulkan_texture.h"

#include <QDebug>
#include <sys/stat.h>
#include <vulkan/vulkan.hpp>

namespace KWin
{

VulkanDevice::VulkanDevice(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice logicalDevice, std::vector<VkQueueFamilyProperties> &&queueProperties)
    : m_physical(physicalDevice)
    , m_logical(logicalDevice)
    // TODO it might be useful to have separate lists for sample + transfer_src
    // and sample + color attachment + transfer_dst
    , m_formats(queryFormats(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT))
    , m_queueProperties(std::move(queueProperties))
{
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &m_memoryProperties);
    createQueues();
}

VulkanDevice::VulkanDevice(VulkanDevice &&other)
    : m_physical(other.m_physical)
    , m_logical(std::exchange(other.m_logical, {}))
    , m_formats(std::move(other.m_formats))
    , m_transferQueue(other.m_transferQueue)
    , m_commandPool(std::exchange(other.m_commandPool, nullptr))
    , m_memoryProperties(other.m_memoryProperties)
    , m_importedTextures(other.m_importedTextures)
{
}

VulkanDevice::~VulkanDevice()
{
    vkQueueWaitIdle(m_transferQueue);
    m_importedTextures.clear();
    for (const auto &[buffer, fence] : m_submittedCommandBuffers) {
        vkFreeCommandBuffers(m_logical, m_commandPool, 1, &buffer);
        vkDestroyFence(m_logical, fence, nullptr);
    }
    if (m_commandPool) {
        vkDestroyCommandPool(m_logical, m_commandPool, nullptr);
    }
    if (m_logical) {
        vkDestroyDevice(m_logical, nullptr);
    }
}

void VulkanDevice::createQueues()
{
    // transfer-only queues usually do DMA, so prefer them
    auto it = std::ranges::find_if(m_queueProperties, [](const VkQueueFamilyProperties &props) {
        return props.queueFlags == VK_QUEUE_TRANSFER_BIT;
    });
    if (it == m_queueProperties.end()) {
        // fall back to anything else that can do transfer.
        // NOTE that compute and graphics queues are guaranteed to have that capability
        it = std::ranges::find_if(m_queueProperties, [](const VkQueueFamilyProperties &props) {
            return props.queueFlags & VK_QUEUE_TRANSFER_BIT;
        });
    }
    Q_ASSERT(it != m_queueProperties.end());
    m_queueFamilyIndex = std::distance(m_queueProperties.begin(), it);
    vkGetDeviceQueue(m_logical, m_queueFamilyIndex, 0, &m_transferQueue);

    // only one queue for now -> also only one command pool
    auto [result, cmdPool] = vk::Device(m_logical).createCommandPool(vk::CommandPoolCreateInfo{
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        m_queueFamilyIndex,
    });
    if (result != vk::Result::eSuccess) {
        qCCritical(KWIN_VULKAN) << "creating a command pool failed:" << vk::to_string(result);
        return;
    }
    m_commandPool = cmdPool;
}

std::shared_ptr<VulkanTexture> VulkanDevice::importBuffer(GraphicsBuffer *buffer, VkImageUsageFlags usage)
{
    if (!buffer->dmabufAttributes()) {
        return nullptr;
    }
    auto it = m_importedTextures.find(buffer);
    if (it != m_importedTextures.end()) {
        return it.value();
    }
    auto opt = importDmabuf(buffer->dmabufAttributes(), usage);
    if (!opt) {
        return nullptr;
    }
    const auto ret = std::make_shared<VulkanTexture>(std::move(*opt));
    m_importedTextures[buffer] = ret;
    connect(buffer, &QObject::destroyed, this, [this, buffer]() {
        m_importedTextures.remove(buffer);
    });
    return ret;
}

/**
 * A dmabuf can have multiple planes with fds pointing to the same image,
 * so checking the number of planes isn't enough to know if it's disjoint
 */
static bool isDisjoint(const DmaBufAttributes &attributes)
{
    if (attributes.planeCount == 1) {
        return false;
    }
    struct stat stat1;
    if (fstat(attributes.fd[0].get(), &stat1) != 0) {
        qCWarning(KWIN_VULKAN) << "failed to fstat dmabuf";
        return true;
    }
    for (int i = 1; i < attributes.planeCount; i++) {
        struct stat stati;
        if (fstat(attributes.fd[i].get(), &stati) != 0) {
            qCWarning(KWIN_VULKAN) << "failed to fstat dmabuf";
            return false;
        }
        if (stat1.st_ino != stati.st_ino) {
            return true;
        }
    }
    return false;
}

std::shared_ptr<VulkanTexture> VulkanDevice::importDmabuf(const DmaBufAttributes *attributes, VkImageUsageFlags usage)
{
    const auto format = FormatInfo::get(attributes->format);
    if (!format) {
        qCWarning(KWIN_VULKAN, "Dmabuf has unknown format");
        return nullptr;
    }
    auto formatIt = m_formats.find(attributes->format);
    if (formatIt == m_formats.end() || !formatIt->contains(attributes->modifier)) {
        if (formatIt == m_formats.end()) {
            qCWarning(KWIN_VULKAN, "Dmabuf has unsupported format %s", qPrintable(FormatInfo::drmFormatName(attributes->format)));
            for (auto it = m_formats.begin(); it != m_formats.end(); it++) {
                qCWarning(KWIN_VULKAN, "Supported fmt: %s", qPrintable(FormatInfo::drmFormatName(it.key())));
            }
        } else {
            qCWarning(KWIN_VULKAN, "Dmabuf has unsupported modifier for format %s", qPrintable(FormatInfo::drmFormatName(attributes->format)));
        }
        return nullptr;
    }
    std::vector<vk::SubresourceLayout> subLayouts;
    for (int i = 0; i < attributes->planeCount; i++) {
        subLayouts.emplace_back(attributes->offset[i], 0, attributes->pitch[i], 0, 0);
    }
    vk::ImageDrmFormatModifierExplicitCreateInfoEXT modifierCI{
        attributes->modifier,
        subLayouts,
    };
    vk::ExternalMemoryImageCreateInfo externalCI{
        vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT,
        &modifierCI,
    };
    const bool disjoint = isDisjoint(*attributes);
    vk::ImageCreateInfo imageCI{
        disjoint ? vk::ImageCreateFlagBits::eDisjoint : vk::ImageCreateFlags(),
        vk::ImageType::e2D,
        vk::Format(format->vulkanFormat),
        vk::Extent3D(attributes->width, attributes->height, 1),
        1,
        1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eDrmFormatModifierEXT,
        vk::ImageUsageFlags(usage),
        vk::SharingMode::eExclusive,
        m_queueFamilyIndex,
        vk::ImageLayout::eUndefined,
        &externalCI,
    };
    const vk::Device logical = vk::Device(m_logical);
    auto [imageResult, image] = logical.createImageUnique(imageCI);
    if (imageResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "creating vulkan image failed!" << vk::to_string(imageResult);
        return nullptr;
    }

    std::array<vk::BindImageMemoryInfo, 4> bindInfos;
    std::array<vk::BindImagePlaneMemoryInfo, 4> planeInfo;
    std::vector<vk::UniqueDeviceMemory> deviceMemory;
    const uint32_t memoryCount = disjoint ? attributes->planeCount : 1;

    std::array<FileDescriptor, 4> duplicatedFds;
    for (size_t i = 0; i < memoryCount; i++) {
        duplicatedFds[i] = attributes->fd[i].duplicate();
    }

    for (uint32_t i = 0; i < memoryCount; i++) {
        const auto [memoryFdResult, memoryFdProperties] = logical.getMemoryFdPropertiesKHR(vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT, duplicatedFds[i].get());
        if (memoryFdResult != vk::Result::eSuccess) {
            qCWarning(KWIN_VULKAN) << "failed to get memory fd properties!" << vk::to_string(memoryFdResult);
            return nullptr;
        }
        vk::ImageMemoryRequirementsInfo2 memRequirementsInfo{image.get()};
        vk::ImagePlaneMemoryRequirementsInfo planeRequirementsInfo;
        if (disjoint) {
            switch (i) {
            case 0:
                planeRequirementsInfo.setPlaneAspect(vk::ImageAspectFlagBits::eMemoryPlane0EXT);
                break;
            case 1:
                planeRequirementsInfo.setPlaneAspect(vk::ImageAspectFlagBits::eMemoryPlane1EXT);
                break;
            case 2:
                planeRequirementsInfo.setPlaneAspect(vk::ImageAspectFlagBits::eMemoryPlane2EXT);
                break;
            case 3:
                planeRequirementsInfo.setPlaneAspect(vk::ImageAspectFlagBits::eMemoryPlane3EXT);
                break;
            }
            memRequirementsInfo.setPNext(&planeRequirementsInfo);
        }
        const vk::MemoryRequirements2 memRequirements = logical.getImageMemoryRequirements2(memRequirementsInfo);
        const auto memoryIndex = findMemoryType(memRequirements.memoryRequirements.memoryTypeBits & memoryFdProperties.memoryTypeBits, {});
        if (!memoryIndex) {
            qCWarning(KWIN_VULKAN, "couldn't find a suitable memory type for %x & %x = %x",
                      memRequirements.memoryRequirements.memoryTypeBits, memoryFdProperties.memoryTypeBits,
                      memRequirements.memoryRequirements.memoryTypeBits & memoryFdProperties.memoryTypeBits);
            return nullptr;
        }

        vk::MemoryDedicatedAllocateInfo dedicatedInfo{image.get()};
        vk::ImportMemoryFdInfoKHR importInfo(vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT, duplicatedFds[i].get(), &dedicatedInfo);
        vk::MemoryAllocateInfo memoryAI(memRequirements.memoryRequirements.size, memoryIndex.value(), &importInfo);
        auto [allocateResult, memory] = logical.allocateMemoryUnique(memoryAI);
        if (allocateResult != vk::Result::eSuccess) {
            qCWarning(KWIN_VULKAN, "'Allocating' memory for dmabuf failed: %s", vk::to_string(allocateResult).c_str());
            return nullptr;
        }

        bindInfos[i] = vk::BindImageMemoryInfo{image.get(), memory.get(), 0};
        if (disjoint) {
            planeInfo[i] = vk::BindImagePlaneMemoryInfo{
                planeRequirementsInfo.planeAspect,
            };
            bindInfos[i].setPNext(&planeInfo[i]);
        }
        deviceMemory.push_back(std::move(memory));
    }
    const vk::Result bindResult = logical.bindImageMemory2(memoryCount, bindInfos.data());
    if (bindResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "failed to bind image to memory";
        return nullptr;
    }
    // on successful import, the driver takes ownership of the file descriptors
    for (FileDescriptor &fd : duplicatedFds) {
        fd.take();
    }

    std::vector<VkDeviceMemory> nonHppMemory;
    nonHppMemory.reserve(deviceMemory.size());
    for (auto &memory : deviceMemory) {
        nonHppMemory.push_back(memory.release());
    }
    return std::make_shared<VulkanTexture>(this, format->vulkanFormat, image.release(), std::move(nonHppMemory));
}

FormatModifierMap VulkanDevice::queryFormats(VkImageUsageFlags flags) const
{
    FormatModifierMap ret;
    for (const auto &[drmFormat, info] : FormatInfo::s_knownFormats) {
        if (info.vulkanFormat == VK_FORMAT_UNDEFINED) {
            continue;
        }
        vk::DrmFormatModifierPropertiesListEXT modifierInfos;
        vk::FormatProperties2 formatProps{
            {},
            &modifierInfos,
        };
        vk::PhysicalDevice physical{m_physical};
        physical.getFormatProperties2(vk::Format(info.vulkanFormat), &formatProps);
        if (modifierInfos.drmFormatModifierCount == 0) {
            continue;
        }
        std::vector<vk::DrmFormatModifierPropertiesEXT> formatModifierProps(modifierInfos.drmFormatModifierCount);
        modifierInfos.pDrmFormatModifierProperties = formatModifierProps.data();
        physical.getFormatProperties2(vk::Format(info.vulkanFormat), &formatProps);

        for (const auto &props : formatModifierProps) {
            vk::PhysicalDeviceImageDrmFormatModifierInfoEXT modifierInfo{
                props.drmFormatModifier,
                vk::SharingMode::eExclusive,
            };
            vk::PhysicalDeviceExternalImageFormatInfo externalInfo{
                vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT,
                &modifierInfo,
            };
            vk::PhysicalDeviceImageFormatInfo2 imageInfo{
                vk::Format(info.vulkanFormat),
                vk::ImageType::e2D,
                vk::ImageTiling::eDrmFormatModifierEXT,
                vk::ImageUsageFlags(flags),
                vk::ImageCreateFlags(),
                &externalInfo,
            };
            vk::ExternalImageFormatProperties externalProps;
            vk::ImageFormatProperties2 imageProps{
                {},
                &externalProps,
            };

            const vk::Result result = physical.getImageFormatProperties2(&imageInfo, &imageProps);
            if (result == vk::Result::eErrorFormatNotSupported) {
                qCDebug(KWIN_VULKAN) << "unsupported format:" << vk::to_string(vk::Format(info.vulkanFormat));
                continue;
            } else if (result != vk::Result::eSuccess) {
                qCWarning(KWIN_VULKAN) << "failed to get image format properties:" << vk::to_string(result);
                continue;
            }
            if (!(externalProps.externalMemoryProperties.externalMemoryFeatures & vk::ExternalMemoryFeatureFlagBits::eImportable)) {
                qCDebug(KWIN_VULKAN) << "can't import format" << vk::to_string(vk::Format(info.vulkanFormat));
                continue;
            }
            if (!(imageProps.imageFormatProperties.sampleCounts & vk::SampleCountFlagBits::e1)) {
                // just in case there's a format that always requires multi sampling, skip that
                continue;
            }
            if (imageProps.imageFormatProperties.maxExtent.width < 16384 || imageProps.imageFormatProperties.maxExtent.height < 16384) {
                // we don't want to support size constraints right now, so just require a size we're unlikely to exceed
                continue;
            }

            ret[drmFormat].insert(props.drmFormatModifier);
        }
    }
    return ret;
}

std::optional<uint32_t> VulkanDevice::findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags memoryPropertyFlags) const
{
    for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; i++) {
        if ((typeBits & (1 << i)) && ((m_memoryProperties.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags)) {
            return i;
        }
    }
    return std::nullopt;
}

const FormatModifierMap &VulkanDevice::supportedFormats() const
{
    return m_formats;
}

VkDevice VulkanDevice::logicalDevice() const
{
    return m_logical;
}

VkQueue VulkanDevice::transferQueue() const
{
    return m_transferQueue;
}

VkCommandBuffer VulkanDevice::createCommandBuffer()
{
    // clean up old command buffers first
    for (auto it = m_submittedCommandBuffers.begin(); it != m_submittedCommandBuffers.end();) {
        const auto &[buffer, fence] = *it;
        VkResult result = vkGetFenceStatus(m_logical, fence);
        if (result == VK_SUCCESS || result == VK_ERROR_DEVICE_LOST) {
            vkFreeCommandBuffers(m_logical, m_commandPool, 1, &buffer);
            vkDestroyFence(m_logical, fence, nullptr);
            it = m_submittedCommandBuffers.erase(it);
        } else {
            it++;
        }
    }

    auto [result, buffers] = vk::Device(m_logical).allocateCommandBuffers(vk::CommandBufferAllocateInfo{
        m_commandPool,
        vk::CommandBufferLevel::ePrimary,
        1,
    });
    if (result != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "Failed to create a command buffer" << vk::to_string(result);
        return nullptr;
    }
    return buffers.front();
}

std::optional<FileDescriptor> VulkanDevice::submit(VkCommandBuffer buffer)
{
    vk::UniqueCommandBuffer cleanup(buffer);
    auto [fenceResult, fence] = vk::Device(m_logical).createFenceUnique(vk::FenceCreateInfo{});
    if (fenceResult != vk::Result::eSuccess) {
        return std::nullopt;
    }
    vk::CommandBuffer hppBuffer(buffer);
    vk::Result result = vk::Queue(m_transferQueue).submit(vk::SubmitInfo{
                                                              {},
                                                              {},
                                                              hppBuffer,
                                                              {},
                                                          },
                                                          fence.get());
    if (result != vk::Result::eSuccess) {
        return std::nullopt;
    }
    // FIXME it crashes here. Maybe the per-device dispatcher isn't as optional as I thought?
    const auto [fdResult, fd] = vk::Device(m_logical).getFenceFdKHR(vk::FenceGetFdInfoKHR{
        fence.get(),
        vk::ExternalFenceHandleTypeFlagBits::eSyncFd,
    });
    if (fdResult != vk::Result::eSuccess) {
        return std::nullopt;
    }
    m_submittedCommandBuffers.push_back(std::make_pair(cleanup.release(), fence.release()));
    return FileDescriptor{fd};
}
}

#include "moc_vulkan_device.cpp"
