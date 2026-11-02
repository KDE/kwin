/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "wayland_egl_backend.h"
#include "core/drmdevice.h"
#include "core/gbmgraphicsbufferallocator.h"
#include "core/renderdevice.h"
#include "opengl/glutils.h"
#include "scene/surfaceitem_wayland.h"
#include "swapchain.h"
#include "wayland-client/linuxdmabuf.h"
#include "wayland/surface.h"
#include "wayland_backend.h"
#include "wayland_display.h"
#include "wayland_logging.h"
#include "wayland_output.h"

#include <KWayland/Client/subsurface.h>
#include <KWayland/Client/surface.h>

#include <cmath>
#include <drm_fourcc.h>
#include <fcntl.h>
#include <unistd.h>

namespace KWin
{
namespace Wayland
{

WaylandEglCursorLayer::WaylandEglCursorLayer(WaylandOutput *output, RenderBackend *backend, WaylandDisplay *display)
    : OutputLayer(output, OutputLayerType::CursorOnly, 255, 255, 255)
    , m_outputBackend(output->backend())
    , m_renderBackend(backend)
    , m_display(display)
{
}

WaylandEglCursorLayer::~WaylandEglCursorLayer()
{
}

std::optional<OutputLayerBeginFrameInfo> WaylandEglCursorLayer::beginFrame(OutputFrame *frame)
{
    const auto bufferSize = targetRect().size();
    if (!m_swapchain || m_swapchain->size() != bufferSize) {
        const FormatModifierMap formatTable = m_display->linuxDmabuf()->formats().intersected(m_renderBackend->supportedFormats());
        const auto suitableFormats = filterAndSortFormats(formatTable, m_requiredAlphaBits, m_output->colorPowerTradeoff());
        for (const auto &candidate : suitableFormats) {
            auto it = formatTable.constFind(candidate.drmFormat);
            if (it == formatTable.constEnd()) {
                continue;
            }
            GraphicsBufferOptions options{
                .size = bufferSize,
                .format = it.key(),
                .modifiers = it.value(),
                .software = false,
                .scanout = false,
            };
            m_swapchain = Swapchain::create(m_renderBackend->renderDevice()->allocator(), options);
            if (m_swapchain) {
                break;
            }
        }
        if (!m_swapchain) {
            return std::nullopt;
        }
    }

    m_buffer = m_swapchain->acquire();
    if (!m_buffer) {
        return std::nullopt;
    }

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer->buffer(), bufferTransform().combine(OutputTransform::FlipY)),
        .repaint = Region::infinite(),
    };
}

bool WaylandEglCursorLayer::endFrame(const Region &damagedDeviceRegion, FileDescriptor &&sync, OutputFrame *frame)
{
    wl_buffer *buffer = m_outputBackend->importBuffer(m_buffer->buffer());
    Q_ASSERT(buffer);

    static_cast<WaylandOutput *>(m_output.get())->cursor()->update(buffer, m_buffer->buffer()->size() / m_output->scale(), (hotspot() / m_output->scale()).toPoint());

    m_swapchain->release(m_buffer.get(), std::move(sync));
    return true;
}

FormatModifierMap WaylandEglCursorLayer::supportedDrmFormats() const
{
    return m_renderBackend->supportedFormats();
}

void WaylandEglCursorLayer::releaseBuffers()
{
    m_buffer.reset();
    m_swapchain.reset();
}

WaylandEglBackend::WaylandEglBackend(WaylandBackend *b, RenderDevice *device)
    : EglBackend(device)
    , m_backend(b)
{
    connect(m_backend, &WaylandBackend::outputAdded, this, &WaylandEglBackend::createOutputLayers);

    b->setEglBackend(this);
}

WaylandEglBackend::~WaylandEglBackend()
{
    const auto outputs = m_backend->outputs();
    for (BackendOutput *output : outputs) {
        static_cast<WaylandOutput *>(output)->setOutputLayers({});
    }
    cleanup();
}

WaylandBackend *WaylandEglBackend::backend() const
{
    return m_backend;
}

void WaylandEglBackend::createOutputLayers(BackendOutput *output)
{
    const auto waylandOutput = static_cast<WaylandOutput *>(output);
    std::vector<std::unique_ptr<OutputLayer>> layers;
    auto primary = std::make_unique<WaylandLayer>(waylandOutput, this, backend()->display(), OutputLayerType::Primary, 0);
    primary->subSurface()->placeAbove(waylandOutput->surface());
    layers.push_back(std::move(primary));
    for (int z = 1; z < 5; z++) {
        auto layer = std::make_unique<WaylandLayer>(waylandOutput, this, backend()->display(), OutputLayerType::GenericLayer, z);
        layer->subSurface()->placeAbove(static_cast<WaylandLayer *>(layers.back().get())->surface());
        layers.push_back(std::move(layer));
    }
    layers.push_back(std::make_unique<WaylandEglCursorLayer>(waylandOutput, this, backend()->display()));
    waylandOutput->setOutputLayers(std::move(layers));
}

bool WaylandEglBackend::init()
{
    if (!initClientExtensions()) {
        qCWarning(KWIN_WAYLAND_BACKEND, "Could not initialize egl");
        return false;
    }
    if (!initRenderingContext()) {
        qCWarning(KWIN_WAYLAND_BACKEND, "Could not initialize rendering context");
        return false;
    }
    initWayland();
    return true;
}

bool WaylandEglBackend::initRenderingContext()
{
    if (!createContext()) {
        return false;
    }

    auto waylandOutputs = m_backend->waylandOutputs();

    // we only allow to start with at least one output
    if (waylandOutputs.isEmpty()) {
        return false;
    }

    for (auto *out : waylandOutputs) {
        createOutputLayers(out);
    }

    return openglContext()->makeCurrent();
}

QList<OutputLayer *> WaylandEglBackend::compatibleOutputLayers(BackendOutput *output)
{
    return static_cast<WaylandOutput *>(output)->outputLayers();
}

}
}

#include "moc_wayland_egl_backend.cpp"
