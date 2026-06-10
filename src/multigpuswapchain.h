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

#include <QObject>
#include <optional>

namespace KWin
{

class DrmDevice;
class EglContext;
class EglSwapchain;
class EglSwapchainSlot;
class GraphicsBuffer;
class OutputFrame;
class RenderDevice;
class VulkanDevice;
class VulkanSwapchain;
class VulkanSwapchainSlot;
class SyncReleasePoint;
class QPainterSwapchain;
class QPainterSwapchainSlot;
class VulkanBuffer;
class MultiGpuCopy;

class KWIN_EXPORT MultiGpuSwapchain : public QObject
{
    Q_OBJECT
public:
    explicit MultiGpuSwapchain(std::unique_ptr<MultiGpuCopy> &&firstCopy, std::unique_ptr<MultiGpuCopy> &&secondCopy);
    ~MultiGpuSwapchain() override;

    struct Ret
    {
        GraphicsBuffer *buffer;
        FileDescriptor sync;
        std::shared_ptr<SyncReleasePoint> releasePoint;
    };
    std::optional<Ret> copyRgbBuffer(GraphicsBuffer *buffer, const Region &damage, FileDescriptor &&sync, OutputFrame *frame,
                                     const std::shared_ptr<SyncReleasePoint> &releasePoint);

    void resetDamageTracking();

    uint32_t format() const;
    uint64_t modifier() const;
    QSize size() const;
    bool needsRecreation() const;

    static std::unique_ptr<MultiGpuSwapchain> createForSampling(RenderDevice *sourceDevice, DrmDevice *targetDevice, uint32_t format, uint64_t modifier, const QSize &size, const FormatModifierMap &importFormats);

    struct AllocationInfo
    {
        std::unique_ptr<MultiGpuSwapchain> swapchain;
        ModifierList importModifiers;
    };
    static std::optional<AllocationInfo> createForScanout(RenderDevice *sourceDevice, DrmDevice *targetDevice, uint32_t format, const ModifierList &modifiers, const QSize &size, const FormatModifierMap &importFormats);

private:
    std::optional<Ret> copyWithVulkanToCpu(GraphicsBuffer *buffer, const Region &damage, FileDescriptor &&sync, OutputFrame *frame,
                                           const std::shared_ptr<SyncReleasePoint> &releasePoint);
    void handleDeviceRemoved(RenderDevice *device);
    void handleGpuReset();

    std::unique_ptr<MultiGpuCopy> m_firstCopy;
    std::unique_ptr<MultiGpuCopy> m_secondCopy;
    const uint32_t m_format;
    const uint64_t m_modifier;
    const QSize m_size;
    bool m_needsRecreation = false;
    friend class EglMultiGpuCopy;
    friend class VulkanMultiGpuCopy;
};
}
