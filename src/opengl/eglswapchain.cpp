/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "opengl/eglswapchain.h"
#include "core/graphicsbuffer.h"
#include "core/graphicsbufferallocator.h"
#include "opengl/eglcontext.h"
#include "opengl/glutils.h"
#include "utils/common.h"

#include <drm_fourcc.h>
#include <errno.h>

namespace KWin
{

EglSwapchainSlot::EglSwapchainSlot(GraphicsBuffer *buffer, std::unique_ptr<GLFramebuffer> &&framebuffer, const std::shared_ptr<GLTexture> &texture)
    : m_buffer(buffer)
    , m_framebuffer(std::move(framebuffer))
    , m_texture(texture)
{
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

bool EglSwapchainSlot::isBusy() const
{
    return m_buffer->isReferenced() || (m_releaseFd.isValid() && !m_releaseFd.isReadable());
}

std::shared_ptr<EglSwapchainSlot> EglSwapchainSlot::create(EglContext *context, GraphicsBuffer *buffer)
{
    auto texture = context->importDmaBufAsTexture(*buffer->dmabufAttributes());
    if (!texture) {
        buffer->drop();
        return nullptr;
    }
    auto framebuffer = std::make_unique<GLFramebuffer>(texture.get());
    if (!framebuffer->valid()) {
        buffer->drop();
        return nullptr;
    }
    return std::make_shared<EglSwapchainSlot>(buffer, std::move(framebuffer), texture);
}

EglSwapchain::EglSwapchain(GraphicsBufferAllocator *allocator, EglContext *context, const QSize &size, uint32_t format, uint64_t modifier, const std::shared_ptr<EglSwapchainSlot> &seed)
    : m_allocator(allocator)
    , m_context(context)
    , m_size(size)
    , m_format(format)
    , m_modifier(modifier)
    , m_slots({seed})
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
    const auto it = std::ranges::find_if(std::as_const(m_slots), [](const auto &slot) {
        return !slot->isBusy();
    });
    if (it != m_slots.cend()) {
        return *it;
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

    auto slot = EglSwapchainSlot::create(m_context, buffer);
    if (!slot) {
        return nullptr;
    }
    m_slots.append(slot);
    return slot;
}

void EglSwapchain::release(std::shared_ptr<EglSwapchainSlot> slot, FileDescriptor &&releaseFence)
{
    slot->m_releaseFd = std::move(releaseFence);
    for (qsizetype i = 0; i < m_slots.count(); ++i) {
        if (m_slots[i] == slot) {
            m_slots[i]->m_age = 1;
        } else if (m_slots[i]->m_age > 0) {
            m_slots[i]->m_age++;
        }
    }
}

std::shared_ptr<EglSwapchain> EglSwapchain::create(GraphicsBufferAllocator *allocator, EglContext *context, const QSize &size, uint32_t format, const QList<uint64_t> &modifiers)
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
    const auto first = EglSwapchainSlot::create(context, seed);
    if (!first) {
        return nullptr;
    }
    return std::make_shared<EglSwapchain>(std::move(allocator),
                                          context,
                                          size,
                                          format,
                                          seed->dmabufAttributes()->modifier,
                                          first);
}

} // namespace KWin
