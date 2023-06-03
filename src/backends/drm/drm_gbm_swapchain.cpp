/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "backends/drm/drm_gbm_swapchain.h"
#include "backends/drm/drm_gpu.h"
#include "core/gbmgraphicsbufferallocator.h"
#include "libkwineffects/kwinglutils.h"
#include "platformsupport/scenes/opengl/eglcontext.h"

#include <errno.h>
#include <gbm.h>
#include <drm_fourcc.h>

namespace KWin
{

DrmEglSwapchainSlot::DrmEglSwapchainSlot(EglContext *context, GraphicsBuffer *buffer)
    : m_buffer(buffer)
{
    m_texture = context->importDmaBufAsTexture(*buffer->dmabufAttributes());
    m_framebuffer = std::make_unique<GLFramebuffer>(m_texture.get());
}

DrmEglSwapchainSlot::~DrmEglSwapchainSlot()
{
    m_framebuffer.reset();
    m_texture.reset();
    m_buffer->drop();
}

GraphicsBuffer *DrmEglSwapchainSlot::buffer() const
{
    return m_buffer;
}

std::shared_ptr<GLTexture> DrmEglSwapchainSlot::texture() const
{
    return m_texture;
}

GLFramebuffer *DrmEglSwapchainSlot::framebuffer() const
{
    return m_framebuffer.get();
}

int DrmEglSwapchainSlot::age() const
{
    return m_age;
}

DrmEglSwapchain::DrmEglSwapchain(std::unique_ptr<GraphicsBufferAllocator> allocator, EglContext *context, const QSize &size, uint32_t format, uint64_t modifier, const QVector<std::shared_ptr<DrmEglSwapchainSlot>> &slots)
    : m_allocator(std::move(allocator))
    , m_context(context)
    , m_size(size)
    , m_format(format)
    , m_modifier(modifier)
    , m_slots(slots)
{
}

DrmEglSwapchain::~DrmEglSwapchain()
{
}

QSize DrmEglSwapchain::size() const
{
    return m_size;
}

uint32_t DrmEglSwapchain::format() const
{
    return m_format;
}

uint64_t DrmEglSwapchain::modifier() const
{
    return m_modifier;
}

std::shared_ptr<DrmEglSwapchainSlot> DrmEglSwapchain::acquire()
{
    for (const auto &slot : std::as_const(m_slots)) {
        if (!slot->buffer()->isReferenced()) {
            return slot;
        }
    }

    GraphicsBuffer *buffer = m_allocator->allocate(GraphicsBufferOptions{
        .size = m_size,
        .format = m_format,
        .modifiers = {m_modifier},
    });
    if (!buffer) {
        qCWarning(KWIN_DRM) << "Failed to allocate a gbm graphics buffer";
        return nullptr;
    }

    auto slot = std::make_shared<DrmEglSwapchainSlot>(m_context, buffer);
    m_slots.append(slot);
    return slot;
}

void DrmEglSwapchain::release(std::shared_ptr<DrmEglSwapchainSlot> slot)
{
    for (qsizetype i = 0; i < m_slots.count(); ++i) {
        if (m_slots[i] == slot) {
            m_slots[i]->m_age = 1;
        } else if (m_slots[i]->m_age > 0) {
            m_slots[i]->m_age++;
        }
    }
}

std::shared_ptr<DrmEglSwapchain> DrmEglSwapchain::create(DrmGpu *gpu, EglContext *context, const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers)
{
    if (!context->makeCurrent()) {
        return nullptr;
    }

    auto allocator = std::make_unique<GbmGraphicsBufferAllocator>(gpu->gbmDevice());

    // The seed graphics buffer is used to fixate modifiers.
    GraphicsBuffer *seed = allocator->allocate(GraphicsBufferOptions{
        .size = size,
        .format = format,
        .modifiers = modifiers,
    });
    if (!seed) {
        return nullptr;
    }

    const QVector<std::shared_ptr<DrmEglSwapchainSlot>> slots{std::make_shared<DrmEglSwapchainSlot>(context, seed)};
    return std::make_shared<DrmEglSwapchain>(std::move(allocator),
                                             context,
                                             size,
                                             format,
                                             seed->dmabufAttributes()->modifier,
                                             slots);
}

} // namespace KWin
