/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2013, 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "wayland_qpainter_backend.h"
#include "core/shmgraphicsbufferallocator.h"
#include "wayland_backend.h"
#include "wayland_logging.h"
#include "wayland_output.h"

#include <KWayland/Client/surface.h>

#include <cmath>
#include <drm_fourcc.h>
#include <sys/mman.h>
#include <wayland-client-protocol.h>

namespace KWin
{
namespace Wayland
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

WaylandQPainterBufferSlot::WaylandQPainterBufferSlot(ShmGraphicsBuffer *graphicsBuffer)
    : graphicsBuffer(graphicsBuffer)
{
    const ShmAttributes *attributes = graphicsBuffer->shmAttributes();
    size = attributes->size.height() * attributes->stride;

    data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, attributes->fd.get(), attributes->offset);
    if (data == MAP_FAILED) {
        qCWarning(KWIN_WAYLAND_BACKEND) << "Failed to map a shared memory buffer";
        return;
    }

    image = QImage(static_cast<uchar *>(data), attributes->size.width(), attributes->size.height(), drmFormatToQImageFormat(attributes->format));
}

WaylandQPainterBufferSlot::~WaylandQPainterBufferSlot()
{
    if (data) {
        munmap(data, size);
    }

    graphicsBuffer->drop();
}

WaylandQPainterSwapchain::WaylandQPainterSwapchain(const QSize &size, uint32_t format)
    : m_allocator(std::make_unique<ShmGraphicsBufferAllocator>())
    , m_size(size)
    , m_format(format)
{
}

QSize WaylandQPainterSwapchain::size() const
{
    return m_size;
}

std::shared_ptr<WaylandQPainterBufferSlot> WaylandQPainterSwapchain::acquire()
{
    for (const auto &slot : m_slots) {
        if (!slot->graphicsBuffer->isReferenced()) {
            return slot;
        }
    }

    ShmGraphicsBuffer *buffer = m_allocator->allocate(GraphicsBufferOptions{
        .size = m_size,
        .format = m_format,
        .software = true,
    });
    if (!buffer) {
        qCDebug(KWIN_WAYLAND_BACKEND) << "Did not get a new Buffer from Shm Pool";
        return nullptr;
    }

    auto slot = std::make_shared<WaylandQPainterBufferSlot>(buffer);
    m_slots.push_back(slot);

    return slot;
}

void WaylandQPainterSwapchain::release(std::shared_ptr<WaylandQPainterBufferSlot> buffer)
{
    for (const auto &slot : m_slots) {
        if (slot == buffer) {
            slot->age = 1;
        } else if (slot->age > 0) {
            slot->age++;
        }
    }
}

WaylandQPainterPrimaryLayer::WaylandQPainterPrimaryLayer(WaylandOutput *output)
    : m_waylandOutput(output)
{
}

void WaylandQPainterPrimaryLayer::present()
{
    wl_buffer *buffer = m_waylandOutput->backend()->importBuffer(m_back->graphicsBuffer);
    Q_ASSERT(buffer);

    auto s = m_waylandOutput->surface();
    s->attachBuffer(buffer);
    s->damage(m_damageJournal.lastDamage());
    s->setScale(std::ceil(m_waylandOutput->scale()));
    s->commit();

    m_swapchain->release(m_back);
}

QRegion WaylandQPainterPrimaryLayer::accumulateDamage(int bufferAge) const
{
    return m_damageJournal.accumulate(bufferAge, infiniteRegion());
}

std::optional<OutputLayerBeginFrameInfo> WaylandQPainterPrimaryLayer::beginFrame()
{
    const QSize nativeSize(m_waylandOutput->modeSize());
    if (!m_swapchain || m_swapchain->size() != nativeSize) {
        m_swapchain = std::make_unique<WaylandQPainterSwapchain>(nativeSize, DRM_FORMAT_XRGB8888);
    }

    m_back = m_swapchain->acquire();
    if (!m_back) {
        return std::nullopt;
    }

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(&m_back->image),
        .repaint = accumulateDamage(m_back->age),
    };
}

bool WaylandQPainterPrimaryLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    m_damageJournal.add(damagedRegion);
    return true;
}

quint32 WaylandQPainterPrimaryLayer::format() const
{
    return DRM_FORMAT_RGBA8888;
}

WaylandQPainterCursorLayer::WaylandQPainterCursorLayer(WaylandOutput *output)
    : m_output(output)
{
}

std::optional<OutputLayerBeginFrameInfo> WaylandQPainterCursorLayer::beginFrame()
{
    const auto tmp = size().expandedTo(QSize(64, 64));
    const QSize bufferSize(std::ceil(tmp.width()), std::ceil(tmp.height()));
    if (!m_swapchain || m_swapchain->size() != bufferSize) {
        m_swapchain = std::make_unique<WaylandQPainterSwapchain>(bufferSize, DRM_FORMAT_ARGB8888);
    }

    m_back = m_swapchain->acquire();
    if (!m_back) {
        return std::nullopt;
    }

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(&m_back->image),
        .repaint = infiniteRegion(),
    };
}

bool WaylandQPainterCursorLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    wl_buffer *buffer = m_output->backend()->importBuffer(m_back->graphicsBuffer);
    Q_ASSERT(buffer);

    m_output->cursor()->update(buffer, scale(), hotspot().toPoint());
    m_swapchain->release(m_back);
    return true;
}

quint32 WaylandQPainterCursorLayer::format() const
{
    return DRM_FORMAT_RGBA8888;
}

WaylandQPainterBackend::WaylandQPainterBackend(Wayland::WaylandBackend *b)
    : QPainterBackend()
    , m_backend(b)
{

    const auto waylandOutputs = m_backend->waylandOutputs();
    for (auto *output : waylandOutputs) {
        createOutput(output);
    }
    connect(m_backend, &WaylandBackend::outputAdded, this, &WaylandQPainterBackend::createOutput);
    connect(m_backend, &WaylandBackend::outputRemoved, this, [this](Output *waylandOutput) {
        m_outputs.erase(waylandOutput);
    });
}

WaylandQPainterBackend::~WaylandQPainterBackend()
{
}

void WaylandQPainterBackend::createOutput(Output *waylandOutput)
{
    m_outputs[waylandOutput] = Layers{
        .primaryLayer = std::make_unique<WaylandQPainterPrimaryLayer>(static_cast<WaylandOutput *>(waylandOutput)),
        .cursorLayer = std::make_unique<WaylandQPainterCursorLayer>(static_cast<WaylandOutput *>(waylandOutput)),
    };
}

void WaylandQPainterBackend::present(Output *output)
{
    m_outputs[output].primaryLayer->present();
}

OutputLayer *WaylandQPainterBackend::primaryLayer(Output *output)
{
    return m_outputs[output].primaryLayer.get();
}

OutputLayer *WaylandQPainterBackend::cursorLayer(Output *output)
{
    return m_outputs[output].cursorLayer.get();
}

}
}
