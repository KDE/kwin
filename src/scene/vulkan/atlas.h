/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/atlas.h"

namespace KWin
{

class VulkanDevice;
class VulkanTexture;

class AtlasVulkan : public Atlas
{
public:
    static std::unique_ptr<AtlasVulkan> create(VulkanDevice *device, const QList<QImage> &images);

    explicit AtlasVulkan(VulkanDevice *device);
    ~AtlasVulkan() override;

    VulkanTexture *texture() const;

    Sprite sprite(uint spriteId) const override;
    bool update(uint spriteId, const QImage &image, const Rect &damage) override;
    bool reset(const QList<QImage> &images) override;

private:
    void upload(uint spriteId, const QImage &data, const Rect &damage);

    VulkanDevice *const m_device;
    std::unique_ptr<VulkanTexture> m_texture;
    QList<Sprite> m_sprites;
};

} // namespace KWin
