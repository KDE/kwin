/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "wayland_egl_backend.h"
#include "core/gbmgraphicsbufferallocator.h"
#include "libkwineffects/kwinglutils.h"
#include "platformsupport/scenes/opengl/basiceglsurfacetexture_internal.h"
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

WaylandEglLayerBuffer::WaylandEglLayerBuffer(GbmGraphicsBuffer *buffer, WaylandEglBackend *backend)
    : m_graphicsBuffer(buffer)
{
    m_texture = backend->importDmaBufAsTexture(*buffer->dmabufAttributes());
    m_framebuffer = std::make_unique<GLFramebuffer>(m_texture.get());
}

WaylandEglLayerBuffer::~WaylandEglLayerBuffer()
{
    m_texture.reset();
    m_framebuffer.reset();
    m_graphicsBuffer->drop();
}

GbmGraphicsBuffer *WaylandEglLayerBuffer::graphicsBuffer() const
{
    return m_graphicsBuffer;
}

GLFramebuffer *WaylandEglLayerBuffer::framebuffer() const
{
    return m_framebuffer.get();
}

std::shared_ptr<GLTexture> WaylandEglLayerBuffer::texture() const
{
    return m_texture;
}

int WaylandEglLayerBuffer::age() const
{
    return m_age;
}

WaylandEglLayerSwapchain::WaylandEglLayerSwapchain(const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers, WaylandEglBackend *backend)
    : m_backend(backend)
    , m_size(size)
    , m_format(format)
    , m_modifiers(modifiers)
    , m_allocator(std::make_unique<GbmGraphicsBufferAllocator>(backend->backend()->gbmDevice()))
{
}

WaylandEglLayerSwapchain::~WaylandEglLayerSwapchain()
{
}

QSize WaylandEglLayerSwapchain::size() const
{
    return m_size;
}

std::shared_ptr<WaylandEglLayerBuffer> WaylandEglLayerSwapchain::acquire()
{
    for (const auto &buffer : m_buffers) {
        if (!buffer->graphicsBuffer()->isReferenced()) {
            return buffer;
        }
    }

    GbmGraphicsBuffer *graphicsBuffer = m_allocator->allocate(m_size, m_format, m_modifiers);
    if (!graphicsBuffer) {
        qCWarning(KWIN_WAYLAND_BACKEND) << "Failed to allocate layer swapchain buffer";
        return nullptr;
    }

    auto buffer = std::make_shared<WaylandEglLayerBuffer>(graphicsBuffer, m_backend);
    m_buffers << buffer;

    return buffer;
}

void WaylandEglLayerSwapchain::release(std::shared_ptr<WaylandEglLayerBuffer> buffer)
{
    for (qsizetype i = 0; i < m_buffers.count(); ++i) {
        if (m_buffers[i] == buffer) {
            m_buffers[i]->m_age = 1;
        } else if (m_buffers[i]->m_age > 0) {
            m_buffers[i]->m_age++;
        }
    }
}

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
    if (eglMakeCurrent(m_backend->eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, m_backend->context()) == EGL_FALSE) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "Make Context Current failed";
        return std::nullopt;
    }

    const QSize nativeSize = m_waylandOutput->modeSize();
    if (!m_swapchain || m_swapchain->size() != nativeSize) {
        const WaylandLinuxDmabufV1 *dmabuf = m_backend->backend()->display()->linuxDmabuf();
        const uint32_t format = DRM_FORMAT_XRGB8888;
        if (!dmabuf->formats().contains(format)) {
            qCCritical(KWIN_WAYLAND_BACKEND) << "DRM_FORMAT_XRGB8888 is unsupported";
            return std::nullopt;
        }
        const QVector<uint64_t> modifiers = dmabuf->formats().value(format);
        m_swapchain = std::make_unique<WaylandEglLayerSwapchain>(nativeSize, format, modifiers, m_backend);
    }

    m_buffer = m_swapchain->acquire();
    if (!m_buffer) {
        return std::nullopt;
    }

    QRegion repair;
    if (m_backend->supportsBufferAge()) {
        repair = m_damageJournal.accumulate(m_buffer->age(), infiniteRegion());
    }

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer->framebuffer()),
        .repaint = repair,
    };
}

bool WaylandEglPrimaryLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    // Flush rendering commands to the dmabuf.
    glFlush();

    m_damageJournal.add(damagedRegion);
    return true;
}

void WaylandEglPrimaryLayer::present()
{
    wl_buffer *buffer = m_backend->backend()->importBuffer(m_buffer->graphicsBuffer());
    Q_ASSERT(buffer);

    KWayland::Client::Surface *surface = m_waylandOutput->surface();
    surface->attachBuffer(buffer);
    surface->damage(m_damageJournal.lastDamage());
    surface->setScale(std::ceil(m_waylandOutput->scale()));
    surface->commit();
    Q_EMIT m_waylandOutput->outputChange(m_damageJournal.lastDamage());

    m_swapchain->release(m_buffer);
}

WaylandEglCursorLayer::WaylandEglCursorLayer(WaylandOutput *output, WaylandEglBackend *backend)
    : m_output(output)
    , m_backend(backend)
{
}

WaylandEglCursorLayer::~WaylandEglCursorLayer()
{
    eglMakeCurrent(m_backend->eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, m_backend->context());
}

std::optional<OutputLayerBeginFrameInfo> WaylandEglCursorLayer::beginFrame()
{
    if (eglMakeCurrent(m_backend->eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, m_backend->context()) == EGL_FALSE) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "Make Context Current failed";
        return std::nullopt;
    }

    const auto tmp = size().expandedTo(QSize(64, 64));
    const QSize bufferSize(std::ceil(tmp.width()), std::ceil(tmp.height()));
    if (!m_swapchain || m_swapchain->size() != bufferSize) {
        const WaylandLinuxDmabufV1 *dmabuf = m_backend->backend()->display()->linuxDmabuf();
        const uint32_t format = DRM_FORMAT_ARGB8888;
        if (!dmabuf->formats().contains(format)) {
            qCCritical(KWIN_WAYLAND_BACKEND) << "DRM_FORMAT_ARGB8888 is unsupported";
            return std::nullopt;
        }
        const QVector<uint64_t> modifiers = dmabuf->formats().value(format);
        m_swapchain = std::make_unique<WaylandEglLayerSwapchain>(bufferSize, format, modifiers, m_backend);
    }

    m_buffer = m_swapchain->acquire();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer->framebuffer()),
        .repaint = infiniteRegion(),
    };
}

bool WaylandEglCursorLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    // Flush rendering commands to the dmabuf.
    glFlush();

    wl_buffer *buffer = m_backend->backend()->importBuffer(m_buffer->graphicsBuffer());
    Q_ASSERT(buffer);

    m_output->cursor()->update(buffer, scale(), hotspot().toPoint());

    m_swapchain->release(m_buffer);
    return true;
}

quint32 WaylandEglCursorLayer::format() const
{
    return m_buffer->graphicsBuffer()->dmabufAttributes()->format;
}

quint32 WaylandEglPrimaryLayer::format() const
{
    return m_buffer->graphicsBuffer()->dmabufAttributes()->format;
}

WaylandEglBackend::WaylandEglBackend(WaylandBackend *b)
    : AbstractEglBackend()
    , m_backend(b)
{
    // Egl is always direct rendering
    setIsDirectRendering(true);

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

std::shared_ptr<GLTexture> WaylandEglBackend::textureForOutput(KWin::Output *output) const
{
    return m_outputs.at(output).primaryLayer->texture();
}

std::unique_ptr<SurfaceTexture> WaylandEglBackend::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return std::make_unique<BasicEGLSurfaceTextureInternal>(this, pixmap);
}

std::unique_ptr<SurfaceTexture> WaylandEglBackend::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return std::make_unique<BasicEGLSurfaceTextureWayland>(this, pixmap);
}

void WaylandEglBackend::present(Output *output)
{
    m_outputs[output].primaryLayer->present();
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
