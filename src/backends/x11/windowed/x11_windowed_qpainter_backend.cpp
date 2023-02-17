/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_windowed_qpainter_backend.h"
#include "x11_windowed_backend.h"
#include "x11_windowed_logging.h"
#include "x11_windowed_output.h"

#include <cerrno>
#include <string.h>
#include <sys/shm.h>
#include <xcb/present.h>
#include <xcb/shm.h>

namespace KWin
{

X11WindowedQPainterLayerBuffer::X11WindowedQPainterLayerBuffer(const QSize &size, X11WindowedOutput *output)
    : m_connection(output->backend()->connection())
    , m_size(size)
{
    QImage::Format format;
    int bytesPerPixel;
    switch (output->depth()) {
    case 24:
    case 32:
        format = QImage::Format_ARGB32_Premultiplied;
        bytesPerPixel = 4;
        break;
    case 30:
        format = QImage::Format_A2RGB30_Premultiplied;
        bytesPerPixel = 4;
        break;
    default:
        qCWarning(KWIN_X11WINDOWED) << "Unsupported output depth:" << output->depth() << ". Falling back to ARGB32";
        format = QImage::Format_ARGB32_Premultiplied;
        bytesPerPixel = 4;
        break;
    }

    int shmId = shmget(IPC_PRIVATE, size.width() * size.height() * bytesPerPixel, IPC_CREAT | 0600);
    if (shmId < 0) {
        qCWarning(KWIN_X11WINDOWED) << "shmget() failed:" << strerror(errno);
        return;
    }

    m_buffer = shmat(shmId, nullptr, 0 /*read/write*/);
    shmctl(shmId, IPC_RMID, nullptr);
    if (reinterpret_cast<long>(m_buffer) == -1) {
        qCWarning(KWIN_X11WINDOWED) << "shmat() failed:" << strerror(errno);
        return;
    }

    xcb_shm_seg_t segment = xcb_generate_id(m_connection);
    xcb_shm_attach(m_connection, segment, shmId, false);

    m_pixmap = xcb_generate_id(m_connection);
    xcb_shm_create_pixmap(m_connection, m_pixmap, output->window(), size.width(), size.height(), output->depth(), segment, 0);
    xcb_shm_detach(m_connection, segment);

    m_view = std::make_unique<QImage>(static_cast<uchar *>(m_buffer), size.width(), size.height(), format);
}

X11WindowedQPainterLayerBuffer::~X11WindowedQPainterLayerBuffer()
{
    if (m_pixmap != XCB_PIXMAP_NONE) {
        xcb_free_pixmap(m_connection, m_pixmap);
    }
    m_view.reset();
    if (reinterpret_cast<long>(m_buffer) != -1) {
        shmdt(m_buffer);
    }
}

QSize X11WindowedQPainterLayerBuffer::size() const
{
    return m_size;
}

xcb_pixmap_t X11WindowedQPainterLayerBuffer::pixmap() const
{
    return m_pixmap;
}

QImage *X11WindowedQPainterLayerBuffer::view() const
{
    return m_view.get();
}

X11WindowedQPainterLayerSwapchain::X11WindowedQPainterLayerSwapchain(const QSize &size, X11WindowedOutput *output)
    : m_size(size)
{
    for (int i = 0; i < 2; ++i) {
        m_buffers.append(std::make_shared<X11WindowedQPainterLayerBuffer>(size, output));
    }
}

QSize X11WindowedQPainterLayerSwapchain::size() const
{
    return m_size;
}

std::shared_ptr<X11WindowedQPainterLayerBuffer> X11WindowedQPainterLayerSwapchain::acquire()
{
    m_index = (m_index + 1) % m_buffers.count();
    return m_buffers[m_index];
}

void X11WindowedQPainterLayerSwapchain::release(std::shared_ptr<X11WindowedQPainterLayerBuffer> buffer)
{
    Q_ASSERT(m_buffers[m_index] == buffer);
}

X11WindowedQPainterPrimaryLayer::X11WindowedQPainterPrimaryLayer(X11WindowedOutput *output)
    : m_output(output)
{
}

std::optional<OutputLayerBeginFrameInfo> X11WindowedQPainterPrimaryLayer::beginFrame()
{
    const QSize bufferSize = m_output->pixelSize();
    if (!m_swapchain || m_swapchain->size() != bufferSize) {
        m_swapchain = std::make_unique<X11WindowedQPainterLayerSwapchain>(bufferSize, m_output);
    }

    QRegion repaint = m_output->exposedArea() + m_output->rect();
    m_output->clearExposedArea();

    m_buffer = m_swapchain->acquire();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer->view()),
        .repaint = repaint,
    };
}

bool X11WindowedQPainterPrimaryLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    return true;
}

void X11WindowedQPainterPrimaryLayer::present()
{
    xcb_xfixes_region_t valid = 0;
    xcb_xfixes_region_t update = 0;
    uint32_t serial = 0;
    uint32_t options = 0;
    uint64_t targetMsc = 0;

    xcb_present_pixmap(m_output->backend()->connection(),
                       m_output->window(),
                       m_buffer->pixmap(),
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

    m_swapchain->release(m_buffer);
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

X11WindowedQPainterCursorLayer *X11WindowedQPainterBackend::cursorLayer(Output *output)
{
    return m_outputs[output].cursorLayer.get();
}

}
