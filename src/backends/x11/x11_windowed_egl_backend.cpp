/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_windowed_egl_backend.h"
#include "core/gbmgraphicsbufferallocator.h"
#include "core/renderdevice.h"
#include "opengl/eglnativefence.h"
#include "swapchain.h"
#include "x11_windowed_backend.h"
#include "x11_windowed_logging.h"
#include "x11_windowed_output.h"

#include <drm_fourcc.h>

namespace KWin
{

X11WindowedPrimaryLayer::X11WindowedPrimaryLayer(X11WindowedBackend *outputBackend, RenderBackend *renderBackend, X11WindowedOutput *output)
    : OutputLayer(output, OutputLayerType::Primary)
    , m_output(output)
    , m_outputBackend(outputBackend)
    , m_renderBackend(renderBackend)
{
}

X11WindowedPrimaryLayer::~X11WindowedPrimaryLayer()
{
}

std::optional<OutputLayerBeginFrameInfo> X11WindowedPrimaryLayer::beginFrame(OutputFrame *frame)
{
    const QSize bufferSize = m_output->modeSize();
    if (!m_swapchain || m_swapchain->size() != bufferSize) {
        const uint32_t format = DRM_FORMAT_XRGB8888;
        const FormatModifierMap formatTable = m_outputBackend->driFormats();
        if (!formatTable.contains(format)) {
            return std::nullopt;
        }
        GraphicsBufferOptions options{
            .size = bufferSize,
            .format = format,
            .modifiers = formatTable[format],
            .software = false,
            .scanout = false,
        };
        m_swapchain = Swapchain::create(m_renderBackend->renderDevice()->allocator(), options);
        if (!m_swapchain) {
            return std::nullopt;
        }
    }

    m_buffer = m_swapchain->acquire();
    if (!m_buffer) {
        return std::nullopt;
    }

    Region repaint = Region::infinite();
    m_output->clearExposedArea();

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer->buffer(), OutputTransform::FlipY),
        .repaint = repaint,
    };
}

bool X11WindowedPrimaryLayer::endFrame(const Region &damagedDeviceRegion, FileDescriptor &&sync, OutputFrame *frame)
{
    m_swapchain->release(m_buffer.get(), std::move(sync));
    m_output->setPrimaryBuffer(m_buffer->buffer());
    return true;
}

FormatModifierMap X11WindowedPrimaryLayer::supportedDrmFormats() const
{
    return m_outputBackend->driFormats();
}

void X11WindowedPrimaryLayer::releaseBuffers()
{
    m_buffer.reset();
    m_swapchain.reset();
}

X11WindowedCursorLayer::X11WindowedCursorLayer(X11WindowedBackend *outputBackend, RenderBackend *renderBackend, X11WindowedOutput *output)
    : OutputLayer(output, OutputLayerType::CursorOnly)
    , m_outputBackend(outputBackend)
    , m_renderBackend(renderBackend)
{
}

X11WindowedCursorLayer::~X11WindowedCursorLayer()
{
}

std::optional<OutputLayerBeginFrameInfo> X11WindowedCursorLayer::beginFrame(OutputFrame *frame)
{
    const auto bufferSize = targetRect().size();
    if (!m_swapchain || m_swapchain->size() != bufferSize) {
        GraphicsBufferOptions options{
            .size = bufferSize,
            .format = DRM_FORMAT_ARGB8888,
            .modifiers = m_renderBackend->supportedFormats()[DRM_FORMAT_ARGB8888],
            .software = false,
            .scanout = false,
            .render = true,
        };
        m_swapchain = Swapchain::create(m_renderBackend->renderDevice()->allocator(), options);
        if (!m_swapchain) {
            return std::nullopt;
        }
    }

    m_slot = m_swapchain->acquire();
    if (!m_slot) {
        return std::nullopt;
    }

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_slot->buffer(), OutputTransform::Normal),
        .repaint = Region::infinite(),
    };
}

bool X11WindowedCursorLayer::endFrame(const Region &damagedDeviceRegion, FileDescriptor &&sync, OutputFrame *frame)
{
    const QImage buffer = m_renderBackend->renderDevice()->download(m_slot->buffer());
    m_slot.reset();
    if (buffer.isNull()) {
        return false;
    }

    static_cast<X11WindowedOutput *>(m_output.get())->cursor()->update(buffer.mirrored(false, true), hotspot());
    return true;
}

FormatModifierMap X11WindowedCursorLayer::supportedDrmFormats() const
{
    return m_renderBackend->supportedFormats();
}

void X11WindowedCursorLayer::releaseBuffers()
{
}

X11WindowedEglBackend::X11WindowedEglBackend(X11WindowedBackend *backend, RenderDevice *device)
    : EglBackend(device)
    , m_backend(backend)
{
}

X11WindowedEglBackend::~X11WindowedEglBackend()
{
    const auto outputs = m_backend->outputs();
    for (BackendOutput *output : outputs) {
        static_cast<X11WindowedOutput *>(output)->setOutputLayers({});
    }
    cleanup();
}

X11WindowedBackend *X11WindowedEglBackend::backend() const
{
    return m_backend;
}

bool X11WindowedEglBackend::init()
{
    qputenv("EGL_PLATFORM", "x11");

    if (!initClientExtensions()) {
        qCWarning(KWIN_X11WINDOWED, "Could not initialize egl");
        return false;
    }
    if (!createContext()) {
        qCWarning(KWIN_X11WINDOWED, "Could not initialize rendering context");
        return false;
    }

    initWayland();

    const auto &outputs = m_backend->outputs();
    for (const auto &output : outputs) {
        X11WindowedOutput *x11Output = static_cast<X11WindowedOutput *>(output);
        std::vector<std::unique_ptr<OutputLayer>> layers;
        layers.push_back(std::make_unique<X11WindowedPrimaryLayer>(backend(), this, x11Output));
        layers.push_back(std::make_unique<X11WindowedCursorLayer>(backend(), this, x11Output));
        x11Output->setOutputLayers(std::move(layers));
    }
    return true;
}

QList<OutputLayer *> X11WindowedEglBackend::compatibleOutputLayers(BackendOutput *output)
{
    return static_cast<X11WindowedOutput *>(output)->outputLayers();
}

} // namespace

#include "moc_x11_windowed_egl_backend.cpp"
