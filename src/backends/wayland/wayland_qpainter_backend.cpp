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
#include "platformsupport/scenes/qpainter/qpainterswapchain.h"
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
    : m_waylandOutput(output)
    , m_backend(backend)
{
}

WaylandQPainterPrimaryLayer::~WaylandQPainterPrimaryLayer()
{
}

void WaylandQPainterPrimaryLayer::present()
{
    wl_buffer *buffer = m_waylandOutput->backend()->importBuffer(m_back->buffer());
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
        m_swapchain = std::make_unique<QPainterSwapchain>(m_backend->graphicsBufferAllocator(), nativeSize, DRM_FORMAT_XRGB8888);
    }

    m_back = m_swapchain->acquire();
    if (!m_back) {
        return std::nullopt;
    }

    m_renderStart = std::chrono::steady_clock::now();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_back->view()->image()),
        .repaint = accumulateDamage(m_back->age()),
    };
}

bool WaylandQPainterPrimaryLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    m_renderTime = std::chrono::steady_clock::now() - m_renderStart;
    m_damageJournal.add(damagedRegion);
    return true;
}

quint32 WaylandQPainterPrimaryLayer::format() const
{
    return DRM_FORMAT_RGBA8888;
}

std::chrono::nanoseconds WaylandQPainterPrimaryLayer::queryRenderTime() const
{
    return m_renderTime;
}

WaylandQPainterCursorLayer::WaylandQPainterCursorLayer(WaylandOutput *output, WaylandQPainterBackend *backend)
    : m_output(output)
    , m_backend(backend)
{
}

WaylandQPainterCursorLayer::~WaylandQPainterCursorLayer()
{
}

std::optional<OutputLayerBeginFrameInfo> WaylandQPainterCursorLayer::beginFrame()
{
    const auto tmp = size().expandedTo(QSize(64, 64));
    const QSize bufferSize(std::ceil(tmp.width()), std::ceil(tmp.height()));
    if (!m_swapchain || m_swapchain->size() != bufferSize) {
        m_swapchain = std::make_unique<QPainterSwapchain>(m_backend->graphicsBufferAllocator(), bufferSize, DRM_FORMAT_ARGB8888);
    }

    m_back = m_swapchain->acquire();
    if (!m_back) {
        return std::nullopt;
    }

    m_renderStart = std::chrono::steady_clock::now();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_back->view()->image()),
        .repaint = infiniteRegion(),
    };
}

bool WaylandQPainterCursorLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    m_renderTime = std::chrono::steady_clock::now() - m_renderStart;
    wl_buffer *buffer = m_output->backend()->importBuffer(m_back->buffer());
    Q_ASSERT(buffer);

    m_output->cursor()->update(buffer, scale(), hotspot().toPoint());
    m_swapchain->release(m_back);
    return true;
}

quint32 WaylandQPainterCursorLayer::format() const
{
    return DRM_FORMAT_RGBA8888;
}

std::chrono::nanoseconds WaylandQPainterCursorLayer::queryRenderTime() const
{
    return m_renderTime;
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

void WaylandQPainterBackend::present(Output *output, const std::shared_ptr<OutputFrame> &frame)
{
    m_outputs[output].primaryLayer->present();
    static_cast<WaylandOutput *>(output)->framePending(frame);
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
