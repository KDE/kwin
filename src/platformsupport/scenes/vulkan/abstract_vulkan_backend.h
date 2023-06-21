/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/renderbackend.h"
#include "vulkan_include.h"

#include <memory>
#include <optional>

struct gbm_device;

namespace KWin
{

struct DmaBufAttributes;
class FileDescriptor;

class VulkanTexture;
class VulkanDevice;

class AbstractVulkanBackend : public RenderBackend
{
    Q_OBJECT
public:
    explicit AbstractVulkanBackend();
    ~AbstractVulkanBackend();

    virtual bool init();
    CompositingType compositingType() const override final;
    OutputLayer *primaryLayer(Output *output) override;
    void present(Output *output) override;

protected:
    vk::Instance m_instance;
    std::vector<std::unique_ptr<VulkanDevice>> m_devices;
};

}
