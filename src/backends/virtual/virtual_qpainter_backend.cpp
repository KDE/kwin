/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_qpainter_backend.h"
#include "core/drmdevice.h"
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
    : OutputLayer(output)
    , m_backend(backend)
{
}

VirtualQPainterLayer::~VirtualQPainterLayer()
{
}

std::optional<OutputLayerBeginFrameInfo> VirtualQPainterLayer::doBeginFrame()
{
    const QSize nativeSize(m_output->modeSize());
    if (!m_swapchain || m_swapchain->size() != nativeSize) {
        m_swapchain = std::make_unique<QPainterSwapchain>(m_backend->graphicsBufferAllocator(), nativeSize, DRM_FORMAT_XRGB8888);
    }

    m_current = m_swapchain->acquire();
    if (!m_current) {
        return std::nullopt;
    }

    m_renderTime = std::make_unique<CpuRenderTimeQuery>();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_current->view()->image()),
        .repaint = m_output->rect(),
    };
}

bool VirtualQPainterLayer::doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    m_renderTime->end();
    frame->addRenderTimeQuery(std::move(m_renderTime));
    return true;
}

QImage *VirtualQPainterLayer::image()
{
    return m_current->view()->image();
}

DrmDevice *VirtualQPainterLayer::scanoutDevice() const
{
    return m_backend->drmDevice();
}

QHash<uint32_t, QList<uint64_t>> VirtualQPainterLayer::supportedDrmFormats() const
{
    return {{DRM_FORMAT_ARGB8888, {DRM_FORMAT_MOD_LINEAR}}};
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

bool VirtualQPainterBackend::present(Output *output, const std::shared_ptr<OutputFrame> &frame)
{
    static_cast<VirtualOutput *>(output)->present(frame);
    return true;
}

VirtualQPainterLayer *VirtualQPainterBackend::primaryLayer(Output *output)
{
    return m_outputs[output].get();
}
}

#include "moc_virtual_qpainter_backend.cpp"
