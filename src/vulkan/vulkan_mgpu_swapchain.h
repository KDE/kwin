/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "utils/filedescriptor.h"

#include <QObject>
#include <memory>

namespace KWin
{

class RenderDevice;
class VulkanSwapchain;
class VulkanSwapchainSlot;
class GraphicsBuffer;

class KWIN_EXPORT VulkanMultiGpuSwapchain : public QObject
{
    Q_OBJECT
public:
    explicit VulkanMultiGpuSwapchain(RenderDevice *sourceDevice, RenderDevice *destinationDevice);
    ~VulkanMultiGpuSwapchain() override;

    struct Ret
    {
        GraphicsBuffer *buffer;
        FileDescriptor sync;
    };
    std::optional<Ret> importBuffer(GraphicsBuffer *src, FileDescriptor &&releaseFence);

private:
    void handleDeviceRemoved(RenderDevice *removed);

    RenderDevice *m_src;
    RenderDevice *m_dst;
    std::unique_ptr<VulkanSwapchain> m_swapchain;
    std::shared_ptr<VulkanSwapchainSlot> m_currentSlot;
    std::optional<uint32_t> m_lastFormat;
};

}
