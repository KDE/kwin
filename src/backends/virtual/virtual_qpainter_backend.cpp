/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_qpainter_backend.h"
#include "core/graphicsbufferview.h"
#include "core/shmgraphicsbufferallocator.h"
#include "platformsupport/scenes/qpainter/qpainterswapchain.h"
#include "utils/softwarevsyncmonitor.h"
#include "virtual_backend.h"
#include "virtual_output.h"

#include <drm_fourcc.h>

namespace KWin
{

VirtualQPainterLayer::VirtualQPainterLayer(Output *output, VirtualQPainterBackend *backend)
    : m_output(output)
    , m_backend(backend)
{
}

VirtualQPainterLayer::~VirtualQPainterLayer()
{
}

std::optional<OutputLayerBeginFrameInfo> VirtualQPainterLayer::beginFrame()
{
    const QSize nativeSize(m_output->modeSize());
    if (!m_swapchain || m_swapchain->size() != nativeSize) {
        m_swapchain = std::make_unique<QPainterSwapchain>(m_backend->graphicsBufferAllocator(), nativeSize, DRM_FORMAT_XRGB8888);
    }

    m_current = m_swapchain->acquire();
    if (!m_current) {
        return std::nullopt;
    }

    m_renderStart = std::chrono::steady_clock::now();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_current->view()->image()),
        .repaint = m_output->rect(),
    };
}

bool VirtualQPainterLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    m_renderTime = std::chrono::steady_clock::now() - m_renderStart;
    return true;
}

QImage *VirtualQPainterLayer::image()
{
    return m_current->view()->image();
}

quint32 VirtualQPainterLayer::format() const
{
    return DRM_FORMAT_XRGB8888;
}

std::chrono::nanoseconds VirtualQPainterLayer::queryRenderTime() const
{
    return m_renderTime;
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

GraphicsBufferAllocator *VirtualQPainterBackend::graphicsBufferAllocator() const
{
    return m_allocator.get();
}

void VirtualQPainterBackend::present(Output *output)
{
    static_cast<VirtualOutput *>(output)->vsyncMonitor()->arm();
}

VirtualQPainterLayer *VirtualQPainterBackend::primaryLayer(Output *output)
{
    return m_outputs[output].get();
}
}

#include "moc_virtual_qpainter_backend.cpp"
