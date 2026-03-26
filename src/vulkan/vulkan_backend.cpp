/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "vulkan_backend.h"
#include "core/drmdevice.h"
#include "core/renderdevice.h"
#include "vulkan_device.h"
#include "wayland/linuxdmabufv1clientbuffer.h"
#include "wayland_server.h"

namespace KWin
{

VulkanBackend::VulkanBackend(RenderDevice *device)
    : m_device(device)
{
}

bool VulkanBackend::checkGraphicsReset()
{
    // TODO this isn't ideal
    return m_device->isInReset();
}

bool VulkanBackend::testImportBuffer(GraphicsBuffer *buffer)
{
    return m_device->vulkanDevice()->importBuffer(buffer, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != nullptr;
}

RenderDevice *VulkanBackend::renderDevice() const
{
    return m_device;
}

CompositingType VulkanBackend::compositingType() const
{
    return CompositingType::VulkanCompositing;
}

void VulkanBackend::initWayland()
{
    LinuxDmaBufV1ClientBufferIntegration *dmabuf = waylandServer()->linuxDmabuf();
    dmabuf->setRenderBackend(this);
    dmabuf->setSupportedFormatsWithModifiers(QList{LinuxDmaBufV1Feedback::Tranche{
        .device = m_device->drmDevice()->deviceId(),
        .flags = {},
        .formatTable = supportedFormats(),
    }});
    waylandServer()->setRenderBackend(this);
}

}
