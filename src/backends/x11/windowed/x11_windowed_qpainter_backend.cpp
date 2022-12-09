/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_windowed_qpainter_backend.h"
#include "x11_windowed_backend.h"
#include "x11_windowed_output.h"

namespace KWin
{

X11WindowedQPainterPrimaryLayer::X11WindowedQPainterPrimaryLayer(X11WindowedOutput *output)
    : m_output(output)
{
}

void X11WindowedQPainterPrimaryLayer::ensureBuffer()
{
    const QSize nativeSize(m_output->pixelSize() * m_output->scale());

    if (buffer.size() != nativeSize) {
        buffer = QImage(nativeSize, QImage::Format_RGB32);
        buffer.fill(Qt::black);
    }
}

std::optional<OutputLayerBeginFrameInfo> X11WindowedQPainterPrimaryLayer::beginFrame()
{
    ensureBuffer();

    QRegion repaint = m_output->exposedArea() + m_output->rect();
    m_output->clearExposedArea();

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(&buffer),
        .repaint = repaint,
    };
}

bool X11WindowedQPainterPrimaryLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    return true;
}

X11WindowedQPainterCursorLayer::X11WindowedQPainterCursorLayer(X11WindowedOutput *output)
    : m_output(output)
{
}

QPoint X11WindowedQPainterCursorLayer::hotspot() const
{
    return m_hotspot;
}

void X11WindowedQPainterCursorLayer::setHotspot(const QPoint &hotspot)
{
    m_hotspot = hotspot;
}

QSize X11WindowedQPainterCursorLayer::size() const
{
    return m_size;
}

void X11WindowedQPainterCursorLayer::setSize(const QSize &size)
{
    m_size = size;
}

std::optional<OutputLayerBeginFrameInfo> X11WindowedQPainterCursorLayer::beginFrame()
{
    const QSize bufferSize = m_size.expandedTo(QSize(64, 64));
    if (m_buffer.size() != bufferSize) {
        m_buffer = QImage(bufferSize, QImage::Format_ARGB32_Premultiplied);
    }

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(&m_buffer),
        .repaint = infiniteRegion(),
    };
}

bool X11WindowedQPainterCursorLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    m_output->cursor()->update(m_buffer, m_hotspot);
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
    if (m_gc) {
        xcb_free_gc(m_backend->connection(), m_gc);
    }
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
    const auto &rendererOutput = m_outputs[output];

    xcb_connection_t *c = m_backend->connection();
    const xcb_window_t window = rendererOutput.primaryLayer->m_output->window();
    if (m_gc == XCB_NONE) {
        m_gc = xcb_generate_id(c);
        xcb_create_gc(c, m_gc, window, 0, nullptr);
    }

    // TODO: only update changes?
    const QImage &buffer = rendererOutput.primaryLayer->buffer;
    xcb_put_image(c, XCB_IMAGE_FORMAT_Z_PIXMAP, window,
                  m_gc, buffer.width(), buffer.height(), 0, 0, 0, 24,
                  buffer.sizeInBytes(), buffer.constBits());
}

OutputLayer *X11WindowedQPainterBackend::primaryLayer(Output *output)
{
    return m_outputs[output].primaryLayer.get();
}

X11WindowedQPainterCursorLayer *X11WindowedQPainterBackend::cursorLayer(Output *output)
{
    return m_outputs[output].cursorLayer.get();
}

}
