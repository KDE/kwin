/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/renderbackend.h"
#include "vulkan/vulkan_include.h"

#include <memory>
#include <optional>

struct gbm_device;

namespace KWin
{

struct DmaBufAttributes;
class FileDescriptor;

class VulkanTexture;
class VulkanDevice;

class VulkanBackend : public RenderBackend
{
    Q_OBJECT
public:
    explicit VulkanBackend();
    ~VulkanBackend();

    virtual bool init();
    CompositingType compositingType() const override final;
    std::unique_ptr<SurfaceTexture> createSurfaceTextureWayland(SurfacePixmap *pixmap) override;

protected:
    vk::Instance m_instance;
    std::vector<std::unique_ptr<VulkanDevice>> m_devices;
};

}
