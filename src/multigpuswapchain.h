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

class KWIN_EXPORT MultiGpuSwapchain : public QObject
{
    Q_OBJECT
public:
    explicit MultiGpuSwapchain(RenderDevice *copyDevice, DrmDevice *targetDevice, const std::shared_ptr<EglContext> &eglContext, std::shared_ptr<EglSwapchain> &&eglSwapchain);
    explicit MultiGpuSwapchain(RenderDevice *copyDevice, DrmDevice *targetDevice, std::unique_ptr<VulkanSwapchain> &&swapchain);
    ~MultiGpuSwapchain() override;

    struct Ret
    {
        GraphicsBuffer *buffer;
        FileDescriptor sync;
    };
    std::optional<Ret> copyRgbBuffer(GraphicsBuffer *buffer, const Region &damage, FileDescriptor &&sync, OutputFrame *frame);

    void resetDamageTracking();

    uint32_t format() const;
    uint64_t modifier() const;
    QSize size() const;
    bool needsRecreation() const;

    /**
     * NOTE that the copyDevice needs to be chosen carefully. Importing a buffer to a given device
     * (even if just for rendering) causes the kernel to possibly migrate the buffer to that device.
     * If for example a buffer on a dedicated GPU is imported to an integrated GPU, it will be moved
     * to system memory, and rendering performance on the dedicated GPU will suffer.
     * Scanning out buffers from one GPU while it's on another GPU is also usually impossible.
     */
    static std::unique_ptr<MultiGpuSwapchain> create(RenderDevice *copyDevice, DrmDevice *targetDevice, uint32_t format, uint64_t modifier, const QSize &size, const FormatModifierMap &importFormats);

private:
    std::optional<Ret> copyWithVulkan(GraphicsBuffer *buffer, const Region &damage, FileDescriptor &&sync, OutputFrame *frame);
    std::optional<Ret> copyWithEGL(GraphicsBuffer *buffer, const Region &damage, FileDescriptor &&sync, OutputFrame *frame);
    void handleDeviceRemoved(RenderDevice *device);
    void handleGpuReset();
    void deleteResources();

    DrmDevice *m_targetDevice = nullptr;
    RenderDevice *m_copyDevice = nullptr;
    std::shared_ptr<EglContext> m_copyContext;
    std::shared_ptr<EglSwapchain> m_eglSwapchain;
    std::shared_ptr<EglSwapchainSlot> m_currentEglSlot;
    std::unique_ptr<VulkanSwapchain> m_vulkanSwapchain;
    std::shared_ptr<VulkanSwapchainSlot> m_currentVulkanSlot;
    DamageJournal m_journal;
    const uint32_t m_format;
    const uint64_t m_modifier;
    const QSize m_size;
    bool m_needsRecreation = false;
};
}
