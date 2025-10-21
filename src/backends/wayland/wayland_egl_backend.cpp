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
#include "opengl/eglswapchain.h"
#include "opengl/glrendertimequery.h"
#include "opengl/glutils.h"
#include "scene/surfaceitem_wayland.h"
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

static const bool bufferAgeEnabled = qEnvironmentVariable("KWIN_USE_BUFFER_AGE") != QStringLiteral("0");

WaylandEglLayer::WaylandEglLayer(WaylandOutput *output, WaylandEglBackend *backend, OutputLayerType type, int zpos)
    : WaylandLayer(output, type, zpos)
    , m_backend(backend)
{
}

WaylandEglLayer::~WaylandEglLayer()
{
}

GLFramebuffer *WaylandEglLayer::fbo() const
{
    return m_buffer->framebuffer();
}

std::optional<OutputLayerBeginFrameInfo> WaylandEglLayer::doBeginFrame()
{
    if (!m_backend->openglContext()->makeCurrent()) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "Make Context Current failed";
        return std::nullopt;
    }

    if (m_color != m_previousColor) {
        // need to force a full repaint
        m_damageJournal.clear();
    }

    const QSize nativeSize = targetRect().size();
    if (!m_swapchain || m_swapchain->size() != nativeSize) {
        const QHash<uint32_t, QList<uint64_t>> formatTable = m_backend->backend()->display()->linuxDmabuf()->formats();
        const auto suitableFormats = filterAndSortFormats(formatTable, m_requiredAlphaBits, m_output->colorPowerTradeoff());
        for (const auto &candidate : suitableFormats) {
            auto it = formatTable.constFind(candidate.drmFormat);
            if (it == formatTable.constEnd()) {
                continue;
            }
            m_swapchain = EglSwapchain::create(m_backend->drmDevice()->allocator(), m_backend->openglContext(), nativeSize, it.key(), it.value());
            if (m_swapchain) {
                break;
            }
        }
        if (!m_swapchain) {
            qCWarning(KWIN_WAYLAND_BACKEND) << "Could not find a suitable render format";
            return std::nullopt;
        }
    }

    m_buffer = m_swapchain->acquire();
    if (!m_buffer) {
        return std::nullopt;
    }

    const QRegion repair = bufferAgeEnabled ? m_damageJournal.accumulate(m_buffer->age(), infiniteRegion()) : infiniteRegion();
    m_query = std::make_unique<GLRenderTimeQuery>(m_backend->openglContextRef());
    m_query->begin();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer->framebuffer(), m_color),
        .repaint = repair,
    };
}

bool WaylandEglLayer::doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    m_query->end();
    frame->addRenderTimeQuery(std::move(m_query));
    // Flush rendering commands to the dmabuf.
    glFlush();
    EGLNativeFence releaseFence{m_backend->eglDisplayObject()};

    setBuffer(m_backend->backend()->importBuffer(m_buffer->buffer()), damagedRegion);
    m_swapchain->release(m_buffer, releaseFence.takeFileDescriptor());

    m_damageJournal.add(damagedRegion);
    return true;
}

bool WaylandEglLayer::importScanoutBuffer(GraphicsBuffer *buffer, const std::shared_ptr<OutputFrame> &frame)
{
    if (!test()) {
        return false;
    }
    auto presentationBuffer = m_backend->backend()->importBuffer(buffer);
    if (!presentationBuffer) {
        return false;
    }
    setBuffer(presentationBuffer, infiniteRegion());
    return true;
}

DrmDevice *WaylandEglLayer::scanoutDevice() const
{
    return m_backend->drmDevice();
}

QHash<uint32_t, QList<uint64_t>> WaylandEglLayer::supportedDrmFormats() const
{
    return m_backend->backend()->display()->linuxDmabuf()->formats();
}

void WaylandEglLayer::releaseBuffers()
{
    m_buffer.reset();
    m_swapchain.reset();
}

WaylandEglCursorLayer::WaylandEglCursorLayer(WaylandOutput *output, WaylandEglBackend *backend)
    : OutputLayer(output, OutputLayerType::CursorOnly, 255, 255, 255)
    , m_backend(backend)
{
}

WaylandEglCursorLayer::~WaylandEglCursorLayer()
{
    m_backend->openglContext()->makeCurrent();
}

std::optional<OutputLayerBeginFrameInfo> WaylandEglCursorLayer::doBeginFrame()
{
    if (!m_backend->openglContext()->makeCurrent()) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "Make Context Current failed";
        return std::nullopt;
    }

    const auto bufferSize = targetRect().size();
    if (!m_swapchain || m_swapchain->size() != bufferSize) {
        const QHash<uint32_t, QList<uint64_t>> formatTable = m_backend->backend()->display()->linuxDmabuf()->formats();
        const auto suitableFormats = filterAndSortFormats(formatTable, m_requiredAlphaBits, m_output->colorPowerTradeoff());
        for (const auto &candidate : suitableFormats) {
            auto it = formatTable.constFind(candidate.drmFormat);
            if (it == formatTable.constEnd()) {
                continue;
            }
            m_swapchain = EglSwapchain::create(m_backend->drmDevice()->allocator(), m_backend->openglContext(), bufferSize, it.key(), it.value());
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

    m_query = std::make_unique<GLRenderTimeQuery>(m_backend->openglContextRef());
    m_query->begin();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer->framebuffer()),
        .repaint = infiniteRegion(),
    };
}

bool WaylandEglCursorLayer::doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    m_query->end();
    if (frame) {
        frame->addRenderTimeQuery(std::move(m_query));
    }
    // Flush rendering commands to the dmabuf.
    glFlush();

    wl_buffer *buffer = m_backend->backend()->importBuffer(m_buffer->buffer());
    Q_ASSERT(buffer);

    static_cast<WaylandOutput *>(m_output.get())->cursor()->update(buffer, m_buffer->buffer()->size() / m_output->scale(), hotspot().toPoint());

    EGLNativeFence releaseFence{m_backend->eglDisplayObject()};
    m_swapchain->release(m_buffer, releaseFence.takeFileDescriptor());
    return true;
}

DrmDevice *WaylandEglCursorLayer::scanoutDevice() const
{
    return m_backend->drmDevice();
}

QHash<uint32_t, QList<uint64_t>> WaylandEglCursorLayer::supportedDrmFormats() const
{
    return m_backend->supportedFormats();
}

void WaylandEglCursorLayer::releaseBuffers()
{
    m_buffer.reset();
    m_swapchain.reset();
}

WaylandEglBackend::WaylandEglBackend(WaylandBackend *b)
    : m_backend(b)
{
    connect(m_backend, &WaylandBackend::outputAdded, this, &WaylandEglBackend::createOutputLayers);

    b->setEglBackend(this);
}

WaylandEglBackend::~WaylandEglBackend()
{
    cleanup();
}

WaylandBackend *WaylandEglBackend::backend() const
{
    return m_backend;
}

DrmDevice *WaylandEglBackend::drmDevice() const
{
    return m_backend->drmDevice();
}

void WaylandEglBackend::cleanupSurfaces()
{
    const auto outputs = m_backend->outputs();
    for (Output *output : outputs) {
        static_cast<WaylandOutput *>(output)->setOutputLayers({});
    }
}

void WaylandEglBackend::createOutputLayers(Output *output)
{
    const auto waylandOutput = static_cast<WaylandOutput *>(output);
    std::vector<std::unique_ptr<OutputLayer>> layers;
    auto primary = std::make_unique<WaylandEglLayer>(waylandOutput, this, OutputLayerType::Primary, 0);
    primary->subSurface()->placeAbove(waylandOutput->surface());
    layers.push_back(std::move(primary));
    for (int z = 1; z < 5; z++) {
        auto layer = std::make_unique<WaylandEglLayer>(waylandOutput, this, OutputLayerType::GenericLayer, z);
        layer->subSurface()->placeAbove(static_cast<WaylandEglLayer *>(layers.back().get())->surface());
        layers.push_back(std::move(layer));
    }
    layers.push_back(std::make_unique<WaylandEglCursorLayer>(waylandOutput, this));
    waylandOutput->setOutputLayers(std::move(layers));
}

bool WaylandEglBackend::initializeEgl()
{
    initClientExtensions();

    if (!m_backend->sceneEglDisplayObject()) {
        for (const QByteArray &extension : {QByteArrayLiteral("EGL_EXT_platform_base"), QByteArrayLiteral("EGL_KHR_platform_gbm")}) {
            if (!hasClientExtension(extension)) {
                qCWarning(KWIN_WAYLAND_BACKEND) << extension << "client extension is not supported by the platform";
                return false;
            }
        }

        m_backend->setEglDisplay(EglDisplay::create(eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, m_backend->drmDevice()->gbmDevice(), nullptr)));
    }

    const auto display = m_backend->sceneEglDisplayObject();
    if (!display) {
        return false;
    }
    setEglDisplay(display);
    return true;
}

void WaylandEglBackend::init()
{
    if (!initializeEgl()) {
        setFailed("Could not initialize egl");
        return;
    }
    if (!initRenderingContext()) {
        setFailed("Could not initialize rendering context");
        return;
    }

    initWayland();
}

bool WaylandEglBackend::initRenderingContext()
{
    if (!createContext(EGL_NO_CONFIG_KHR)) {
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

QList<OutputLayer *> WaylandEglBackend::compatibleOutputLayers(Output *output)
{
    return static_cast<WaylandOutput *>(output)->outputLayers();
}

}
}

#include "moc_wayland_egl_backend.cpp"
