/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "kwin_export.h"
#include "utils/filedescriptor.h"

#include <QList>
#include <QSize>

#include <cstdint>
#include <epoxy/egl.h>
#include <memory>

namespace KWin
{

class GraphicsBufferAllocator;
class GraphicsBuffer;
class GLFramebuffer;
class GLTexture;
class EglContext;

class KWIN_EXPORT EglSwapchainSlot
{
public:
    EglSwapchainSlot(GraphicsBuffer *buffer, std::unique_ptr<GLFramebuffer> &&framebuffer, const std::shared_ptr<GLTexture> &texture);
    ~EglSwapchainSlot();

    GraphicsBuffer *buffer() const;
    std::shared_ptr<GLTexture> texture() const;
    GLFramebuffer *framebuffer() const;
    int age() const;

    static std::shared_ptr<EglSwapchainSlot> create(EglContext *context, GraphicsBuffer *buffer);

private:
    bool isBusy() const;

    GraphicsBuffer *m_buffer;
    std::unique_ptr<GLFramebuffer> m_framebuffer;
    std::shared_ptr<GLTexture> m_texture;
    int m_age = 0;
    FileDescriptor m_releaseFd;
    friend class EglSwapchain;
};

class KWIN_EXPORT EglSwapchain
{
public:
    EglSwapchain(GraphicsBufferAllocator *allocator, EglContext *context, const QSize &size, uint32_t format, uint64_t modifier, const std::shared_ptr<EglSwapchainSlot> &seed);
    ~EglSwapchain();

    QSize size() const;
    uint32_t format() const;
    uint64_t modifier() const;

    std::shared_ptr<EglSwapchainSlot> acquire();
    void release(std::shared_ptr<EglSwapchainSlot> slot, FileDescriptor &&releaseFence);

    static std::shared_ptr<EglSwapchain> create(GraphicsBufferAllocator *allocator, EglContext *context, const QSize &size, uint32_t format, const QList<uint64_t> &modifiers);

private:
    GraphicsBufferAllocator *m_allocator;
    EglContext *m_context;
    QSize m_size;
    uint32_t m_format;
    uint64_t m_modifier;
    QList<std::shared_ptr<EglSwapchainSlot>> m_slots;
};

} // namespace KWin
