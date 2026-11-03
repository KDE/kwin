/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "screencastbuffer.h"
#include "compositor.h"
#include "core/drmdevice.h"
#include "core/renderdevice.h"
#include "core/shmgraphicsbufferallocator.h"
#include "main.h"
#include "scene/itemrenderer.h"
#include "scene/workspacescene.h"

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

DmaBufScreenCastBuffer::DmaBufScreenCastBuffer(GraphicsBuffer *buffer, std::unique_ptr<SyncTimeline> &&synctimeline)
    : ScreenCastBuffer(buffer)
    , synctimeline(std::move(synctimeline))
{
}

DmaBufScreenCastBuffer *DmaBufScreenCastBuffer::create(RenderDevice *renderDevice, pw_buffer *pwBuffer, const GraphicsBufferOptions &options)
{
    GraphicsBuffer *buffer = renderDevice->allocator()->allocate(options);
    if (!buffer) {
        return nullptr;
    }

    const DmaBufAttributes *attrs = buffer->dmabufAttributes();
    if (!attrs) {
        buffer->drop();
        return nullptr;
    }

    const void *syncTimelineMeta = spa_buffer_find_meta_data(pwBuffer->buffer, SPA_META_SyncTimeline, sizeof(spa_meta_sync_timeline));
    if (pwBuffer->buffer->n_datas != uint32_t(attrs->planeCount + (syncTimelineMeta ? 2 : 0))) {
        buffer->drop();
        return nullptr;
    }

    ItemRenderer *renderer = kwinApp()->scene()->renderer(renderDevice);
    if (!renderer->testRenderTargetImport(buffer)) {
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

    std::unique_ptr<SyncTimeline> synctimeline;
    if (syncTimelineMeta) {
        synctimeline = std::make_unique<SyncTimeline>(renderDevice->drmDevice()->fileDescriptor());
        const FileDescriptor &syncobjfd = synctimeline->fileDescriptor();
        if (!syncobjfd.isValid()) {
            buffer->drop();
            return nullptr;
        }

        // Signal the first timeline point, so the very first recording can proceed.
        synctimeline->signal(0);

        spa_data &acquireData = spaData[attrs->planeCount];
        acquireData.type = SPA_DATA_SyncObj;
        acquireData.flags = SPA_DATA_FLAG_READABLE;
        acquireData.fd = syncobjfd.get();

        spa_data &releaseData = spaData[attrs->planeCount + 1];
        releaseData.type = SPA_DATA_SyncObj;
        releaseData.flags = SPA_DATA_FLAG_READABLE;
        releaseData.fd = syncobjfd.get();
    }

    return new DmaBufScreenCastBuffer(buffer, std::move(synctimeline));
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
