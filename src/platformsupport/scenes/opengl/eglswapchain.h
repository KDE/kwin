/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "kwin_export.h"

#include <QSize>
#include <QVector>

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
    EglSwapchainSlot(EglContext *context, GraphicsBuffer *buffer);
    ~EglSwapchainSlot();

    GraphicsBuffer *buffer() const;
    std::shared_ptr<GLTexture> texture() const;
    GLFramebuffer *framebuffer() const;
    int age() const;

private:
    GraphicsBuffer *m_buffer;
    std::unique_ptr<GLFramebuffer> m_framebuffer;
    std::shared_ptr<GLTexture> m_texture;
    int m_age = 0;
    friend class EglSwapchain;
};

class KWIN_EXPORT EglSwapchain
{
public:
    EglSwapchain(GraphicsBufferAllocator *allocator, EglContext *context, const QSize &size, uint32_t format, uint64_t modifier, const QVector<std::shared_ptr<EglSwapchainSlot>> &slots);
    ~EglSwapchain();

    QSize size() const;
    uint32_t format() const;
    uint64_t modifier() const;

    std::shared_ptr<EglSwapchainSlot> acquire();
    void release(std::shared_ptr<EglSwapchainSlot> slot);

    static std::shared_ptr<EglSwapchain> create(GraphicsBufferAllocator *allocator, EglContext *context, const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers);

private:
    GraphicsBufferAllocator *m_allocator;
    EglContext *m_context;
    QSize m_size;
    uint32_t m_format;
    uint64_t m_modifier;
    QVector<std::shared_ptr<EglSwapchainSlot>> m_slots;
};

} // namespace KWin
