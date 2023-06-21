/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "vulkan_texture.h"
#include "vulkan_device.h"

#include <utility>

namespace KWin
{

VulkanTexture::VulkanTexture(vk::Format format, vk::UniqueImage &&image, std::vector<vk::UniqueDeviceMemory> &&memory, vk::UniqueImageView &&imageView)
    : m_format(format)
    , m_memory(std::move(memory))
    , m_image(std::move(image))
    , m_view(std::move(imageView))
{
}

VulkanTexture::VulkanTexture(VulkanTexture &&other)
    : m_format(other.m_format)
    , m_memory(std::move(other.m_memory))
    , m_image(std::move(other.m_image))
    , m_view(std::move(other.m_view))
{
}

VulkanTexture::~VulkanTexture()
{
    m_view.reset();
    m_image.reset();
    m_memory.clear();
}

vk::Image VulkanTexture::handle() const
{
    return m_image.get();
}

vk::ImageView VulkanTexture::view() const
{
    return m_view.get();
}

vk::Format VulkanTexture::format() const
{
    return m_format;
}

static std::optional<vk::Format> qImageToVulkanFormat(QImage::Format format)
{
    // TODO support the rest of the formats using swizzles?
    switch (format) {
    case QImage::Format_Alpha8:
    case QImage::Format_Grayscale8:
        return vk::Format::eR8Unorm;
    case QImage::Format_Grayscale16:
        return vk::Format::eR16Unorm;
    case QImage::Format_RGB16:
        return vk::Format::eR5G6B5UnormPack16;
    case QImage::Format_RGB555:
        return vk::Format::eA1R5G5B5UnormPack16;
    case QImage::Format_RGB888:
        return vk::Format::eR8G8B8Unorm;
    case QImage::Format_RGB444:
    case QImage::Format_ARGB4444_Premultiplied:
        return vk::Format::eR4G4B4A4UnormPack16;
    case QImage::Format_RGBX8888:
    case QImage::Format_RGBA8888_Premultiplied:
        return vk::Format::eR8G8B8A8Unorm;
    case QImage::Format_BGR30:
    case QImage::Format_A2BGR30_Premultiplied:
        return vk::Format::eA2B10G10R10UnormPack32;
    case QImage::Format_RGBX64:
    case QImage::Format_RGBA64_Premultiplied:
        return vk::Format::eR16G16B16A16Unorm;
    default:
        return std::nullopt;
    }
}

std::unique_ptr<VulkanTexture> VulkanTexture::allocate(VulkanDevice *device, vk::Format format, const QSize &size)
{
    auto [imageResult, image] = device->logicalDevice().createImageUnique(vk::ImageCreateInfo{
        vk::ImageCreateFlags(),
        vk::ImageType::e2D,
        format,
        vk::Extent3D(size.width(), size.height(), 1),
        1, // mipmap levels
        1, // array layers
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        vk::SharingMode::eExclusive,
        {}, // queue family indices
        vk::ImageLayout::eUndefined,
    });
    if (imageResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "creating image failed!" << vk::to_string(imageResult);
        return nullptr;
    }
    auto memory = device->allocateMemory(image.get(), vk::MemoryPropertyFlagBits::eDeviceLocal);
    if (!memory) {
        return nullptr;
    }
    const vk::Result bindResult = device->logicalDevice().bindImageMemory(image.get(), memory.get(), 0);
    if (bindResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "binding memory to image failed!" << vk::to_string(bindResult);
        return nullptr;
    }
    auto [viewResult, view] = device->logicalDevice().createImageViewUnique(vk::ImageViewCreateInfo{
        vk::ImageViewCreateFlags(),
        image.get(),
        vk::ImageViewType::e2D,
        format,
        vk::ComponentSwizzle::eIdentity,
        vk::ImageSubresourceRange{
            vk::ImageAspectFlagBits::eColor,
            0, // mipmap base level
            1, // level count
            0, // base array layer
            1, // layer count
        },
    });
    if (viewResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "creating image view failed!" << vk::to_string(viewResult);
        return nullptr;
    }
    std::vector<vk::UniqueDeviceMemory> memories;
    memories.push_back(std::move(memory));
    return std::make_unique<VulkanTexture>(format, std::move(image), std::move(memories), std::move(view));
}

std::unique_ptr<VulkanTexture> VulkanTexture::upload(VulkanDevice *device, const QImage &image)
{
    const auto fmt = qImageToVulkanFormat(image.format());
    if (!fmt) {
        return nullptr;
    }
    auto ret = allocate(device, *fmt, image.size());
    if (!ret) {
        return nullptr;
    }

    // allocate a CPU-visible buffer for the data, and copy the data into it
    const auto [bufferResult, stagingBuffer] = device->logicalDevice().createBufferUnique(vk::BufferCreateInfo{
        vk::BufferCreateFlags(),
        vk::DeviceSize(image.sizeInBytes()),
        vk::BufferUsageFlagBits::eTransferSrc,
    });
    if (bufferResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "creating staging buffer failed!" << vk::to_string(bufferResult);
        return nullptr;
    }
    const auto stagingMemory = device->allocateMemory(stagingBuffer.get(), vk::MemoryPropertyFlagBits::eHostVisible);
    if (!stagingMemory) {
        return nullptr;
    }
    const auto [mapResult, dataPtr] = device->logicalDevice().mapMemory(stagingMemory.get(), 0, image.sizeInBytes(), vk::MemoryMapFlags());
    if (mapResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "mapping memory failed!" << vk::to_string(mapResult);
        return nullptr;
    }
    std::memcpy(dataPtr, image.constBits(), image.sizeInBytes());
    device->logicalDevice().unmapMemory(stagingMemory.get());
    const vk::Result bindResult = device->logicalDevice().bindBufferMemory(stagingBuffer.get(), stagingMemory.get(), 0);
    if (bindResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "binding memory to buffer failed!" << vk::to_string(bindResult);
        return nullptr;
    }

    // transition the image to the layout used for copying
    const auto cmd = device->allocateCommandBuffer();
    const auto cmdBeginResult = cmd->begin(vk::CommandBufferBeginInfo{
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });
    if (cmdBeginResult != vk::Result::eSuccess) {
        qCWarning(KWIN_VULKAN) << "failed to begin command buffer" << vk::to_string(cmdBeginResult);
        return nullptr;
    }
    vk::ImageMemoryBarrier barrier{
        vk::AccessFlags(),
        vk::AccessFlagBits::eTransferWrite,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        ret->handle(),
        vk::ImageSubresourceRange{
            vk::ImageAspectFlagBits::eColor,
            0, // base mipmap level
            1, // mipmap level count
            0, // base array layer
            1, // array layer count
        },
    };
    cmd->pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eTransfer,
        {}, // dependency flags
        {}, // memory barriers
        {}, // buffer memory barriers
        barrier);

    // copy the data to the image, and transition its layout to be ideal for sampling again
    std::array regions{
        vk::BufferImageCopy2{
            0, // buffer offset
            0, // buffer row length
            0, // buffer image height
            vk::ImageSubresourceLayers{
                vk::ImageAspectFlagBits::eColor,
                0, // mip map level
                0, // base array layer
                1, // layer count
            },
            vk::Offset3D(),
            vk::Extent3D(image.width(), image.height(), 1),
        },
    };
    cmd->copyBufferToImage2(vk::CopyBufferToImageInfo2{
        stagingBuffer.get(),
        ret->handle(),
        vk::ImageLayout::eShaderReadOnlyOptimal,
        regions,
    });

    // wait for the above commands to succeed, so that we can safely delete the staging buffer
    // TODO store the staging buffer in the texture & use a semaphore for synchronization instead

    if (!device->submitCommandBufferBlocking(cmd.get())) {
        return nullptr;
    }
    return ret;
}
}
