/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "texture.h"
#include "core/graphicsbuffer.h"
#include "core/graphicsbufferview.h"
#include "vulkan/vulkan_backend.h"
#include "vulkan/vulkan_device.h"
#include "vulkan/vulkan_logging.h"
#include "vulkan/vulkan_texture.h"

namespace KWin
{

TextureVulkan::~TextureVulkan()
{
}

VulkanTexture *TextureVulkan::texture() const
{
    return m_texture.get();
}

std::unique_ptr<ImageTextureVulkan> ImageTextureVulkan::create(VulkanDevice *device, const QImage &image)
{
    return nullptr;
}

ImageTextureVulkan::ImageTextureVulkan(VulkanDevice *device)
    : m_device(device)
{
}

void ImageTextureVulkan::attach(GraphicsBuffer *buffer, const Region &region)
{
    Q_UNREACHABLE();
}

bool ImageTextureVulkan::upload(const QImage &image)
{
    upload(image, image.rect());
    return m_texture != nullptr;
}

void ImageTextureVulkan::upload(const QImage &image, const Rect &region)
{
    const auto format = VulkanTexture::qImageToVulkanFormat(image.format());
    if (!format.has_value()) {
        m_texture.reset();
        return;
    }
    if (!m_texture || m_texture->size() != image.size() || m_texture->format() != *format) {
        m_texture = VulkanTexture::allocate(m_device, *format, image.size(), vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc);
        if (!m_texture) {
            return;
        }
    }
    m_texture->update(image, Rect(image.rect()));
}

std::unique_ptr<BufferTextureVulkan> BufferTextureVulkan::create(VulkanDevice *device, GraphicsBuffer *buffer)
{
    auto texture = std::make_unique<BufferTextureVulkan>(device);
    if (texture->attach(buffer)) {
        return texture;
    }
    return nullptr;
}

BufferTextureVulkan::BufferTextureVulkan(VulkanDevice *device)
    : m_device(device)
{
}

bool BufferTextureVulkan::attach(GraphicsBuffer *buffer)
{
    if (buffer->dmabufAttributes()) {
        return loadDmabufTexture(buffer);
    } else if (buffer->shmAttributes()) {
        return loadShmTexture(buffer);
    } else if (buffer->singlePixelAttributes()) {
        return loadSinglePixelTexture(buffer);
    } else {
        qCDebug(KWIN_VULKAN) << "Failed to create BufferTextureVulkan for a buffer of unknown type" << buffer;
        return false;
    }
}

void BufferTextureVulkan::attach(GraphicsBuffer *buffer, const Region &region)
{
    if (buffer->dmabufAttributes()) {
        updateDmabufTexture(buffer);
    } else if (buffer->shmAttributes()) {
        updateShmTexture(buffer, region);
    } else if (buffer->singlePixelAttributes()) {
        updateSinglePixelTexture(buffer);
    } else {
        qCDebug(KWIN_VULKAN) << "Failed to update BufferTextureVulkan for a buffer of unknown type" << buffer;
    }
}

void BufferTextureVulkan::upload(const QImage &image, const Rect &region)
{
    Q_UNREACHABLE();
}

void BufferTextureVulkan::reset()
{
    m_texture.reset();
    m_bufferType = BufferType::None;
    m_size = QSize();
}

bool BufferTextureVulkan::loadShmTexture(GraphicsBuffer *buffer)
{
    const GraphicsBufferView view(buffer);
    if (Q_UNLIKELY(view.isNull())) {
        return false;
    }

    auto texture = VulkanTexture::upload(m_device, *view.image(), vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc);
    if (Q_UNLIKELY(!texture)) {
        return false;
    }

    m_bufferType = BufferType::Shm;
    m_texture = std::move(texture);
    m_size = buffer->size();
    const auto info = FormatInfo::get(buffer->shmAttributes()->format);
    m_isFloatingPoint = info && info->floatingPoint;

    return true;
}

void BufferTextureVulkan::updateShmTexture(GraphicsBuffer *buffer, const Region &region)
{
    if (Q_UNLIKELY(m_bufferType != BufferType::Shm)) {
        reset();
        attach(buffer);
        return;
    }

    const GraphicsBufferView view(buffer);
    if (Q_UNLIKELY(view.isNull())) {
        return;
    }

    m_texture->update(*view.image(), region);
    const auto info = FormatInfo::get(buffer->shmAttributes()->format);
    m_isFloatingPoint = info && info->floatingPoint;
}

bool BufferTextureVulkan::loadDmabufTexture(GraphicsBuffer *buffer)
{
    auto tex = m_device->importBuffer(buffer, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    if (!tex) {
        return false;
    }
    m_texture = std::move(tex);
    m_bufferType = BufferType::DmaBuf;
    m_size = buffer->size();
    const auto info = FormatInfo::get(buffer->dmabufAttributes()->format);
    m_isFloatingPoint = info && info->floatingPoint;
    return true;
}

void BufferTextureVulkan::updateDmabufTexture(GraphicsBuffer *buffer)
{
    if (Q_UNLIKELY(m_bufferType != BufferType::DmaBuf)) {
        reset();
        attach(buffer);
        return;
    }
    auto tex = m_device->importBuffer(buffer, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    if (!tex) {
        return;
    }
    m_texture = std::move(tex);
}

bool BufferTextureVulkan::loadSinglePixelTexture(GraphicsBuffer *buffer)
{
    // TODO this shouldn't allocate a texture,
    // the renderer should just use a color in the shader
    const GraphicsBufferView view(buffer);
    auto texture = VulkanTexture::upload(m_device, *view.image(), vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc);
    if (Q_UNLIKELY(!texture)) {
        return false;
    }
    m_texture = std::move(texture);
    m_bufferType = BufferType::SinglePixel;
    m_size = QSize(1, 1);
    m_isFloatingPoint = false;
    return true;
}

void BufferTextureVulkan::updateSinglePixelTexture(GraphicsBuffer *buffer)
{
    if (Q_UNLIKELY(m_bufferType != BufferType::SinglePixel)) {
        reset();
        attach(buffer);
        return;
    }
    const GraphicsBufferView view(buffer);
    m_texture->update(*view.image(), Rect(0, 0, 1, 1));
}

}
