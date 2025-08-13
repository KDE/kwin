/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_windowed_qpainter_backend.h"
#include "core/graphicsbufferview.h"
#include "core/shmgraphicsbufferallocator.h"
#include "qpainter/qpainterswapchain.h"
#include "x11_windowed_backend.h"
#include "x11_windowed_output.h"

#include <cerrno>
#include <cmath>
#include <drm_fourcc.h>
#include <string.h>
#include <xcb/present.h>

namespace KWin
{

X11WindowedQPainterPrimaryLayer::X11WindowedQPainterPrimaryLayer(X11WindowedOutput *output, X11WindowedQPainterBackend *backend)
    : OutputLayer(output, OutputLayerType::Primary)
    , m_output(output)
    , m_backend(backend)
{
}

X11WindowedQPainterPrimaryLayer::~X11WindowedQPainterPrimaryLayer()
{
}

std::optional<OutputLayerBeginFrameInfo> X11WindowedQPainterPrimaryLayer::doBeginFrame()
{
    const QSize bufferSize = m_output->modeSize();
    if (!m_swapchain || m_swapchain->size() != bufferSize) {
        m_swapchain = std::make_unique<QPainterSwapchain>(m_backend->graphicsBufferAllocator(), bufferSize, m_output->backend()->driFormatForDepth(m_output->depth()));
    }

    m_current = m_swapchain->acquire();
    if (!m_current) {
        return std::nullopt;
    }

    QRegion repaint = infiniteRegion();
    m_output->clearExposedArea();

    m_renderTime = std::make_unique<CpuRenderTimeQuery>();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_current->view()->image()),
        .repaint = repaint,
    };
}

bool X11WindowedQPainterPrimaryLayer::doEndFrame(const QRegion &renderedDeviceRegion, const QRegion &damagedDeviceRegion, OutputFrame *frame)
{
    m_renderTime->end();
    frame->addRenderTimeQuery(std::move(m_renderTime));
    m_output->setPrimaryBuffer(m_current->buffer());
    return true;
}

DrmDevice *X11WindowedQPainterPrimaryLayer::scanoutDevice() const
{
    return m_backend->drmDevice();
}

QHash<uint32_t, QList<uint64_t>> X11WindowedQPainterPrimaryLayer::supportedDrmFormats() const
{
    return m_backend->supportedFormats();
}

void X11WindowedQPainterPrimaryLayer::releaseBuffers()
{
    m_current.reset();
    m_swapchain.reset();
}

X11WindowedQPainterCursorLayer::X11WindowedQPainterCursorLayer(X11WindowedOutput *output)
    : OutputLayer(output, OutputLayerType::CursorOnly)
    , m_output(output)
{
}

std::optional<OutputLayerBeginFrameInfo> X11WindowedQPainterCursorLayer::doBeginFrame()
{
    const auto bufferSize = targetRect().size();
    if (m_buffer.size() != bufferSize) {
        m_buffer = QImage(bufferSize, QImage::Format_ARGB32_Premultiplied);
    }

    m_renderTime = std::make_unique<CpuRenderTimeQuery>();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(&m_buffer),
        .repaint = infiniteRegion(),
    };
}

bool X11WindowedQPainterCursorLayer::doEndFrame(const QRegion &renderedDeviceRegion, const QRegion &damagedDeviceRegion, OutputFrame *frame)
{
    m_renderTime->end();
    if (frame) {
        frame->addRenderTimeQuery(std::move(m_renderTime));
    }
    m_output->cursor()->update(m_buffer, hotspot());
    return true;
}

DrmDevice *X11WindowedQPainterCursorLayer::scanoutDevice() const
{
    return nullptr;
}

QHash<uint32_t, QList<uint64_t>> X11WindowedQPainterCursorLayer::supportedDrmFormats() const
{
    return {{DRM_FORMAT_ARGB8888, {DRM_FORMAT_MOD_LINEAR}}};
}

void X11WindowedQPainterCursorLayer::releaseBuffers()
{
}

X11WindowedQPainterBackend::X11WindowedQPainterBackend(X11WindowedBackend *backend)
    : QPainterBackend()
    , m_backend(backend)
    , m_allocator(std::make_unique<ShmGraphicsBufferAllocator>())
{
    const auto outputs = m_backend->outputs();
    for (BackendOutput *output : outputs) {
        addOutput(output);
    }

    connect(backend, &X11WindowedBackend::outputAdded, this, &X11WindowedQPainterBackend::addOutput);
}

X11WindowedQPainterBackend::~X11WindowedQPainterBackend()
{
    const auto outputs = m_backend->outputs();
    for (BackendOutput *output : outputs) {
        static_cast<X11WindowedOutput *>(output)->setOutputLayers({});
    }
}

void X11WindowedQPainterBackend::addOutput(BackendOutput *output)
{
    X11WindowedOutput *x11Output = static_cast<X11WindowedOutput *>(output);
    std::vector<std::unique_ptr<OutputLayer>> layers;
    layers.push_back(std::make_unique<X11WindowedQPainterPrimaryLayer>(x11Output, this));
    layers.push_back(std::make_unique<X11WindowedQPainterCursorLayer>(x11Output));
    x11Output->setOutputLayers(std::move(layers));
}

GraphicsBufferAllocator *X11WindowedQPainterBackend::graphicsBufferAllocator() const
{
    return m_allocator.get();
}

QList<OutputLayer *> X11WindowedQPainterBackend::compatibleOutputLayers(BackendOutput *output)
{
    return static_cast<X11WindowedOutput *>(output)->outputLayers();
}

}

#include "moc_x11_windowed_qpainter_backend.cpp"
