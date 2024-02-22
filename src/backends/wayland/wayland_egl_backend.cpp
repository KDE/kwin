/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "wayland_egl_backend.h"
#include "core/gbmgraphicsbufferallocator.h"
#include "opengl/eglswapchain.h"
#include "opengl/glrendertimequery.h"
#include "opengl/glutils.h"
#include "platformsupport/scenes/opengl/basiceglsurfacetexture_wayland.h"
#include "wayland_backend.h"
#include "wayland_display.h"
#include "wayland_logging.h"
#include "wayland_output.h"

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

WaylandEglPrimaryLayer::WaylandEglPrimaryLayer(WaylandOutput *output, WaylandEglBackend *backend)
    : m_waylandOutput(output)
    , m_backend(backend)
{
}

WaylandEglPrimaryLayer::~WaylandEglPrimaryLayer()
{
}

GLFramebuffer *WaylandEglPrimaryLayer::fbo() const
{
    return m_buffer->framebuffer();
}

std::shared_ptr<GLTexture> WaylandEglPrimaryLayer::texture() const
{
    return m_buffer->texture();
}

std::optional<OutputLayerBeginFrameInfo> WaylandEglPrimaryLayer::beginFrame()
{
    if (!m_backend->contextObject()->makeCurrent()) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "Make Context Current failed";
        return std::nullopt;
    }

    const QSize nativeSize = m_waylandOutput->modeSize();
    if (!m_swapchain || m_swapchain->size() != nativeSize) {
        const QHash<uint32_t, QList<uint64_t>> formatTable = m_backend->backend()->display()->linuxDmabuf()->formats();
        uint32_t format = DRM_FORMAT_INVALID;
        QList<uint64_t> modifiers;
        for (const uint32_t &candidateFormat : {DRM_FORMAT_XRGB2101010, DRM_FORMAT_XRGB8888}) {
            auto it = formatTable.constFind(candidateFormat);
            if (it != formatTable.constEnd()) {
                format = it.key();
                modifiers = it.value();
                break;
            }
        }
        if (format == DRM_FORMAT_INVALID) {
            qCWarning(KWIN_WAYLAND_BACKEND) << "Could not find a suitable render format";
            return std::nullopt;
        }
        m_swapchain = EglSwapchain::create(m_backend->graphicsBufferAllocator(), m_backend->contextObject(), nativeSize, format, modifiers);
        if (!m_swapchain) {
            return std::nullopt;
        }
    }

    m_buffer = m_swapchain->acquire();
    if (!m_buffer) {
        return std::nullopt;
    }

    const QRegion repair = bufferAgeEnabled ? m_damageJournal.accumulate(m_buffer->age(), infiniteRegion()) : infiniteRegion();
    if (!m_query) {
        m_query = std::make_unique<GLRenderTimeQuery>();
    }
    m_query->begin();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer->framebuffer()),
        .repaint = repair,
    };
}

bool WaylandEglPrimaryLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    m_query->end();
    // Flush rendering commands to the dmabuf.
    glFlush();

    m_damageJournal.add(damagedRegion);
    return true;
}

void WaylandEglPrimaryLayer::present()
{
    wl_buffer *buffer = m_backend->backend()->importBuffer(m_buffer->buffer());
    Q_ASSERT(buffer);

    KWayland::Client::Surface *surface = m_waylandOutput->surface();
    surface->attachBuffer(buffer);
    surface->damage(m_damageJournal.lastDamage());
    surface->setScale(std::ceil(m_waylandOutput->scale()));
    surface->commit();
    Q_EMIT m_waylandOutput->outputChange(m_damageJournal.lastDamage());

    m_swapchain->release(m_buffer);
}

std::chrono::nanoseconds WaylandEglPrimaryLayer::queryRenderTime() const
{
    m_backend->makeCurrent();
    return m_query->result();
}

WaylandEglCursorLayer::WaylandEglCursorLayer(WaylandOutput *output, WaylandEglBackend *backend)
    : m_output(output)
    , m_backend(backend)
{
}

WaylandEglCursorLayer::~WaylandEglCursorLayer()
{
    m_backend->contextObject()->makeCurrent();
}

std::optional<OutputLayerBeginFrameInfo> WaylandEglCursorLayer::beginFrame()
{
    if (!m_backend->contextObject()->makeCurrent()) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "Make Context Current failed";
        return std::nullopt;
    }

    const auto tmp = size().expandedTo(QSize(64, 64));
    const QSize bufferSize(std::ceil(tmp.width()), std::ceil(tmp.height()));
    if (!m_swapchain || m_swapchain->size() != bufferSize) {
        const QHash<uint32_t, QList<uint64_t>> formatTable = m_backend->backend()->display()->linuxDmabuf()->formats();
        uint32_t format = DRM_FORMAT_INVALID;
        QList<uint64_t> modifiers;
        for (const uint32_t &candidateFormat : {DRM_FORMAT_ARGB2101010, DRM_FORMAT_ARGB8888}) {
            auto it = formatTable.constFind(candidateFormat);
            if (it != formatTable.constEnd()) {
                format = it.key();
                modifiers = it.value();
                break;
            }
        }
        if (format == DRM_FORMAT_INVALID) {
            qCWarning(KWIN_WAYLAND_BACKEND) << "Could not find a suitable render format";
            return std::nullopt;
        }
        m_swapchain = EglSwapchain::create(m_backend->graphicsBufferAllocator(), m_backend->contextObject(), bufferSize, format, modifiers);
        if (!m_swapchain) {
            return std::nullopt;
        }
    }

    m_buffer = m_swapchain->acquire();
    if (!m_buffer) {
        return std::nullopt;
    }

    if (!m_query) {
        m_query = std::make_unique<GLRenderTimeQuery>();
    }
    m_query->begin();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer->framebuffer()),
        .repaint = infiniteRegion(),
    };
}

bool WaylandEglCursorLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    m_query->end();
    // Flush rendering commands to the dmabuf.
    glFlush();

    wl_buffer *buffer = m_backend->backend()->importBuffer(m_buffer->buffer());
    Q_ASSERT(buffer);

    m_output->cursor()->update(buffer, scale(), hotspot().toPoint());

    m_swapchain->release(m_buffer);
    return true;
}

std::chrono::nanoseconds WaylandEglCursorLayer::queryRenderTime() const
{
    m_backend->makeCurrent();
    return m_query->result();
}

WaylandEglBackend::WaylandEglBackend(WaylandBackend *b)
    : AbstractEglBackend()
    , m_backend(b)
    , m_allocator(std::make_unique<GbmGraphicsBufferAllocator>(b->gbmDevice()))
{
    connect(m_backend, &WaylandBackend::outputAdded, this, &WaylandEglBackend::createEglWaylandOutput);
    connect(m_backend, &WaylandBackend::outputRemoved, this, [this](Output *output) {
        m_outputs.erase(output);
    });

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

GraphicsBufferAllocator *WaylandEglBackend::graphicsBufferAllocator() const
{
    return m_allocator.get();
}

void WaylandEglBackend::cleanupSurfaces()
{
    m_outputs.clear();
}

bool WaylandEglBackend::createEglWaylandOutput(Output *waylandOutput)
{
    m_outputs[waylandOutput] = Layers{
        .primaryLayer = std::make_unique<WaylandEglPrimaryLayer>(static_cast<WaylandOutput *>(waylandOutput), this),
        .cursorLayer = std::make_unique<WaylandEglCursorLayer>(static_cast<WaylandOutput *>(waylandOutput), this),
    };
    return true;
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

        m_backend->setEglDisplay(EglDisplay::create(eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, m_backend->gbmDevice(), nullptr)));
    }

    auto display = m_backend->sceneEglDisplayObject();
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

    initKWinGL();
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
        if (!createEglWaylandOutput(out)) {
            return false;
        }
    }

    if (m_outputs.empty()) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "Create Window Surfaces failed";
        return false;
    }

    return makeCurrent();
}

std::pair<std::shared_ptr<KWin::GLTexture>, ColorDescription> WaylandEglBackend::textureForOutput(KWin::Output *output) const
{
    return std::make_pair(m_outputs.at(output).primaryLayer->texture(), ColorDescription::sRGB);
}

std::unique_ptr<SurfaceTexture> WaylandEglBackend::createSurfaceTextureWayland(SurfacePixmap *pixmap)
{
    return std::make_unique<BasicEGLSurfaceTextureWayland>(this, pixmap);
}

void WaylandEglBackend::present(Output *output, const std::shared_ptr<OutputFrame> &frame)
{
    m_outputs[output].primaryLayer->present();
    static_cast<WaylandOutput *>(output)->framePending(frame);
}

OutputLayer *WaylandEglBackend::primaryLayer(Output *output)
{
    return m_outputs[output].primaryLayer.get();
}

OutputLayer *WaylandEglBackend::cursorLayer(Output *output)
{
    return m_outputs[output].cursorLayer.get();
}

}
}

#include "moc_wayland_egl_backend.cpp"
