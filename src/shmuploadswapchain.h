/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/drm_formats.h"
#include "core/region.h"
#include "kwin_export.h"
#include "utils/damagejournal.h"
#include "utils/filedescriptor.h"

#include <QImage>
#include <QObject>

namespace KWin
{

class DrmDevice;
class GraphicsBuffer;
class OutputFrame;
class RenderDevice;
class VulkanDevice;
class VulkanSwapchain;
class VulkanSwapchainSlot;
class SyncReleasePoint;

class KWIN_EXPORT ShmUploadSwapchain : public QObject
{
    Q_OBJECT

public:
    explicit ShmUploadSwapchain(RenderDevice *copyDevice, DrmDevice *targetDevice, std::unique_ptr<VulkanSwapchain> &&swapchain);
    ~ShmUploadSwapchain() override;

    struct Ret
    {
        GraphicsBuffer *buffer;
        FileDescriptor sync;
        std::shared_ptr<SyncReleasePoint> releasePoint;
    };
    std::optional<Ret> uploadRgbBuffer(GraphicsBuffer *buffer, const Region &damage);

    bool suitableFor(GraphicsBuffer *buffer) const;

    /**
     * NOTE the copy device needs to be compatible with the target device
     */
    static std::unique_ptr<ShmUploadSwapchain> create(RenderDevice *copyDevice, DrmDevice *targetDevice, uint32_t format, const QSize &size, const FormatModifierMap &importFormats);

private:
    void handleDeviceRemoved(RenderDevice *device);
    void handleGpuReset();
    void deleteResources();

    DrmDevice *m_targetDevice = nullptr;
    RenderDevice *m_copyDevice = nullptr;
    std::unique_ptr<VulkanSwapchain> m_vulkanSwapchain;
    std::shared_ptr<VulkanSwapchainSlot> m_currentVulkanSlot;
    DamageJournal m_journal;
    const uint32_t m_format;
    const QSize m_size;
    bool m_needsRecreation = false;
};

}
