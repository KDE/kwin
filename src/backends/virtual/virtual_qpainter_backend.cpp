/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_qpainter_backend.h"
#include "core/shmgraphicsbufferallocator.h"
#include "utils/softwarevsyncmonitor.h"
#include "virtual_backend.h"
#include "virtual_logging.h"
#include "virtual_output.h"

#include <QPainter>

#include <drm_fourcc.h>
#include <sys/mman.h>

namespace KWin
{

static QImage::Format drmFormatToQImageFormat(uint32_t drmFormat)
{
    switch (drmFormat) {
    case DRM_FORMAT_ARGB8888:
        return QImage::Format_ARGB32;
    case DRM_FORMAT_XRGB8888:
        return QImage::Format_RGB32;
    default:
        Q_UNREACHABLE();
    }
}

VirtualQPainterBufferSlot::VirtualQPainterBufferSlot(GraphicsBuffer *graphicsBuffer)
    : graphicsBuffer(graphicsBuffer)
{
    const ShmAttributes *attributes = graphicsBuffer->shmAttributes();
    size = attributes->size.height() * attributes->stride;

    data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, attributes->fd.get(), 0);
    if (data == MAP_FAILED) {
        qCWarning(KWIN_VIRTUAL) << "Failed to map a shared memory buffer";
        return;
    }

    image = QImage(static_cast<uchar *>(data), attributes->size.width(), attributes->size.height(), drmFormatToQImageFormat(attributes->format));
}

VirtualQPainterBufferSlot::~VirtualQPainterBufferSlot()
{
    if (data) {
        munmap(data, size);
    }

    graphicsBuffer->drop();
}

VirtualQPainterSwapchain::VirtualQPainterSwapchain(const QSize &size, uint32_t format, VirtualQPainterBackend *backend)
    : m_backend(backend)
    , m_size(size)
    , m_format(format)
{
}

QSize VirtualQPainterSwapchain::size() const
{
    return m_size;
}

std::shared_ptr<VirtualQPainterBufferSlot> VirtualQPainterSwapchain::acquire()
{
    for (const auto &slot : m_slots) {
        if (!slot->graphicsBuffer->isReferenced()) {
            return slot;
        }
    }

    GraphicsBuffer *buffer = m_backend->graphicsBufferAllocator()->allocate(m_size, m_format);
    if (!buffer) {
        qCDebug(KWIN_VIRTUAL) << "Did not get a new Buffer from Shm Pool";
        return nullptr;
    }

    auto slot = std::make_shared<VirtualQPainterBufferSlot>(buffer);
    m_slots.push_back(slot);

    return slot;
}

VirtualQPainterLayer::VirtualQPainterLayer(Output *output, VirtualQPainterBackend *backend)
    : m_backend(backend)
    , m_output(output)
{
}

std::optional<OutputLayerBeginFrameInfo> VirtualQPainterLayer::beginFrame()
{
    const QSize nativeSize(m_output->modeSize());
    if (!m_swapchain || m_swapchain->size() != nativeSize) {
        m_swapchain = std::make_unique<VirtualQPainterSwapchain>(nativeSize, DRM_FORMAT_XRGB8888, m_backend);
    }

    m_current = m_swapchain->acquire();
    if (!m_current) {
        return std::nullopt;
    }

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(&m_current->image),
        .repaint = m_output->rect(),
    };
}

bool VirtualQPainterLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    return true;
}

QImage *VirtualQPainterLayer::image()
{
    return &m_current->image;
}

quint32 VirtualQPainterLayer::format() const
{
    return DRM_FORMAT_XRGB8888;
}

VirtualQPainterBackend::VirtualQPainterBackend(VirtualBackend *backend)
    : m_allocator(std::make_unique<ShmGraphicsBufferAllocator>())
{
    connect(backend, &VirtualBackend::outputAdded, this, &VirtualQPainterBackend::addOutput);
    connect(backend, &VirtualBackend::outputRemoved, this, &VirtualQPainterBackend::removeOutput);

    const auto outputs = backend->outputs();
    for (Output *output : outputs) {
        addOutput(output);
    }
}

VirtualQPainterBackend::~VirtualQPainterBackend() = default;

void VirtualQPainterBackend::addOutput(Output *output)
{
    m_outputs[output] = std::make_unique<VirtualQPainterLayer>(output, this);
}

void VirtualQPainterBackend::removeOutput(Output *output)
{
    m_outputs.erase(output);
}

void VirtualQPainterBackend::present(Output *output)
{
    static_cast<VirtualOutput *>(output)->vsyncMonitor()->arm();
}

VirtualQPainterLayer *VirtualQPainterBackend::primaryLayer(Output *output)
{
    return m_outputs[output].get();
}

GraphicsBufferAllocator *VirtualQPainterBackend::graphicsBufferAllocator() const
{
    return m_allocator.get();
}

}
