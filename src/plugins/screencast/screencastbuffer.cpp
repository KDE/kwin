/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "screencastbuffer.h"
#include "compositor.h"
#include "core/drmdevice.h"
#include "core/shmgraphicsbufferallocator.h"
#include "opengl/glframebuffer.h"
#include "platformsupport/scenes/opengl/abstract_egl_backend.h"

namespace KWin
{

ScreenCastBuffer::ScreenCastBuffer(GraphicsBuffer *buffer)
    : m_buffer(buffer)
{
}

ScreenCastBuffer::~ScreenCastBuffer()
{
    m_buffer->drop();
}

DmaBufScreenCastBuffer::DmaBufScreenCastBuffer(GraphicsBuffer *buffer, std::shared_ptr<GLTexture> &&texture, std::unique_ptr<GLFramebuffer> &&framebuffer)
    : ScreenCastBuffer(buffer)
    , texture(std::move(texture))
    , framebuffer(std::move(framebuffer))
{
}

DmaBufScreenCastBuffer *DmaBufScreenCastBuffer::create(pw_buffer *pwBuffer, const GraphicsBufferOptions &options)
{
    AbstractEglBackend *backend = dynamic_cast<AbstractEglBackend *>(Compositor::self()->backend());
    if (!backend || !backend->drmDevice()) {
        return nullptr;
    }

    GraphicsBuffer *buffer = backend->drmDevice()->allocator()->allocate(options);
    if (!buffer) {
        return nullptr;
    }

    const DmaBufAttributes *attrs = buffer->dmabufAttributes();
    if (!attrs) {
        buffer->drop();
        return nullptr;
    }

    if (pwBuffer->buffer->n_datas != uint32_t(attrs->planeCount)) {
        buffer->drop();
        return nullptr;
    }

    backend->makeCurrent();

    auto texture = backend->importDmaBufAsTexture(*attrs);
    if (!texture) {
        buffer->drop();
        return nullptr;
    }

    auto framebuffer = std::make_unique<GLFramebuffer>(texture.get());
    if (!framebuffer->valid()) {
        buffer->drop();
        return nullptr;
    }

    struct spa_data *spaData = pwBuffer->buffer->datas;
    for (int i = 0; i < attrs->planeCount; ++i) {
        spaData[i].type = SPA_DATA_DmaBuf;
        spaData[i].flags = SPA_DATA_FLAG_READWRITE;
        spaData[i].mapoffset = 0;
        spaData[i].maxsize = i == 0 ? attrs->pitch[i] * attrs->height : 0; // TODO: dmabufs don't have a well defined size, it should be zero but some clients check the size to see if the buffer is valid
        spaData[i].fd = attrs->fd[i].get();
        spaData[i].data = nullptr;
        spaData[i].chunk->offset = attrs->offset[i];
        spaData[i].chunk->size = spaData[i].maxsize;
        spaData[i].chunk->stride = attrs->pitch[i];
        spaData[i].chunk->flags = SPA_CHUNK_FLAG_NONE;
    };

    return new DmaBufScreenCastBuffer(buffer, std::move(texture), std::move(framebuffer));
}

MemFdScreenCastBuffer::MemFdScreenCastBuffer(GraphicsBuffer *buffer, GraphicsBufferView &&view)
    : ScreenCastBuffer(buffer)
    , view(std::move(view))
{
}

MemFdScreenCastBuffer *MemFdScreenCastBuffer::create(pw_buffer *pwBuffer, const GraphicsBufferOptions &options)
{
    GraphicsBuffer *buffer = ShmGraphicsBufferAllocator().allocate(options);
    if (!buffer) {
        return nullptr;
    }

    GraphicsBufferView view(buffer, GraphicsBuffer::Read | GraphicsBuffer::Write);
    if (view.isNull()) {
        buffer->drop();
        return nullptr;
    }

    const ShmAttributes *attributes = buffer->shmAttributes();

    struct spa_data *spaData = pwBuffer->buffer->datas;
    spaData->type = SPA_DATA_MemFd;
    spaData->flags = SPA_DATA_FLAG_READWRITE;
    spaData->mapoffset = 0;
    spaData->maxsize = attributes->stride * attributes->size.height();
    spaData->fd = attributes->fd.get();
    spaData->data = nullptr;
    spaData->chunk->offset = 0;
    spaData->chunk->size = spaData->maxsize;
    spaData->chunk->stride = attributes->stride;
    spaData->chunk->flags = SPA_CHUNK_FLAG_NONE;

    return new MemFdScreenCastBuffer(buffer, std::move(view));
}

} // namespace KWin
