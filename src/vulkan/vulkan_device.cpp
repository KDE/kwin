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
#include "vulkan_shader.h"
#include "vulkan_texture.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <sys/stat.h>

namespace KWin
{

VulkanDevice::VulkanDevice(vk::raii::PhysicalDevice physicalDevice, vk::raii::Device &&logicalDevice,
                           std::vector<vk::QueueFamilyProperties> &&queueProperties, vk::PhysicalDeviceType type,
                           std::optional<VkDeviceSize> minImportedHostPointerAlignment)
    : m_type(type)
    , m_physical(physicalDevice)
    , m_logical(std::move(logicalDevice))
    , m_sampleFormats(queryFormats(s_imageSampleFlags))
    , m_storageFormats(queryFormats(s_imageStorageFlags))
    , m_queueProperties(std::move(queueProperties))
    , m_deviceLimits(m_physical.getProperties().limits)
    , m_minImportedHostPointerAlignment(minImportedHostPointerAlignment)
    , m_name(physicalDevice.getProperties().deviceName.data())
{
    m_memoryProperties = physicalDevice.getMemoryProperties();
    getQueues();
}

VulkanDevice::~VulkanDevice()
{
    Q_EMIT deviceLost();
    m_yuvConversions.clear();
    m_samplers.clear();
    m_shaders.clear();
    m_graphicsQueue.reset();
    m_computeQueue.reset();
    m_transferQueue.reset();
    m_importedTextures.clear();
    m_importedBuffers.clear();
    m_logical.clear();
}

void VulkanDevice::getQueues()
{
    // prefer the most minimal capabilities for the transfer queue
    auto transfer = m_queueProperties | std::views::filter([](const vk::QueueFamilyProperties &props) {
        return bool(props.queueFlags & vk::QueueFlagBits::eTransfer);
    });
    const auto transferIt = std::ranges::min_element(transfer, [](const VkQueueFamilyProperties &left, const VkQueueFamilyProperties &right) {
        return std::popcount(left.queueFlags) < std::popcount(right.queueFlags);
    });
    if (transferIt != transfer.end()) {
        m_transferQueue = VulkanQueue::create(this, std::distance(m_queueProperties.begin(), transferIt.base()));
    }

    auto compute = m_queueProperties | std::views::filter([](const vk::QueueFamilyProperties &props) {
        return bool(props.queueFlags & vk::QueueFlagBits::eCompute);
    });
    const auto computeIt = std::ranges::min_element(compute, [](const VkQueueFamilyProperties &left, const VkQueueFamilyProperties &right) {
        return std::popcount(left.queueFlags) < std::popcount(right.queueFlags);
    });
    Q_ASSERT(computeIt != compute.end());
    m_computeQueue = VulkanQueue::create(this, std::distance(m_queueProperties.begin(), computeIt.base()));

    auto it = std::ranges::find_if(m_queueProperties, [](const vk::QueueFamilyProperties &props) {
        return bool(props.queueFlags & vk::QueueFlagBits::eGraphics);
    });
    Q_ASSERT(it != m_queueProperties.end());
    m_graphicsQueue = VulkanQueue::create(this, std::distance(m_queueProperties.begin(), it));
}

std::shared_ptr<VulkanTexture> VulkanDevice::importBuffer(GraphicsBuffer *buffer, vk::ImageUsageFlags usage,
                                                          YUVMatrixCoefficients coefficients, EncodingRange range)
{
    if (!buffer->dmabufAttributes()) {
        return nullptr;
    }
    auto it = m_importedTextures.find(buffer);
    if (it != m_importedTextures.end()) {
        return it.value();
    }
    std::shared_ptr<VulkanTexture> ret = importDmabuf(buffer->dmabufAttributes(), usage, coefficients, range);
    m_importedTextures[buffer] = ret;
    connect(buffer, &QObject::destroyed, this, [this, buffer]() {
        m_importedTextures.remove(buffer);
    });
    return ret;
}

std::shared_ptr<VulkanBuffer> VulkanDevice::importBufferAsBuffer(GraphicsBuffer *buffer)
{
    if (!buffer->dmabufAttributes() && !buffer->hostDataAttributes()) {
        return nullptr;
    }
    auto it = m_importedBuffers.find(buffer);
    if (it != m_importedBuffers.end()) {
        return it.value();
    }
    constexpr auto usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst;
    std::shared_ptr<VulkanBuffer> ret;
    if (buffer->hostDataAttributes()) {
        ret = importHostPointerAsBuffer(buffer->hostDataAttributes(), usage);
    } else {
        ret = importDmabufAsBuffer(buffer->dmabufAttributes(), usage);
    }
    m_importedBuffers[buffer] = ret;
    connect(buffer, &QObject::destroyed, this, [this, buffer]() {
        m_importedBuffers.remove(buffer);
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

std::shared_ptr<VulkanTexture> VulkanDevice::importDmabuf(const DmaBufAttributes *attributes, vk::ImageUsageFlags usage,
                                                          YUVMatrixCoefficients coefficients, EncodingRange range)
{
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
    const auto info = FormatInfo::get(attributes->format);
    const auto format = vk::Format(info->vulkanFormat);
    vk::ImageCreateInfo imageInfo{
        disjoint ? vk::ImageCreateFlagBits::eDisjoint : vk::ImageCreateFlags(),
        vk::ImageType::e2D,
        format,
        vk::Extent3D(attributes->width, attributes->height, 1),
        1,
        1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eDrmFormatModifierEXT,
        usage,
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

    vk::raii::ImageView view{nullptr};
    vk::raii::Sampler sampler{nullptr};
    if (usage & vk::ImageUsageFlagBits::eSampled) {
        vk::ImageViewCreateInfo viewInfo{
            vk::ImageViewCreateFlags{},
            image,
            vk::ImageViewType::e2D,
            format,
            vk::ComponentMapping{},
            vk::ImageSubresourceRange{
                vk::ImageAspectFlagBits::eColor,
                0,
                1,
                0,
                1,
            },
        };
        vk::SamplerYcbcrConversionInfo ycbcrInfo;
        if (info->yuv) {
            Q_ASSERT(coefficients != YUVMatrixCoefficients::Identity);
            auto conversion = createYuvConversion(SamplerKey{
                .format = format,
                .coefficients = coefficients,
                .encodingRange = range,
            });
            if (!conversion) {
                return nullptr;
            }
            ycbcrInfo.setConversion(*conversion);
            viewInfo.setPNext(&ycbcrInfo);
        }
        auto [viewResult, view2] = m_logical.createImageView(viewInfo);
        if (viewResult != vk::Result::eSuccess) {
            return nullptr;
        }
        view = std::move(view2);

        sampler = VulkanTexture::createSampler(this);
        if (!*sampler) {
            return nullptr;
        }
    }

    return std::make_shared<VulkanTexture>(this, format, std::move(image),
                                           std::move(view), std::move(sampler), std::move(deviceMemory),
                                           QSize(attributes->width, attributes->height));
}

std::shared_ptr<VulkanBuffer> VulkanDevice::importDmabufAsBuffer(const DmaBufAttributes *attributes, vk::BufferUsageFlags usage)
{
    if (attributes->modifier != DRM_FORMAT_MOD_LINEAR) {
        // VkBuffer is always linear
        return nullptr;
    }
    if (isDisjoint(*attributes)) {
        return nullptr;
    }

    const vk::DeviceSize size = attributes->pitch[0] * attributes->height;

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

std::shared_ptr<VulkanBuffer> VulkanDevice::importHostPointerAsBuffer(const HostMemoryAttributes *attributes, vk::BufferUsageFlags usage)
{
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

FormatModifierMap VulkanDevice::queryFormats(vk::ImageUsageFlags flags) const
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
                flags,
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

QString VulkanDevice::name() const
{
    return m_name;
}

std::optional<VkDeviceSize> VulkanDevice::minImportedHostPointerAlignment() const
{
    return m_minImportedHostPointerAlignment;
}

const FormatModifierMap &VulkanDevice::sampleFormats() const
{
    return m_sampleFormats;
}

const FormatModifierMap &VulkanDevice::storageFormats() const
{
    return m_storageFormats;
}

const vk::raii::Device &VulkanDevice::logicalDevice() const
{
    return m_logical;
}

VulkanQueue *VulkanDevice::graphicsQueue() const
{
    return m_graphicsQueue.get();
}

VulkanQueue *VulkanDevice::computeQueue() const
{
    return m_computeQueue.get();
}

VulkanQueue *VulkanDevice::transferQueue() const
{
    return m_transferQueue.get();
}

std::span<const vk::QueueFamilyProperties> VulkanDevice::queueFamilyProperties() const
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

void VulkanDevice::waitIdle()
{
    m_graphicsQueue->waitIdle();
}

const vk::SamplerYcbcrConversion *VulkanDevice::createYuvConversion(const SamplerKey &key)
{
    const auto it = m_yuvConversions.find(key);
    if (it != m_yuvConversions.end()) {
        return &*it->second;
    }
    vk::SamplerYcbcrModelConversion coefficients = vk::SamplerYcbcrModelConversion::eYcbcrIdentity;
    switch (key.coefficients) {
    case YUVMatrixCoefficients::BT601:
        coefficients = vk::SamplerYcbcrModelConversion::eYcbcr601;
        break;
    case YUVMatrixCoefficients::BT709:
        coefficients = vk::SamplerYcbcrModelConversion::eYcbcr709;
        break;
    case YUVMatrixCoefficients::BT2020:
        coefficients = vk::SamplerYcbcrModelConversion::eYcbcr2020;
        break;
    case YUVMatrixCoefficients::Identity:
        coefficients = vk::SamplerYcbcrModelConversion::eRgbIdentity;
        break;
    }
    vk::SamplerYcbcrConversionCreateInfo ycbcrConversionInfo{
        key.format,
        coefficients,
        key.encodingRange == EncodingRange::Full ? vk::SamplerYcbcrRange::eItuFull : vk::SamplerYcbcrRange::eItuNarrow,
        vk::ComponentMapping{},
        vk::ChromaLocation::eMidpoint,
        vk::ChromaLocation::eMidpoint,
        vk::Filter::eLinear,
        false,
    };
    auto [result, conversion] = m_logical.createSamplerYcbcrConversion(ycbcrConversionInfo);
    if (result != vk::Result::eSuccess) {
        return nullptr;
    }
    auto [ret, inserted] = m_yuvConversions.insert_or_assign(key, std::move(conversion));
    return &*ret->second;
}

const vk::Sampler *VulkanDevice::createSampler(const SamplerKey &key)
{
    const auto it = m_samplers.find(key);
    if (it != m_samplers.end()) {
        return &*it->second;
    }
    vk::SamplerCreateInfo samplerInfo{
        vk::SamplerCreateFlags{},
        vk::Filter::eLinear, // mag filter
        vk::Filter::eLinear, // min filter
        vk::SamplerMipmapMode::eNearest,
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge,
        0,
        false, // enable anisotropy
        0, // maxAnisotropy
        false, // enable compare
        vk::CompareOp::eNever,
        0, // min lod
        0, // max lod
        vk::BorderColor::eFloatTransparentBlack,
        false, // unnormalized coordinates
    };
    vk::SamplerYcbcrConversionInfo ycbcrInfo;
    if (key.coefficients != YUVMatrixCoefficients::Identity) {
        // TODO always use eYcbcrIdentity + a matrix in the shader
        // to reduce the number of permutations?
        auto conversion = createYuvConversion(key);
        if (!conversion) {
            return nullptr;
        }
        ycbcrInfo.setConversion(*conversion);
        samplerInfo.setPNext(&ycbcrInfo);
    }
    auto [samplerResult, sampler] = m_logical.createSampler(samplerInfo);
    if (samplerResult != vk::Result::eSuccess) {
        return nullptr;
    }
    auto [ret, inserted] = m_samplers.insert_or_assign(key, std::move(sampler));
    return &*ret->second;
}

bool VulkanDevice::loadYuvShader(const QString &path, std::vector<vk::DescriptorSetLayoutBinding> &&descriptorSetLayouts, vk::Format format)
{
    QByteArray data;
    for (YUVMatrixCoefficients coefficients : {YUVMatrixCoefficients::Identity, YUVMatrixCoefficients::BT601, YUVMatrixCoefficients::BT709, YUVMatrixCoefficients::BT2020}) {
        for (EncodingRange range : {EncodingRange::Limited, EncodingRange::Full}) {
            const SamplerKey samplerKey{format, coefficients, range};
            const ShaderKey shaderKey{path, samplerKey};

            const auto it = m_shaders.find(shaderKey);
            if (it != m_shaders.end()) {
                continue;
            }

            if (data.isEmpty()) {
                const QString absolutePath = QFileInfo(path).isAbsolute() ? path : QStandardPaths::locate(QStandardPaths::GenericDataLocation, path);
                QFile file(absolutePath);
                if (!file.open(QFile::OpenModeFlag::ReadOnly)) {
                    qCWarning(KWIN_VULKAN, "Couldn't open shader file %s / %s", qPrintable(path), qPrintable(absolutePath));
                    return false;
                }
                data = file.readAll();
            }

            for (auto &layout : descriptorSetLayouts) {
                if (layout.descriptorType == vk::DescriptorType::eCombinedImageSampler
                    || layout.descriptorType == vk::DescriptorType::eSampler) {
                    auto sampler = createSampler(samplerKey);
                    if (!sampler) {
                        return false;
                    }
                    layout.setPImmutableSamplers(sampler);
                    Q_ASSERT(layout.descriptorCount == 1);
                }
            }

            auto shader = VulkanShader::compile(this, data, descriptorSetLayouts);
            m_shaders[shaderKey] = shader;
            if (!shader) {
                return false;
            }
        }
    }
    return true;
}

VulkanShader *VulkanDevice::getShader(const QString &path,
                                      vk::Format format,
                                      YUVMatrixCoefficients coefficients,
                                      EncodingRange range)
{
    const SamplerKey samplerKey{format, coefficients, range};
    const ShaderKey shaderKey{path, samplerKey};

    const auto it = m_shaders.find(shaderKey);
    if (it != m_shaders.end()) {
        return it->second.get();
    } else {
        return nullptr;
    }
}

}

#include "moc_vulkan_device.cpp"
