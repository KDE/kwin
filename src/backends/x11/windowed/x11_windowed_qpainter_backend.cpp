/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_windowed_qpainter_backend.h"
#include "core/shmgraphicsbufferallocator.h"
#include "x11_windowed_backend.h"
#include "x11_windowed_logging.h"
#include "x11_windowed_output.h"

#include <cerrno>
#include <cmath>
#include <drm_fourcc.h>
#include <string.h>
#include <sys/mman.h>
#include <xcb/present.h>

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

X11WindowedQPainterLayerBuffer::X11WindowedQPainterLayerBuffer(ShmGraphicsBuffer *buffer, X11WindowedOutput *output)
    : m_graphicsBuffer(buffer)
{
    const ShmAttributes *attributes = buffer->shmAttributes();

    m_size = attributes->size.height() * attributes->stride;
    m_data = mmap(nullptr, m_size, PROT_READ | PROT_WRITE, MAP_SHARED, attributes->fd.get(), 0);
    if (m_data == MAP_FAILED) {
        qCWarning(KWIN_X11WINDOWED) << "Failed to map a shared memory buffer";
        return;
    }

    m_view = std::make_unique<QImage>(static_cast<uchar *>(m_data), attributes->size.width(), attributes->size.height(), drmFormatToQImageFormat(attributes->format));
}

X11WindowedQPainterLayerBuffer::~X11WindowedQPainterLayerBuffer()
{
    if (m_data) {
        munmap(m_data, m_size);
    }
    m_view.reset();
    m_graphicsBuffer->drop();
}

ShmGraphicsBuffer *X11WindowedQPainterLayerBuffer::graphicsBuffer() const
{
    return m_graphicsBuffer;
}

QImage *X11WindowedQPainterLayerBuffer::view() const
{
    return m_view.get();
}

X11WindowedQPainterLayerSwapchain::X11WindowedQPainterLayerSwapchain(const QSize &size, uint32_t format, X11WindowedOutput *output)
    : m_output(output)
    , m_size(size)
    , m_format(format)
    , m_allocator(std::make_unique<ShmGraphicsBufferAllocator>())
{
}

QSize X11WindowedQPainterLayerSwapchain::size() const
{
    return m_size;
}

std::shared_ptr<X11WindowedQPainterLayerBuffer> X11WindowedQPainterLayerSwapchain::acquire()
{
    for (const auto &buffer : m_buffers) {
        if (!buffer->graphicsBuffer()->isReferenced()) {
            return buffer;
        }
    }

    ShmGraphicsBuffer *graphicsBuffer = m_allocator->allocate(m_size, m_format);
    if (!graphicsBuffer) {
        qCWarning(KWIN_X11WINDOWED) << "Failed to allocate a shared memory graphics buffer";
        return nullptr;
    }

    auto buffer = std::make_shared<X11WindowedQPainterLayerBuffer>(graphicsBuffer, m_output);
    m_buffers.push_back(buffer);

    return buffer;
}

X11WindowedQPainterPrimaryLayer::X11WindowedQPainterPrimaryLayer(X11WindowedOutput *output)
    : m_output(output)
{
}

std::optional<OutputLayerBeginFrameInfo> X11WindowedQPainterPrimaryLayer::beginFrame()
{
    const QSize bufferSize = m_output->modeSize();
    if (!m_swapchain || m_swapchain->size() != bufferSize) {
        m_swapchain = std::make_unique<X11WindowedQPainterLayerSwapchain>(bufferSize, m_output->backend()->driFormatForDepth(m_output->depth()), m_output);
    }

    m_current = m_swapchain->acquire();
    if (!m_current) {
        return std::nullopt;
    }

    QRegion repaint = m_output->exposedArea() + m_output->rect();
    m_output->clearExposedArea();

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_current->view()),
        .repaint = repaint,
    };
}

bool X11WindowedQPainterPrimaryLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    return true;
}

void X11WindowedQPainterPrimaryLayer::present()
{
    xcb_pixmap_t pixmap = m_output->importBuffer(m_current->graphicsBuffer());
    Q_ASSERT(pixmap != XCB_PIXMAP_NONE);

    xcb_xfixes_region_t valid = 0;
    xcb_xfixes_region_t update = 0;
    uint32_t serial = 0;
    uint32_t options = 0;
    uint64_t targetMsc = 0;

    xcb_present_pixmap(m_output->backend()->connection(),
                       m_output->window(),
                       pixmap,
                       serial,
                       valid,
                       update,
                       0,
                       0,
                       XCB_NONE,
                       XCB_NONE,
                       XCB_NONE,
                       options,
                       targetMsc,
                       0,
                       0,
                       0,
                       nullptr);
}

quint32 X11WindowedQPainterPrimaryLayer::format() const
{
    switch (m_current->view()->format()) {
    case QImage::Format_A2RGB30_Premultiplied:
        return DRM_FORMAT_ARGB2101010;
    case QImage::Format_ARGB32_Premultiplied:
    default:
        return DRM_FORMAT_ARGB8888;
    }
}

X11WindowedQPainterCursorLayer::X11WindowedQPainterCursorLayer(X11WindowedOutput *output)
    : m_output(output)
{
}

std::optional<OutputLayerBeginFrameInfo> X11WindowedQPainterCursorLayer::beginFrame()
{
    const auto tmp = size().expandedTo(QSize(64, 64));
    const QSize bufferSize(std::ceil(tmp.width()), std::ceil(tmp.height()));
    if (m_buffer.size() != bufferSize) {
        m_buffer = QImage(bufferSize, QImage::Format_ARGB32_Premultiplied);
    }

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(&m_buffer),
        .repaint = infiniteRegion(),
    };
}

quint32 X11WindowedQPainterCursorLayer::format() const
{
    return DRM_FORMAT_ARGB8888;
}

bool X11WindowedQPainterCursorLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    m_output->cursor()->update(m_buffer, hotspot());
    return true;
}

X11WindowedQPainterBackend::X11WindowedQPainterBackend(X11WindowedBackend *backend)
    : QPainterBackend()
    , m_backend(backend)
{
    const auto outputs = m_backend->outputs();
    for (Output *output : outputs) {
        addOutput(output);
    }

    connect(backend, &X11WindowedBackend::outputAdded, this, &X11WindowedQPainterBackend::addOutput);
    connect(backend, &X11WindowedBackend::outputRemoved, this, &X11WindowedQPainterBackend::removeOutput);
}

X11WindowedQPainterBackend::~X11WindowedQPainterBackend()
{
    m_outputs.clear();
}

void X11WindowedQPainterBackend::addOutput(Output *output)
{
    X11WindowedOutput *x11Output = static_cast<X11WindowedOutput *>(output);
    m_outputs[output] = Layers{
        .primaryLayer = std::make_unique<X11WindowedQPainterPrimaryLayer>(x11Output),
        .cursorLayer = std::make_unique<X11WindowedQPainterCursorLayer>(x11Output),
    };
}

void X11WindowedQPainterBackend::removeOutput(Output *output)
{
    m_outputs.erase(output);
}

void X11WindowedQPainterBackend::present(Output *output)
{
    m_outputs[output].primaryLayer->present();
}

OutputLayer *X11WindowedQPainterBackend::primaryLayer(Output *output)
{
    return m_outputs[output].primaryLayer.get();
}

OutputLayer *X11WindowedQPainterBackend::cursorLayer(Output *output)
{
    return m_outputs[output].cursorLayer.get();
}

}
