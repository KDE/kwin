/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2013, 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "wayland_qpainter_backend.h"
#include "core/graphicsbufferview.h"
#include "core/shmgraphicsbufferallocator.h"
#include "qpainter/qpainterswapchain.h"
#include "wayland_backend.h"
#include "wayland_output.h"

#include <KWayland/Client/surface.h>

#include <cmath>
#include <drm_fourcc.h>
#include <wayland-client-protocol.h>

namespace KWin
{
namespace Wayland
{

WaylandQPainterPrimaryLayer::WaylandQPainterPrimaryLayer(WaylandOutput *output, WaylandQPainterBackend *backend)
    : OutputLayer(output)
    , m_waylandOutput(output)
    , m_backend(backend)
{
}

WaylandQPainterPrimaryLayer::~WaylandQPainterPrimaryLayer()
{
}

QRegion WaylandQPainterPrimaryLayer::accumulateDamage(int bufferAge) const
{
    return m_damageJournal.accumulate(bufferAge, infiniteRegion());
}

std::optional<OutputLayerBeginFrameInfo> WaylandQPainterPrimaryLayer::doBeginFrame()
{
    const QSize nativeSize(m_waylandOutput->modeSize());
    if (!m_swapchain || m_swapchain->size() != nativeSize) {
        m_swapchain = std::make_unique<QPainterSwapchain>(m_backend->graphicsBufferAllocator(), nativeSize, DRM_FORMAT_XRGB8888);
    }

    m_back = m_swapchain->acquire();
    if (!m_back) {
        return std::nullopt;
    }

    m_renderTime = std::make_unique<CpuRenderTimeQuery>();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_back->view()->image(), m_output->colorDescription()),
        .repaint = accumulateDamage(m_back->age()),
    };
}

bool WaylandQPainterPrimaryLayer::doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    m_renderTime->end();
    frame->addRenderTimeQuery(std::move(m_renderTime));
    m_damageJournal.add(damagedRegion);
    m_waylandOutput->setPrimaryBuffer(m_waylandOutput->backend()->importBuffer(m_back->buffer()));
    m_swapchain->release(m_back);
    return true;
}

DrmDevice *WaylandQPainterPrimaryLayer::scanoutDevice() const
{
    return m_backend->drmDevice();
}

QHash<uint32_t, QList<uint64_t>> WaylandQPainterPrimaryLayer::supportedDrmFormats() const
{
    return {{DRM_FORMAT_ARGB8888, {DRM_FORMAT_MOD_LINEAR}}};
}

WaylandQPainterCursorLayer::WaylandQPainterCursorLayer(WaylandOutput *output, WaylandQPainterBackend *backend)
    : OutputLayer(output)
    , m_backend(backend)
{
}

WaylandQPainterCursorLayer::~WaylandQPainterCursorLayer()
{
}

std::optional<OutputLayerBeginFrameInfo> WaylandQPainterCursorLayer::doBeginFrame()
{
    const auto bufferSize = targetRect().size();
    if (!m_swapchain || m_swapchain->size() != bufferSize) {
        m_swapchain = std::make_unique<QPainterSwapchain>(m_backend->graphicsBufferAllocator(), bufferSize, DRM_FORMAT_ARGB8888);
    }

    m_back = m_swapchain->acquire();
    if (!m_back) {
        return std::nullopt;
    }

    m_renderTime = std::make_unique<CpuRenderTimeQuery>();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_back->view()->image()),
        .repaint = infiniteRegion(),
    };
}

bool WaylandQPainterCursorLayer::doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    if (frame) {
        frame->addRenderTimeQuery(std::move(m_renderTime));
    }
    wl_buffer *buffer = static_cast<WaylandOutput *>(m_output.get())->backend()->importBuffer(m_back->buffer());
    Q_ASSERT(buffer);

    static_cast<WaylandOutput *>(m_output.get())->cursor()->update(buffer, m_back->buffer()->size() / m_output->scale(), hotspot().toPoint());
    m_swapchain->release(m_back);
    return true;
}

DrmDevice *WaylandQPainterCursorLayer::scanoutDevice() const
{
    return m_backend->drmDevice();
}

QHash<uint32_t, QList<uint64_t>> WaylandQPainterCursorLayer::supportedDrmFormats() const
{
    return {{DRM_FORMAT_ARGB8888, {DRM_FORMAT_MOD_LINEAR}}};
}

WaylandQPainterBackend::WaylandQPainterBackend(Wayland::WaylandBackend *b)
    : QPainterBackend()
    , m_backend(b)
    , m_allocator(std::make_unique<ShmGraphicsBufferAllocator>())
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
        .primaryLayer = std::make_unique<WaylandQPainterPrimaryLayer>(static_cast<WaylandOutput *>(waylandOutput), this),
        .cursorLayer = std::make_unique<WaylandQPainterCursorLayer>(static_cast<WaylandOutput *>(waylandOutput), this),
    };
}

GraphicsBufferAllocator *WaylandQPainterBackend::graphicsBufferAllocator() const
{
    return m_allocator.get();
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

#include "moc_wayland_qpainter_backend.cpp"
