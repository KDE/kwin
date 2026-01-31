/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "vulkan_mgpu_swapchain.h"
#include "core/drmdevice.h"
#include "core/gpumanager.h"
#include "core/graphicsbuffer.h"
#include "opengl/egldisplay.h"
#include "vulkan/vulkan_logging.h"
#include "vulkan/vulkan_texture.h"
#include "vulkan_device.h"
#include "vulkan_swapchain.h"

#include <vulkan/vulkan.hpp>

namespace KWin
{

struct DrmFormat
{
    uint32_t format;
    ModifierList modifiers;
};

static std::optional<DrmFormat> chooseFormat(uint32_t inputFormat, const FormatModifierMap &srcFormats,
                                             const QHash<uint32_t, EglDisplay::DrmFormatInfo> &dstFormats)
{
    if (srcFormats.contains(inputFormat)) {
        auto mods = intersect(srcFormats[inputFormat], dstFormats[inputFormat].allModifiers);
        if (!mods.empty()) {
            return DrmFormat{
                .format = inputFormat,
                .modifiers = std::move(mods),
            };
        }
    }
    const auto info = *FormatInfo::get(inputFormat);
    std::optional<DrmFormat> ret;
    std::optional<uint32_t> retBPP;
    for (auto it = srcFormats.begin(); it != srcFormats.end(); it++) {
        const auto otherInfo = FormatInfo::get(it.key());
        if (!otherInfo.has_value() || otherInfo->bitsPerColor < info.bitsPerColor || otherInfo->alphaBits < info.alphaBits) {
            continue;
        }
        if (!retBPP || otherInfo->bitsPerPixel < retBPP) {
            const uint32_t fmt = it.key();
            const auto mods = intersect(srcFormats[fmt], dstFormats[fmt].allModifiers);
            if (!mods.empty()) {
                ret = DrmFormat{
                    .format = fmt,
                    .modifiers = mods,
                };
                retBPP = otherInfo->bitsPerPixel;
            }
        }
    }
    return ret;
}

VulkanMultiGpuSwapchain::VulkanMultiGpuSwapchain(RenderDevice *sourceDevice, RenderDevice *destinationDevice)
    : m_src(sourceDevice)
    , m_dst(destinationDevice)
{
    connect(GpuManager::s_self.get(), &GpuManager::renderDeviceRemoved, this, &VulkanMultiGpuSwapchain::handleDeviceRemoved);
}

VulkanMultiGpuSwapchain::~VulkanMultiGpuSwapchain()
{
}

std::optional<VulkanMultiGpuSwapchain::Ret> VulkanMultiGpuSwapchain::importBuffer(GraphicsBuffer *src, FileDescriptor &&releaseFence)
{
    if (!m_src || !m_src->vulkanDevice()) {
        return std::nullopt;
    }
    const auto srcVk = m_src->vulkanDevice();
    if (!m_swapchain || m_swapchain->size() != src->size() || m_lastFormat != src->dmabufAttributes()->format) {
        // TODO make the dst formats more generic / independent of EGL and Vulkan
        const auto fmt = chooseFormat(src->dmabufAttributes()->format, srcVk->supportedFormats(), m_dst->eglDisplay()->allSupportedDrmFormats());
        if (!fmt) {
            qCCritical(KWIN_VULKAN, "Couldn't find a compatible format");
            return std::nullopt;
        }
        m_swapchain = VulkanSwapchain::create(srcVk, m_dst->drmDevice()->allocator(), src->size(), fmt->format, fmt->modifiers);
        if (!m_swapchain) {
            return std::nullopt;
        }
    } else if (m_currentSlot) {
        m_swapchain->release(m_currentSlot.get(), std::move(releaseFence));
    }

    const auto srcTexture = srcVk->importBuffer(src, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    if (!srcTexture) {
        qCWarning(KWIN_VULKAN, "Could not import buffer for multi GPU copy!");
        return std::nullopt;
    }

    m_currentSlot = m_swapchain->acquire();
    if (!m_currentSlot) {
        qCWarning(KWIN_VULKAN, "Swapchain acquire failed!");
        return std::nullopt;
    }

    const vk::CommandBuffer commandBuffer(srcVk->createCommandBuffer());
    vk::Result result = commandBuffer.begin(vk::CommandBufferBeginInfo{
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });
    if (result != vk::Result::eSuccess) {
        return std::nullopt;
    }
    std::vector<vk::ImageBlit> regions = {
        vk::ImageBlit{
            // src
            vk::ImageSubresourceLayers{
                vk::ImageAspectFlagBits::eColor,
                0,
                0,
                1,
            },
            std::array{
                vk::Offset3D{0, 0, 0},
                vk::Offset3D{src->size().width(), src->size().height(), 1},
            },
            // dst
            vk::ImageSubresourceLayers{
                vk::ImageAspectFlagBits::eColor,
                0,
                0,
                1,
            },
            std::array{
                vk::Offset3D{0, 0, 0},
                vk::Offset3D{src->size().width(), src->size().height(), 1},
            },
        },
    };
    commandBuffer.blitImage(srcTexture->handle(), vk::ImageLayout::eGeneral,
                            m_currentSlot->texture()->handle(), vk::ImageLayout::eGeneral,
                            regions, vk::Filter::eNearest);
    result = commandBuffer.end();
    if (result != vk::Result::eSuccess) {
        return std::nullopt;
    }
    auto syncFd = srcVk->submit(commandBuffer);

    if (!syncFd.has_value()) {
        qCWarning(KWIN_VULKAN, "Command buffer submission failed!");
        return std::nullopt;
    } else {
        qWarning() << "success?";
        return Ret{
            .buffer = m_currentSlot->buffer(),
            .sync = std::move(*syncFd),
        };
    }
}

void VulkanMultiGpuSwapchain::handleDeviceRemoved(RenderDevice *removed)
{
    if (removed != m_src && removed != m_dst) {
        return;
    }
    m_swapchain.reset();
    m_src = nullptr;
    m_dst = nullptr;
}

}

#include "moc_vulkan_mgpu_swapchain.cpp"
