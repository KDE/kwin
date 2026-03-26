/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "scene/ninepatch.h"

#include <memory>

class QImage;

namespace KWin
{

class VulkanTexture;
class VulkanDevice;

class NinePatchVulkan : public NinePatch
{
public:
    static std::unique_ptr<NinePatchVulkan> create(VulkanDevice *device,
                                                   const QImage &image);
    static std::unique_ptr<NinePatchVulkan> create(VulkanDevice *device,
                                                   const QImage &topLeftPatch,
                                                   const QImage &topPatch,
                                                   const QImage &topRightPatch,
                                                   const QImage &rightPatch,
                                                   const QImage &bottomRightPatch,
                                                   const QImage &bottomPatch,
                                                   const QImage &bottomLeftPatch,
                                                   const QImage &leftPatch);

    explicit NinePatchVulkan(std::unique_ptr<VulkanTexture> &&texture);
    ~NinePatchVulkan() override;

    VulkanTexture *texture() const;

private:
    std::unique_ptr<VulkanTexture> m_texture;
};

}
