/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/drm_formats.h"
#include "core/graphicsbufferallocator.h"
#include "kwin_export.h"
#include "utils/filedescriptor.h"

#include <QList>
#include <QSize>

#include <cstdint>
#include <epoxy/egl.h>
#include <memory>

namespace KWin
{

class GLFramebuffer;
class GLTexture;
class EglContext;
class SyncReleasePoint;
class GraphicsBufferReleasePoint;

class KWIN_EXPORT EglSwapchainSlot
{
public:
    EglSwapchainSlot(std::shared_ptr<GraphicsBuffer> &&buffer, std::unique_ptr<GLFramebuffer> &&framebuffer, const std::shared_ptr<GLTexture> &texture);
    ~EglSwapchainSlot();

    GraphicsBuffer *buffer() const;
    std::shared_ptr<GLTexture> texture() const;
    GLFramebuffer *framebuffer() const;
    int age() const;
    const FileDescriptor &releaseFd() const;
    std::shared_ptr<SyncReleasePoint> releasePoint();

    static std::shared_ptr<EglSwapchainSlot> create(EglContext *context, std::shared_ptr<GraphicsBuffer> &&buffer);

private:
    bool isBusy() const;

    std::shared_ptr<GraphicsBuffer> m_buffer;
    std::unique_ptr<GLFramebuffer> m_framebuffer;
    std::shared_ptr<GLTexture> m_texture;
    int m_age = 0;
    std::shared_ptr<GraphicsBufferReleasePoint> m_releasePoint;
    friend class EglSwapchain;
};

class KWIN_EXPORT EglSwapchain
{
public:
    EglSwapchain(GraphicsBufferAllocator *allocator, EglContext *context, const GraphicsBufferOptions &options, const std::shared_ptr<EglSwapchainSlot> &seed);
    ~EglSwapchain();

    QSize size() const;
    uint32_t format() const;
    uint64_t modifier() const;
    bool scanout() const;

    std::shared_ptr<EglSwapchainSlot> acquire();
    void release(std::shared_ptr<EglSwapchainSlot> slot, FileDescriptor &&releaseFence);

    void resetBufferAge();

    static std::shared_ptr<EglSwapchain> create(GraphicsBufferAllocator *allocator, EglContext *context, GraphicsBufferOptions options);

private:
    GraphicsBufferAllocator *m_allocator;
    EglContext *m_context;
    GraphicsBufferOptions m_options;
    QList<std::shared_ptr<EglSwapchainSlot>> m_slots;
};

} // namespace KWin
