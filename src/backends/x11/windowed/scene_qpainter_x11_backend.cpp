/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "scene_qpainter_x11_backend.h"
#include "main.h"
#include "softwarevsyncmonitor.h"
#include "x11windowed_backend.h"
#include "x11windowed_output.h"

namespace KWin
{

X11WindowedQPainterOutput::X11WindowedQPainterOutput(Output *output, xcb_window_t window)
    : window(window)
    , m_output(output)
{
}

void X11WindowedQPainterOutput::ensureBuffer()
{
    const QSize nativeSize(m_output->pixelSize() * m_output->scale());

    if (buffer.size() != nativeSize) {
        buffer = QImage(nativeSize, QImage::Format_RGB32);
        buffer.fill(Qt::black);
    }
}

OutputLayerBeginFrameInfo X11WindowedQPainterOutput::beginFrame()
{
    ensureBuffer();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(&buffer),
        .repaint = m_output->rect(),
    };
}

bool X11WindowedQPainterOutput::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(renderedRegion)
    Q_UNUSED(damagedRegion)
    return true;
}

X11WindowedQPainterBackend::X11WindowedQPainterBackend(X11WindowedBackend *backend)
    : QPainterBackend()
    , m_backend(backend)
{
    const auto outputs = m_backend->enabledOutputs();
    for (Output *output : outputs) {
        addOutput(output);
    }

    connect(backend, &X11WindowedBackend::outputEnabled, this, &X11WindowedQPainterBackend::addOutput);
    connect(backend, &X11WindowedBackend::outputDisabled, this, &X11WindowedQPainterBackend::removeOutput);
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
    m_outputs[output] = std::make_shared<X11WindowedQPainterOutput>(output, m_backend->windowForScreen(output));
}

void X11WindowedQPainterBackend::removeOutput(Output *output)
{
    m_outputs.remove(output);
}

void X11WindowedQPainterBackend::present(Output *output)
{
    static_cast<X11WindowedOutput *>(output)->vsyncMonitor()->arm();

    xcb_connection_t *c = m_backend->connection();
    const xcb_window_t window = m_backend->window();
    if (m_gc == XCB_NONE) {
        m_gc = xcb_generate_id(c);
        xcb_create_gc(c, m_gc, window, 0, nullptr);
    }

    const auto &rendererOutput = m_outputs[output];
    Q_ASSERT(rendererOutput);

    // TODO: only update changes?
    const QImage &buffer = rendererOutput->buffer;
    xcb_put_image(c, XCB_IMAGE_FORMAT_Z_PIXMAP, rendererOutput->window,
                  m_gc, buffer.width(), buffer.height(), 0, 0, 0, 24,
                  buffer.sizeInBytes(), buffer.constBits());
}

OutputLayer *X11WindowedQPainterBackend::primaryLayer(Output *output)
{
    return m_outputs[output].get();
}
}
