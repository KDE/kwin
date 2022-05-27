/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "scene_qpainter_x11_backend.h"
#include "main.h"
#include "screens.h"
#include "softwarevsyncmonitor.h"
#include "x11windowed_backend.h"
#include "x11windowed_output.h"

namespace KWin
{

X11WindowedQPainterOutput::X11WindowedQPainterOutput(Output *output, xcb_window_t window)
    : window(window)
    , buffer(output->pixelSize() * output->scale(), QImage::Format_RGB32)
    , m_output(output)
{
    buffer.fill(Qt::black);
}

OutputLayerBeginFrameInfo X11WindowedQPainterOutput::beginFrame()
{
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
    connect(screens(), &Screens::changed, this, &X11WindowedQPainterBackend::createOutputs);
    createOutputs();
}

X11WindowedQPainterBackend::~X11WindowedQPainterBackend()
{
    m_outputs.clear();
    if (m_gc) {
        xcb_free_gc(m_backend->connection(), m_gc);
    }
}

void X11WindowedQPainterBackend::createOutputs()
{
    m_outputs.clear();
    const auto &outputs = m_backend->outputs();
    for (const auto &x11Output : outputs) {
        m_outputs[x11Output] = QSharedPointer<X11WindowedQPainterOutput>::create(x11Output, m_backend->windowForScreen(x11Output));
    }
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
