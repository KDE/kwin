/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "shmuploadswapchain.h"

#include "core/drmdevice.h"
#include "core/gpumanager.h"
#include "core/graphicsbuffer.h"
#include "core/renderdevice.h"
#include "core/syncobjtimeline.h"
#include "vulkan/vulkan_buffer.h"
#include "vulkan/vulkan_device.h"
#include "vulkan/vulkan_logging.h"
#include "vulkan/vulkan_swapchain.h"
#include "vulkan/vulkan_texture.h"

#include <QSocketNotifier>

namespace KWin
{

/**
 * Helper class for synchronizing shm buffer accesses, since shm buffers
 * have neither explicit nor implicit sync like dma buffers do
 */
class ShmReferencer
{
public:
    explicit ShmReferencer(GraphicsBuffer *buffer, std::unique_ptr<VulkanShmBuffer> &&vkBuffer, FileDescriptor &&syncFd);

    GraphicsBufferRef m_ref;
    std::unique_ptr<VulkanShmBuffer> m_vkBuffer;
    FileDescriptor m_sync;
    QSocketNotifier m_notifier;
};

ShmReferencer::ShmReferencer(GraphicsBuffer *buffer, std::unique_ptr<VulkanShmBuffer> &&vkBuffer, FileDescriptor &&syncFd)
    : m_ref(buffer)
    , m_vkBuffer(std::move(vkBuffer))
    , m_sync(std::move(syncFd))
    , m_notifier(m_sync.get(), QSocketNotifier::Read)
{
    QObject::connect(&m_notifier, &QSocketNotifier::activated, &m_notifier, [this]() {
        delete this;
    });
    m_notifier.setEnabled(true);
    if (m_sync.isReadable()) {
        delete this;
    }
}

ShmUploadSwapchain::ShmUploadSwapchain(RenderDevice *copyDevice, DrmDevice *targetDevice, std::unique_ptr<VulkanSwapchain> &&swapchain)
    : m_targetDevice(targetDevice)
    , m_copyDevice(copyDevice)
    , m_vulkanSwapchain(std::move(swapchain))
    , m_format(m_vulkanSwapchain->format())
    , m_size(m_vulkanSwapchain->size())
{
    connect(GpuManager::self(), &GpuManager::renderDeviceRemoved, this, &ShmUploadSwapchain::handleDeviceRemoved);
    connect(m_copyDevice->vulkanDevice(), &VulkanDevice::deviceLost, this, &ShmUploadSwapchain::handleGpuReset);
}

ShmUploadSwapchain::~ShmUploadSwapchain()
{
    deleteResources();
}

std::optional<ShmUploadSwapchain::Ret> ShmUploadSwapchain::uploadRgbBuffer(GraphicsBuffer *buffer, const Region &damage)
{
    Q_ASSERT(suitableFor(buffer));
    auto info = FormatInfo::get(buffer->shmAttributes()->format);
    if (damage.isEmpty() && m_currentVulkanSlot) {
        return Ret{
            .buffer = m_currentVulkanSlot->buffer(),
            .sync = m_currentVulkanSlot->releaseFd().duplicate(),
            .releasePoint = m_currentVulkanSlot->releasePoint(),
        };
    }
    VulkanDevice *vk = m_copyDevice->vulkanDevice();

    auto vkBuffer = vk->importShm(buffer, vk::BufferUsageFlagBits::eTransferSrc);
    if (!vkBuffer) {
        return std::nullopt;
    }

    m_currentVulkanSlot = m_vulkanSwapchain->acquire();
    if (!m_currentVulkanSlot) {
        return std::nullopt;
    }

    const Rect completeRect{QPoint(), m_size};
    const Region toRender = (m_journal.accumulate(m_currentVulkanSlot->age(), completeRect) | damage) & completeRect;
    if (toRender.isEmpty()) {
        return Ret{
            .buffer = m_currentVulkanSlot->buffer(),
            .sync = FileDescriptor{},
            .releasePoint = m_currentVulkanSlot->releasePoint(),
        };
    }
    m_journal.add(damage);

    auto commandBuffer = vk->createCommandBuffer();
    vk::Result result = commandBuffer.begin(vk::CommandBufferBeginInfo{
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });
    if (result != vk::Result::eSuccess) {
        m_journal.clear();
        return std::nullopt;
    }

    vk::ImageMemoryBarrier2 memoryBarrier{
        vk::PipelineStageFlagBits2::eAllCommands,
        vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead,
        vk::PipelineStageFlagBits2::eAllCommands,
        vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead,
        vk::ImageLayout::eGeneral,
        vk::ImageLayout::eGeneral,
        vk::QueueFamilyExternal,
        vk->transferQueueFamily(),
        m_currentVulkanSlot->texture()->handle(),
        vk::ImageSubresourceRange{
            vk::ImageAspectFlagBits::eColor,
            0,
            1,
            0,
            1,
        },
    };
    commandBuffer.pipelineBarrier2(vk::DependencyInfo{
        vk::DependencyFlags{},
        {},
        {},
        memoryBarrier,
    });

    const uint32_t bytesPerPixel = info->bitsPerPixel / 8;
    const std::vector<vk::BufferImageCopy> regions = toRender.rects() | std::views::transform([&](const Rect &rect) {
        return vk::BufferImageCopy{
            vk::DeviceSize(rect.y() * buffer->shmAttributes()->stride + rect.x() * bytesPerPixel), // offset in bytes
            uint32_t(buffer->size().width()), // row length
            uint32_t(buffer->size().height()), // image height
            vk::ImageSubresourceLayers{
                vk::ImageAspectFlagBits::eColor,
                0, // mip map level
                0, // base array layer
                1, // layer count
            },
            vk::Offset3D(rect.x(), rect.y(), 0),
            vk::Extent3D(rect.width(), rect.height(), 1),

        };
    }) | std::ranges::to<std::vector>();
    commandBuffer.copyBufferToImage(vkBuffer->handle(), m_currentVulkanSlot->texture()->handle(), vk::ImageLayout::eGeneral, regions);

    memoryBarrier.setSrcQueueFamilyIndex(vk->transferQueueFamily());
    memoryBarrier.setDstQueueFamilyIndex(vk::QueueFamilyExternal);
    commandBuffer.pipelineBarrier2(vk::DependencyInfo{
        vk::DependencyFlags{},
        {},
        {},
        memoryBarrier,
    });

    result = commandBuffer.end();
    if (result != vk::Result::eSuccess) {
        m_journal.clear();
        return std::nullopt;
    }

    auto completionFd = vk->submit(std::move(commandBuffer), FileDescriptor{});
    if (!completionFd.has_value()) {
        m_needsRecreation = true;
        return std::nullopt;
    }
    if (auto dup = completionFd->duplicate()) {
        new ShmReferencer(buffer, std::move(vkBuffer), std::move(dup));
    }
    m_vulkanSwapchain->release(m_currentVulkanSlot.get(), completionFd->duplicate());
    return Ret{
        .buffer = m_currentVulkanSlot->buffer(),
        .sync = std::move(*completionFd),
        .releasePoint = m_currentVulkanSlot->releasePoint(),
    };
}

void ShmUploadSwapchain::handleDeviceRemoved(RenderDevice *device)
{
    if (m_copyDevice == device || m_targetDevice == device->drmDevice()) {
        deleteResources();
    }
}

void ShmUploadSwapchain::handleGpuReset()
{
    deleteResources();
    m_needsRecreation = true;
}

void ShmUploadSwapchain::deleteResources()
{
    if (m_vulkanSwapchain) {
        m_currentVulkanSlot.reset();
        m_vulkanSwapchain.reset();
    }
    m_copyDevice = nullptr;
    m_targetDevice = nullptr;
}

bool ShmUploadSwapchain::suitableFor(GraphicsBuffer *buffer) const
{
    return !m_needsRecreation
        && m_format == buffer->shmAttributes()->format
        && m_size == buffer->shmAttributes()->size;
}

std::unique_ptr<ShmUploadSwapchain> ShmUploadSwapchain::create(RenderDevice *copyDevice, DrmDevice *targetDevice, uint32_t format, const QSize &size, const FormatModifierMap &importFormats)
{
    if (!copyDevice->vulkanDevice() || !copyDevice->vulkanDevice()->supportedFormats().contains(format) || !importFormats.contains(format)) {
        return nullptr;
    }
    auto info = FormatInfo::get(format);
    if (!info) {
        return nullptr;
    }
    auto modifiers = importFormats.value(format).intersected(copyDevice->vulkanDevice()->supportedFormats().value(format));
    auto swapchain = VulkanSwapchain::create(copyDevice->vulkanDevice(), targetDevice->allocator(), size, format, modifiers);
    if (!swapchain) {
        return nullptr;
    }
    return std::make_unique<ShmUploadSwapchain>(copyDevice, targetDevice, std::move(swapchain));
}

}
