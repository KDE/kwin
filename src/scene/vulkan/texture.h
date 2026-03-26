/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "scene/texture.h"

#include <QImage>

namespace KWin
{

class GraphicsBuffer;
class Region;
class Rect;
class VulkanTexture;
class VulkanDevice;

class TextureVulkan : public Texture
{
public:
    ~TextureVulkan() override;

    // TODO add support for multiple planes
    VulkanTexture *texture() const;

protected:
    std::shared_ptr<VulkanTexture> m_texture;
};

class ImageTextureVulkan : public TextureVulkan
{
public:
    static std::unique_ptr<ImageTextureVulkan> create(VulkanDevice *device, const QImage &image);

    explicit ImageTextureVulkan(VulkanDevice *device);

    void attach(GraphicsBuffer *buffer, const Region &region) override;

    bool upload(const QImage &image);
    void upload(const QImage &image, const Rect &region) override;

private:
    VulkanDevice *const m_device;
};

class BufferTextureVulkan : public TextureVulkan
{
public:
    static std::unique_ptr<BufferTextureVulkan> create(VulkanDevice *device, GraphicsBuffer *buffer);

    explicit BufferTextureVulkan(VulkanDevice *device);

    bool attach(GraphicsBuffer *buffer);
    void attach(GraphicsBuffer *buffer, const Region &region) override;

    void upload(const QImage &image, const Rect &region) override;

private:
    void reset();

    bool loadShmTexture(GraphicsBuffer *buffer);
    void updateShmTexture(GraphicsBuffer *buffer, const Region &region);
    bool loadDmabufTexture(GraphicsBuffer *buffer);
    void updateDmabufTexture(GraphicsBuffer *buffer);
    bool loadSinglePixelTexture(GraphicsBuffer *buffer);
    void updateSinglePixelTexture(GraphicsBuffer *buffer);

    enum class BufferType {
        None,
        Shm,
        DmaBuf,
        SinglePixel,
    };

    BufferType m_bufferType = BufferType::None;
    VulkanDevice *m_device;
};

}
