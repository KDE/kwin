/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "platformsupport/scenes/opengl/eglswapchain.h"
#include "core/graphicsbuffer.h"
#include "core/graphicsbufferallocator.h"
#include "libkwineffects/glutils.h"
#include "platformsupport/scenes/opengl/eglcontext.h"
#include "utils/common.h"

#include <errno.h>
#include <drm_fourcc.h>

namespace KWin
{

EglSwapchainSlot::EglSwapchainSlot(EglContext *context, GraphicsBuffer *buffer)
    : m_buffer(buffer)
{
    m_texture = context->importDmaBufAsTexture(*buffer->dmabufAttributes());
    if (m_texture) {
        m_framebuffer = std::make_unique<GLFramebuffer>(m_texture.get());
    }
}

EglSwapchainSlot::~EglSwapchainSlot()
{
    m_framebuffer.reset();
    m_texture.reset();
    m_buffer->drop();
}

GraphicsBuffer *EglSwapchainSlot::buffer() const
{
    return m_buffer;
}

std::shared_ptr<GLTexture> EglSwapchainSlot::texture() const
{
    return m_texture;
}

GLFramebuffer *EglSwapchainSlot::framebuffer() const
{
    return m_framebuffer.get();
}

int EglSwapchainSlot::age() const
{
    return m_age;
}

EglSwapchain::EglSwapchain(GraphicsBufferAllocator *allocator, EglContext *context, const QSize &size, uint32_t format, uint64_t modifier, const QVector<std::shared_ptr<EglSwapchainSlot>> &slots)
    : m_allocator(allocator)
    , m_context(context)
    , m_size(size)
    , m_format(format)
    , m_modifier(modifier)
    , m_slots(slots)
{
}

EglSwapchain::~EglSwapchain()
{
}

QSize EglSwapchain::size() const
{
    return m_size;
}

uint32_t EglSwapchain::format() const
{
    return m_format;
}

uint64_t EglSwapchain::modifier() const
{
    return m_modifier;
}

std::shared_ptr<EglSwapchainSlot> EglSwapchain::acquire()
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
        qCWarning(KWIN_OPENGL) << "Failed to allocate an egl gbm swapchain graphics buffer";
        return nullptr;
    }

    auto slot = std::make_shared<EglSwapchainSlot>(m_context, buffer);
    if (!slot->framebuffer()->valid()) {
        return nullptr;
    }
    m_slots.append(slot);
    return slot;
}

void EglSwapchain::release(std::shared_ptr<EglSwapchainSlot> slot)
{
    for (qsizetype i = 0; i < m_slots.count(); ++i) {
        if (m_slots[i] == slot) {
            m_slots[i]->m_age = 1;
        } else if (m_slots[i]->m_age > 0) {
            m_slots[i]->m_age++;
        }
    }
}

std::shared_ptr<EglSwapchain> EglSwapchain::create(GraphicsBufferAllocator *allocator, EglContext *context, const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers)
{
    if (!context->makeCurrent()) {
        return nullptr;
    }

    // The seed graphics buffer is used to fixate modifiers.
    GraphicsBuffer *seed = allocator->allocate(GraphicsBufferOptions{
        .size = size,
        .format = format,
        .modifiers = modifiers,
    });
    if (!seed) {
        return nullptr;
    }

    const QVector<std::shared_ptr<EglSwapchainSlot>> slots{std::make_shared<EglSwapchainSlot>(context, seed)};
    if (!slots.front()->framebuffer() || !slots.front()->framebuffer()->valid()) {
        return nullptr;
    }
    return std::make_shared<EglSwapchain>(std::move(allocator),
                                             context,
                                             size,
                                             format,
                                             seed->dmabufAttributes()->modifier,
                                             slots);
}

} // namespace KWin
