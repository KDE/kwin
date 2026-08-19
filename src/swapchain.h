/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023-2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/drm_formats.h"
#include "core/graphicsbufferallocator.h"
#include "kwin_export.h"
#include "utils/filedescriptor.h"

#include <QSize>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace KWin
{

class Swapchain;
class SyncReleasePoint;
class GraphicsBufferReleasePoint;

class KWIN_EXPORT SwapchainSlot
{
public:
    explicit SwapchainSlot(GraphicsBuffer *buffer);
    ~SwapchainSlot();

    GraphicsBuffer *buffer() const;
    int age() const;
    bool isBusy() const;
    const FileDescriptor &releaseFd() const;
    std::shared_ptr<SyncReleasePoint> releasePoint();

private:
    GraphicsBuffer *const m_buffer;
    int m_age = 0;
    std::shared_ptr<GraphicsBufferReleasePoint> m_releasePoint;
    friend class Swapchain;
};

class KWIN_EXPORT Swapchain
{
public:
    explicit Swapchain(GraphicsBufferAllocator *allocator, const GraphicsBufferOptions &options, std::shared_ptr<SwapchainSlot> &&initialSlot);
    ~Swapchain();

    QSize size() const;
    uint32_t format() const;
    uint64_t modifier() const;
    bool scanout() const;

    std::shared_ptr<SwapchainSlot> acquire();
    void release(SwapchainSlot *slot, FileDescriptor &&releaseFd);

    void resetBufferAge();

    static std::unique_ptr<Swapchain> create(GraphicsBufferAllocator *allocator, GraphicsBufferOptions options);

private:
    GraphicsBufferAllocator *const m_allocator;
    const GraphicsBufferOptions m_options;
    std::vector<std::shared_ptr<SwapchainSlot>> m_slots;
};

}
