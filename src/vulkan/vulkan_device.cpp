/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023-2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "vulkan_device.h"
#include "core/gpumanager.h"
#include "core/graphicsbuffer.h"
#include "vulkan_buffer.h"
#include "vulkan_logging.h"
#include "vulkan_texture.h"

#include <QDebug>
#include <sys/stat.h>

namespace KWin
{

VulkanDevice::VulkanDevice(vk::raii::PhysicalDevice physicalDevice, vk::raii::Device &&logicalDevice,
                           std::vector<VkQueueFamilyProperties> &&queueProperties, vk::PhysicalDeviceType type,
                           std::optional<VkDeviceSize> minImportedHostPointerAlignment)
    : m_type(type)
    , m_physical(physicalDevice)
    , m_logical(std::move(logicalDevice))
    , m_transferFormats(queryFormats(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT))
    , m_queueProperties(std::move(queueProperties))
    , m_deviceLimits(m_physical.getProperties().limits)
    , m_minImportedHostPointerAlignment(minImportedHostPointerAlignment)
{
    m_memoryProperties = physicalDevice.getMemoryProperties();
    getQueues();
}

VulkanDevice::~VulkanDevice()
{
    Q_EMIT deviceLost();
    m_graphicsQueue.reset();
    m_transferQueue.reset();
    m_importedTextures.clear();
    m_importedBuffers.clear();
    m_logical.clear();
}

void VulkanDevice::getQueues()
{
    // prefer the most minimal capabilities for the transfer queue
    auto transfer = m_queueProperties | std::views::filter([](const VkQueueFamilyProperties &props) {
        return props.queueFlags & VK_QUEUE_TRANSFER_BIT;
    });
    const auto transferIt = std::ranges::min_element(transfer, [](const VkQueueFamilyProperties &left, const VkQueueFamilyProperties &right) {
        return std::popcount(left.queueFlags) < std::popcount(right.queueFlags);
    });
    if (transferIt != transfer.end()) {
        m_transferQueue = VulkanQueue::create(this, std::distance(m_queueProperties.begin(), transferIt.base()));
    }

    auto it = std::ranges::find_if(m_queueProperties, [](const VkQueueFamilyProperties &props) {
        return props.queueFlags & VK_QUEUE_GRAPHICS_BIT;
    });
    Q_ASSERT(it != m_queueProperties.end());
    m_graphicsQueue = VulkanQueue::create(this, std::distance(m_queueProperties.begin(), it));
}

std::shared_ptr<VulkanTexture> VulkanDevice::importBuffer(GraphicsBuffer *buffer, VkImageUsageFlags usage)
{
    if (!buffer->dmabufAttributes() && !buffer->hostDataAttributes()) {
        return nullptr;
    }
    auto it = m_importedTextures.find(buffer);
    if (it != m_importedTextures.end()) {
        return it.value();
    }
    std::shared_ptr<VulkanTexture> ret;
    if (buffer->hostDataAttributes()) {
        // TODO delete this path? It shouldn't be necessary,
        // and can be quite error prone with the whole stride thing...
        // ALSO FIXME there's still GPU resets on AMD sometimes, and consistently on Intel...
        // Maybe try "import buffer as VkBuffer" workaround instead? Should be a lot simpler as well.
        ret = importHostPointerAsTexture(buffer->hostDataAttributes(), usage);
    } else {
        ret = importDmabuf(buffer->dmabufAttributes(), usage);
    }
    if (!ret) {
        return nullptr;
    }
    m_importedTextures[buffer] = ret;
    connect(buffer, &QObject::destroyed, this, [this, buffer]() {
        m_importedTextures.remove(buffer);
    });
    return ret;
}

std::shared_ptr<VulkanBuffer> VulkanDevice::importBufferAsBuffer(GraphicsBuffer *buffer, vk::BufferUsageFlags usage)
{
    if (!buffer->hostDataAttributes() && !buffer->dmabufAttributes()) {
        return nullptr;
    }
    auto it = m_importedBuffers.find(buffer);
    if (it != m_importedBuffers.end()) {
        return it.value();
    }
    std::shared_ptr<VulkanBuffer> ret;
    if (buffer->hostDataAttributes()) {
        ret = importHostPointerAsBuffer(buffer->hostDataAttributes(), usage);
    } else {
        ret = importDmabufAsBuffer(buffer->dmabufAttributes(), usage);
    }
    m_importedBuffers[buffer] = ret;
    if (ret) {
        connect(buffer, &QObject::destroyed, this, [this, buffer]() {
            m_importedTextures.remove(buffer);
        });
    }
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
    auto formatIt = m_transferFormats.find(attributes->format);
    if (formatIt == m_transferFormats.end() || !formatIt->contains(attributes->modifier)) {
        if (formatIt == m_transferFormats.end()) {
            qCWarning(KWIN_VULKAN, "Dmabuf has unsupported format %s", qPrintable(FormatInfo::drmFormatName(attributes->format)));
            for (auto it = m_transferFormats.begin(); it != m_transferFormats.end(); it++) {
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
    vk::ImageDrmFormatModifierExplicitCreateInfoEXT modifierInfo{
        attributes->modifier,
        subLayouts,
    };
    vk::ExternalMemoryImageCreateInfo externalInfo{
        vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT,
        &modifierInfo,
    };
    const bool disjoint = isDisjoint(*attributes);
    vk::ImageCreateInfo imageInfo{
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
        // the queue family index is ignored with share mode exclusive,
        // instead Vulkan implicitly assigns ownership to the first queue
        // the image is used in.
        0,
        vk::ImageLayout::eUndefined,
        &externalInfo,
    };
    auto [imageResult, image] = m_logical.createImage(imageInfo);
    if (imageResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "creating vulkan image failed!" << vk::to_string(imageResult);
        return nullptr;
    }

    const uint32_t memoryCount = disjoint ? attributes->planeCount : 1;
    std::vector<vk::BindImageMemoryInfo> bindInfos;
    bindInfos.resize(memoryCount);
    std::array<vk::BindImagePlaneMemoryInfo, 4> planeInfo;
    std::vector<vk::raii::DeviceMemory> deviceMemory;

    std::array<FileDescriptor, 4> duplicatedFds;
    for (size_t i = 0; i < memoryCount; i++) {
        duplicatedFds[i] = attributes->fd[i].duplicate();
    }

    for (uint32_t i = 0; i < memoryCount; i++) {
        const auto [memoryFdResult, memoryFdProperties] = m_logical.getMemoryFdPropertiesKHR(vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT, duplicatedFds[i].get());
        if (memoryFdResult != vk::Result::eSuccess) {
            qCWarning(KWIN_VULKAN) << "failed to get memory fd properties!" << vk::to_string(memoryFdResult);
            return nullptr;
        }
        vk::ImageMemoryRequirementsInfo2 memRequirementsInfo{image};
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
        const vk::MemoryRequirements2 memRequirements = m_logical.getImageMemoryRequirements2(memRequirementsInfo);
        const auto memoryIndex = findMemoryType(memRequirements.memoryRequirements.memoryTypeBits & memoryFdProperties.memoryTypeBits, {});
        if (!memoryIndex) {
            qCWarning(KWIN_VULKAN, "couldn't find a suitable memory type for %x & %x = %x",
                      memRequirements.memoryRequirements.memoryTypeBits, memoryFdProperties.memoryTypeBits,
                      memRequirements.memoryRequirements.memoryTypeBits & memoryFdProperties.memoryTypeBits);
            return nullptr;
        }

        vk::MemoryDedicatedAllocateInfo dedicatedInfo{image};
        vk::ImportMemoryFdInfoKHR importInfo(vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT, duplicatedFds[i].get(), &dedicatedInfo);
        vk::MemoryAllocateInfo memoryInfo(memRequirements.memoryRequirements.size, memoryIndex.value(), &importInfo);
        auto [allocateResult, memory] = m_logical.allocateMemory(memoryInfo);
        if (allocateResult != vk::Result::eSuccess) {
            qCWarning(KWIN_VULKAN, "'Allocating' memory for dmabuf failed: %s", vk::to_string(allocateResult).c_str());
            return nullptr;
        }

        bindInfos[i] = vk::BindImageMemoryInfo{image, memory, 0};
        if (disjoint) {
            planeInfo[i] = vk::BindImagePlaneMemoryInfo{
                planeRequirementsInfo.planeAspect,
            };
            bindInfos[i].setPNext(&planeInfo[i]);
        }
        deviceMemory.push_back(std::move(memory));
    }
    const vk::Result bindResult = m_logical.bindImageMemory2(bindInfos);
    if (bindResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "failed to bind image to memory";
        return nullptr;
    }
    // on successful import, the driver takes ownership of the file descriptors
    for (FileDescriptor &fd : duplicatedFds) {
        fd.take();
    }
    return std::make_shared<VulkanTexture>(this, vk::Format(format->vulkanFormat), std::move(image),
                                           std::move(deviceMemory), QSize(attributes->width, attributes->height));
}

std::shared_ptr<VulkanTexture> VulkanDevice::importHostPointerAsTexture(const HostMemoryAttributes *attributes, VkImageUsageFlags usage)
{
    const auto format = FormatInfo::get(attributes->format);
    if (!format) {
        qCWarning(KWIN_VULKAN, "Dmabuf has unknown format");
        return nullptr;
    }
    auto formatIt = m_transferFormats.find(attributes->format);
    if (formatIt == m_transferFormats.end() || !formatIt->contains(DRM_FORMAT_MOD_LINEAR)) {
        return nullptr;
    }

    vk::ExternalMemoryImageCreateInfo externalInfo{
        vk::ExternalMemoryHandleTypeFlagBits::eHostAllocationEXT,
    };
    vk::ImageCreateInfo imageInfo{
        vk::ImageCreateFlags(),
        vk::ImageType::e2D,
        vk::Format(format->vulkanFormat),
        vk::Extent3D(attributes->size.width(), attributes->size.height(), 1),
        1,
        1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eLinear,
        vk::ImageUsageFlags(usage),
        vk::SharingMode::eExclusive,
        // the queue family index is ignored with share mode exclusive,
        // instead Vulkan implicitly assigns ownership to the first queue
        // the image is used in.
        0,
        vk::ImageLayout::eUndefined,
        &externalInfo,
    };
    auto [imageResult, image] = m_logical.createImage(imageInfo);
    if (imageResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "creating vulkan image failed!" << vk::to_string(imageResult);
        return nullptr;
    }

    const auto [result, properties] = m_logical.getMemoryHostPointerPropertiesEXT(vk::ExternalMemoryHandleTypeFlagBits::eHostAllocationEXT, attributes->data.get());
    if (result != vk::Result::eSuccess) {
        return nullptr;
    }

    vk::ImageMemoryRequirementsInfo2 memRequirementsInfo{image};
    const vk::MemoryRequirements2 memRequirements = m_logical.getImageMemoryRequirements2(memRequirementsInfo);
    if (memRequirements.memoryRequirements.size > attributes->sizeInBytes) {
        return nullptr;
    }

    const auto memoryIndex = findMemoryType(memRequirements.memoryRequirements.memoryTypeBits & properties.memoryTypeBits, {});
    if (!memoryIndex) {
        qCWarning(KWIN_VULKAN, "couldn't find a suitable memory type for %x & %x = %x",
                  memRequirements.memoryRequirements.memoryTypeBits, properties.memoryTypeBits,
                  memRequirements.memoryRequirements.memoryTypeBits & properties.memoryTypeBits);
        return nullptr;
    }

    vk::ImportMemoryHostPointerInfoEXT importInfo(vk::ExternalMemoryHandleTypeFlagBits::eHostAllocationEXT, attributes->data.get());
    // NOTE that this cannot use memoryRequirements.size, since it may be smaller
    // than the buffer, and that gets the import rejected on Intel
    vk::MemoryAllocateInfo memoryInfo(attributes->sizeInBytes, memoryIndex.value(), &importInfo);
    auto [allocateResult, memory] = m_logical.allocateMemory(memoryInfo);
    if (allocateResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN, "'Allocating' memory for host memory image failed: %s", vk::to_string(allocateResult).c_str());
        return nullptr;
    }

    vk::BindImageMemoryInfo bindInfo{image, memory, 0};
    const vk::Result bindResult = m_logical.bindImageMemory2(bindInfo);
    if (bindResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "failed to bind image to memory";
        return nullptr;
    }
    std::vector<vk::raii::DeviceMemory> memories;
    memories.push_back(std::move(memory));
    return std::make_shared<VulkanTexture>(this, vk::Format(format->vulkanFormat), std::move(image),
                                           std::move(memories), attributes->size);
}

std::shared_ptr<VulkanBuffer> VulkanDevice::importHostPointerAsBuffer(const HostMemoryAttributes *attributes, vk::BufferUsageFlags usage)
{
    const auto format = FormatInfo::get(attributes->format);
    if (!format) {
        qCWarning(KWIN_VULKAN, "Dmabuf has unknown format");
        return nullptr;
    }
    auto formatIt = m_transferFormats.find(attributes->format);
    if (formatIt == m_transferFormats.end() || !formatIt->contains(DRM_FORMAT_MOD_LINEAR)) {
        return nullptr;
    }

    vk::ExternalMemoryBufferCreateInfo externalInfo{
        vk::ExternalMemoryHandleTypeFlagBits::eHostAllocationEXT,
    };
    vk::BufferCreateInfo bufferInfo{
        vk::BufferCreateFlags(),
        attributes->sizeInBytes,
        usage,
        vk::SharingMode::eExclusive,
        {},
        &externalInfo,
    };
    auto [imageResult, buffer] = m_logical.createBuffer(bufferInfo);
    if (imageResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "creating vulkan buffer failed!" << vk::to_string(imageResult);
        return nullptr;
    }

    const auto [result, properties] = m_logical.getMemoryHostPointerPropertiesEXT(vk::ExternalMemoryHandleTypeFlagBits::eHostAllocationEXT, attributes->data.get());
    if (result != vk::Result::eSuccess) {
        return nullptr;
    }

    vk::BufferMemoryRequirementsInfo2 memRequirementsInfo{buffer};
    const vk::MemoryRequirements2 memRequirements = m_logical.getBufferMemoryRequirements2(memRequirementsInfo);
    if (memRequirements.memoryRequirements.size > attributes->sizeInBytes) {
        return nullptr;
    }

    const auto memoryIndex = findMemoryType(memRequirements.memoryRequirements.memoryTypeBits & properties.memoryTypeBits, {});
    if (!memoryIndex) {
        qCWarning(KWIN_VULKAN, "couldn't find a suitable memory type for %x & %x = %x",
                  memRequirements.memoryRequirements.memoryTypeBits, properties.memoryTypeBits,
                  memRequirements.memoryRequirements.memoryTypeBits & properties.memoryTypeBits);
        return nullptr;
    }

    vk::ImportMemoryHostPointerInfoEXT importInfo(vk::ExternalMemoryHandleTypeFlagBits::eHostAllocationEXT, attributes->data.get());
    // NOTE that this cannot use memoryRequirements.size, since it may be smaller
    // than the buffer, and that gets the import rejected on Intel
    vk::MemoryAllocateInfo memoryInfo(attributes->sizeInBytes, memoryIndex.value(), &importInfo);
    auto [allocateResult, memory] = m_logical.allocateMemory(memoryInfo);
    if (allocateResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN, "'Allocating' memory for host memory image failed: %s", vk::to_string(allocateResult).c_str());
        return nullptr;
    }

    const vk::Result bindResult = buffer.bindMemory(memory, 0);
    if (bindResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "failed to bind image to memory";
        return nullptr;
    }
    return std::make_shared<VulkanBuffer>(std::move(buffer), std::move(memory), attributes->sizeInBytes);
}

std::shared_ptr<VulkanBuffer> VulkanDevice::importDmabufAsBuffer(const DmaBufAttributes *attributes, vk::BufferUsageFlags usage)
{
    const auto format = FormatInfo::get(attributes->format);
    if (!format) {
        qCWarning(KWIN_VULKAN, "Dmabuf has unknown format");
        return nullptr;
    }
    if (attributes->modifier != DRM_FORMAT_MOD_LINEAR) {
        return nullptr;
    }
    auto formatIt = m_transferFormats.find(attributes->format);
    if (formatIt == m_transferFormats.end() || !formatIt->contains(DRM_FORMAT_MOD_LINEAR)) {
        return nullptr;
    }
    if (isDisjoint(*attributes)) {
        return nullptr;
    }

    const vk::DeviceSize size = attributes->pitch[0] * attributes->width;

    vk::ExternalMemoryBufferCreateInfo externalInfo{
        vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT,
    };
    vk::BufferCreateInfo bufferInfo{
        vk::BufferCreateFlags(),
        size,
        usage,
        vk::SharingMode::eExclusive,
        {},
        &externalInfo,
    };
    auto [imageResult, buffer] = m_logical.createBuffer(bufferInfo);
    if (imageResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "creating vulkan buffer failed!" << vk::to_string(imageResult);
        return nullptr;
    }

    FileDescriptor duplicatedFd = attributes->fd[0].duplicate();
    const auto [memoryFdResult, memoryFdProperties] = m_logical.getMemoryFdPropertiesKHR(vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT, duplicatedFd.get());
    if (memoryFdResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "failed to get memory fd properties!" << vk::to_string(memoryFdResult);
        return nullptr;
    }
    vk::BufferMemoryRequirementsInfo2 memRequirementsInfo{buffer};
    const vk::MemoryRequirements2 memRequirements = m_logical.getBufferMemoryRequirements2(memRequirementsInfo);
    const auto memoryIndex = findMemoryType(memRequirements.memoryRequirements.memoryTypeBits & memoryFdProperties.memoryTypeBits, {});
    if (!memoryIndex) {
        qCWarning(KWIN_VULKAN, "couldn't find a suitable memory type for %x & %x = %x",
                  memRequirements.memoryRequirements.memoryTypeBits, memoryFdProperties.memoryTypeBits,
                  memRequirements.memoryRequirements.memoryTypeBits & memoryFdProperties.memoryTypeBits);
        return nullptr;
    }

    vk::MemoryDedicatedAllocateInfo dedicatedInfo{{}, buffer};
    vk::ImportMemoryFdInfoKHR importInfo(vk::ExternalMemoryHandleTypeFlagBits::eDmaBufEXT, duplicatedFd.get(), &dedicatedInfo);
    vk::MemoryAllocateInfo memoryInfo(memRequirements.memoryRequirements.size, memoryIndex.value(), &importInfo);
    auto [allocateResult, memory] = m_logical.allocateMemory(memoryInfo);
    if (allocateResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN, "'Allocating' memory for dmabuf failed: %s", vk::to_string(allocateResult).c_str());
        return nullptr;
    }

    const vk::Result bindResult = buffer.bindMemory(memory, 0);
    if (bindResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "failed to bind image to memory";
        return nullptr;
    }
    // on successful import, the driver takes ownership of the file descriptor(s)
    duplicatedFd.take();

    return std::make_shared<VulkanBuffer>(std::move(buffer), std::move(memory), size);
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

std::optional<uint32_t> VulkanDevice::findMemoryType(uint32_t typeBits, vk::MemoryPropertyFlags memoryPropertyFlags) const
{
    for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; i++) {
        if ((typeBits & (1 << i)) && ((m_memoryProperties.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags)) {
            return i;
        }
    }
    return std::nullopt;
}

bool VulkanDevice::isSoftwareRenderer() const
{
    return m_type == vk::PhysicalDeviceType::eCpu;
}

vk::PhysicalDeviceType VulkanDevice::type() const
{
    return m_type;
}

const FormatModifierMap &VulkanDevice::transferFormats() const
{
    return m_transferFormats;
}

const vk::raii::Device &VulkanDevice::logicalDevice() const
{
    return m_logical;
}

VulkanQueue *VulkanDevice::graphicsQueue() const
{
    return m_graphicsQueue.get();
}

VulkanQueue *VulkanDevice::transferQueue() const
{
    return m_transferQueue.get();
}

std::span<const VkQueueFamilyProperties> VulkanDevice::queueFamilyProperties() const
{
    return m_queueProperties;
}

float VulkanDevice::nanosecondsPerQueryTick() const
{
    return m_deviceLimits.timestampPeriod;
}

std::optional<vk::raii::Semaphore> VulkanDevice::importSemaphore(FileDescriptor &&syncFd) const
{
    if (!syncFd.isValid()) {
        return std::nullopt;
    }
    vk::SemaphoreCreateInfo semaphoreInfo{};
    auto [result, semaphore] = m_logical.createSemaphore(semaphoreInfo);
    if (result != vk::Result::eSuccess) {
        return std::nullopt;
    }
    vk::ImportSemaphoreFdInfoKHR importInfo{
        semaphore,
        vk::SemaphoreImportFlagBits::eTemporary,
        vk::ExternalSemaphoreHandleTypeFlagBits::eSyncFd,
        syncFd.get(),
    };
    result = m_logical.importSemaphoreFdKHR(importInfo);
    if (result != vk::Result::eSuccess) {
        return std::nullopt;
    }
    // the driver takes ownership of the fd on successful import
    syncFd.take();
    return std::move(semaphore);
}

void VulkanDevice::handleDeviceLoss()
{
    if (m_lost) {
        return;
    }
    qCWarning(KWIN_VULKAN, "Vulkan device lost!");
    m_lost = true;
    Q_EMIT deviceLost();
}

vk::raii::DeviceMemory VulkanDevice::allocateMemory(const vk::ImageCreateInfo &imageInfo, vk::MemoryPropertyFlags memoryProperties)
{
    const auto requirements = m_logical.getImageMemoryRequirements(vk::DeviceImageMemoryRequirements{
        &imageInfo,
    });
    if (const auto typeIndex = findMemoryType(requirements.memoryRequirements.memoryTypeBits, memoryProperties)) {
        auto [result, ret] = m_logical.allocateMemory(vk::MemoryAllocateInfo{
            requirements.memoryRequirements.size,
            *typeIndex,
        });
        if (result == vk::Result::eSuccess) {
            return std::move(ret);
        } else {
            qCWarning(KWIN_VULKAN) << "Allocating memory for an image failed:" << vk::to_string(result);
            return nullptr;
        }
    } else {
        qCWarning(KWIN_VULKAN) << "could not find a suitable memory index for an image";
        return nullptr;
    }
}

vk::raii::DeviceMemory VulkanDevice::allocateMemory(const vk::BufferCreateInfo &bufferInfo, vk::MemoryPropertyFlags memoryProperties)
{
    const auto requirements = m_logical.getBufferMemoryRequirements(vk::DeviceBufferMemoryRequirements{
        &bufferInfo,
    });
    if (const auto typeIndex = findMemoryType(requirements.memoryRequirements.memoryTypeBits, memoryProperties)) {
        auto [result, ret] = m_logical.allocateMemory(vk::MemoryAllocateInfo{
            requirements.memoryRequirements.size,
            *typeIndex,
        });
        if (result == vk::Result::eSuccess) {
            return std::move(ret);
        } else {
            qCWarning(KWIN_VULKAN) << "Allocating memory for a buffer failed:" << vk::to_string(result);
            return nullptr;
        }
    } else {
        qCWarning(KWIN_VULKAN) << "could not find a suitable memory index for a buffer";
        return nullptr;
    }
}

std::optional<VkDeviceSize> VulkanDevice::minImportedHostPointerAlignment() const
{
    return m_minImportedHostPointerAlignment;
}

void VulkanDevice::waitIdle()
{
    m_graphicsQueue->waitIdle();
}

}

#include "moc_vulkan_device.cpp"
