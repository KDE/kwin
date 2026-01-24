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

namespace KWin
{

VulkanDevice::VulkanDevice(vk::raii::PhysicalDevice physicalDevice, vk::raii::Device &&logicalDevice,
                           std::vector<VkQueueFamilyProperties> &&queueProperties, vk::PhysicalDeviceType type)
    : m_type(type)
    , m_physical(physicalDevice)
    , m_logical(std::move(logicalDevice))
    // TODO it might be useful to have separate lists for sample + transfer_src
    // and sample + color attachment + transfer_dst
    , m_formats(queryFormats(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT))
    , m_queueProperties(std::move(queueProperties))
    , m_transferQueue(nullptr)
    , m_commandPool(nullptr)
    , m_deviceLimits(m_physical.getProperties().limits)
{
    m_memoryProperties = physicalDevice.getMemoryProperties();
    getQueue();
    createCommandPool();
}

VulkanDevice::~VulkanDevice()
{
    Q_EMIT deviceLost();
    m_transferQueue.waitIdle();
    m_importedTextures.clear();
    m_submittedCommandBuffers.clear();
    m_commandPool.clear();
    m_logical.clear();
}

void VulkanDevice::getQueue()
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
    m_transferQueue = m_logical.getQueue(m_queueFamilyIndex, 0);
}

void VulkanDevice::createCommandPool()
{
    // only one queue for now -> also only one command pool
    auto [result, cmdPool] = m_logical.createCommandPool(vk::CommandPoolCreateInfo{
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        m_queueFamilyIndex,
    });
    if (result != vk::Result::eSuccess) {
        qCCritical(KWIN_VULKAN) << "creating a command pool failed:" << vk::to_string(result);
        return;
    }
    m_commandPool = std::move(cmdPool);
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
    auto ret = importDmabuf(buffer->dmabufAttributes(), usage);
    if (!ret) {
        return nullptr;
    }
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
        // TODO remove these debug prints... or at least make them qCDebug!
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
        m_queueFamilyIndex,
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

const FormatModifierMap &VulkanDevice::supportedFormats() const
{
    return m_formats;
}

const vk::raii::Device &VulkanDevice::logicalDevice() const
{
    return m_logical;
}

const vk::raii::Queue &VulkanDevice::transferQueue() const
{
    return m_transferQueue;
}

uint32_t VulkanDevice::transferQueueFamily() const
{
    return m_queueFamilyIndex;
}

std::span<const VkQueueFamilyProperties> VulkanDevice::queueFamilyProperties() const
{
    return m_queueProperties;
}

float VulkanDevice::nanosecondsPerQueryTick() const
{
    return m_deviceLimits.timestampPeriod;
}

vk::raii::CommandBuffer VulkanDevice::createCommandBuffer()
{
    // clean up old command buffers first
    for (auto it = m_submittedCommandBuffers.begin(); it != m_submittedCommandBuffers.end();) {
        const SubmittedCommand &cmd = *it;
        // TODO use a QSocketNotifier per submission to do this asynchronously?
        if (cmd.completionSyncFd.isReadable()) {
            it = m_submittedCommandBuffers.erase(it);
        } else {
            it++;
        }
    }

    auto [result, buffers] = m_logical.allocateCommandBuffers(vk::CommandBufferAllocateInfo{
        m_commandPool,
        vk::CommandBufferLevel::ePrimary,
        1,
    });
    if (result != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "Failed to create a command buffer" << vk::to_string(result);
        return nullptr;
    }
    return std::move(buffers.front());
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

std::optional<FileDescriptor> VulkanDevice::submit(vk::raii::CommandBuffer &&buffer, FileDescriptor &&syncFd)
{
    vk::ExportFenceCreateInfo exportInfo{
        vk::ExternalFenceHandleTypeFlagBits::eSyncFd,
    };
    auto [fenceResult, fence] = m_logical.createFence(vk::FenceCreateInfo{
        vk::FenceCreateFlags{},
        &exportInfo,
    });
    if (fenceResult != vk::Result::eSuccess) {
        return std::nullopt;
    }
    std::vector<vk::Semaphore> waitSemaphores;
    std::vector<vk::PipelineStageFlags> waitFlags;
    auto waitSemaphore = importSemaphore(std::move(syncFd));
    if (waitSemaphore.has_value()) {
        waitSemaphores.push_back(*waitSemaphore);
        waitFlags.push_back(vk::PipelineStageFlagBits::eAllCommands);
    }
    vk::Result result = m_transferQueue.submit(vk::SubmitInfo{
                                                   waitSemaphores,
                                                   waitFlags,
                                                   *buffer,
                                                   {},
                                               },
                                               fence);
    if (result == vk::Result::eErrorDeviceLost) {
        handleDeviceLoss();
        return std::nullopt;
    } else if (result != vk::Result::eSuccess) {
        return std::nullopt;
    }
    const auto [fdResult, fd] = m_logical.getFenceFdKHR(vk::FenceGetFdInfoKHR{
        fence,
        vk::ExternalFenceHandleTypeFlagBits::eSyncFd,
    });
    if (fdResult != vk::Result::eSuccess) {
        return std::nullopt;
    }
    FileDescriptor ret{fd};
    m_submittedCommandBuffers.push_back(SubmittedCommand{
        .waitSemaphore = waitSemaphore ? std::move(*waitSemaphore) : nullptr,
        .buffer = std::move(buffer),
        .completionSyncFd = ret.duplicate(),
    });
    return ret;
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
    m_transferQueue.waitIdle();
}

}

#include "moc_vulkan_device.cpp"
