/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "wayland_egl_backend.h"
#include "basiceglsurfacetexture_internal.h"
#include "basiceglsurfacetexture_wayland.h"
#include "../drm/gbm_dmabuf.h"

#include "wayland_backend.h"
#include "wayland_display.h"
#include "wayland_logging.h"
#include "wayland_output.h"

#include <fcntl.h>
#include <unistd.h>

// kwin libs
#include <kwinglplatform.h>
#include <kwinglutils.h>

// KDE
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

// Qt
#include <QFile>
#include <QOpenGLContext>

#include <cmath>
#include <drm_fourcc.h>
#include <gbm.h>

#include "wayland-linux-dmabuf-unstable-v1-client-protocol.h"

namespace KWin
{
namespace Wayland
{

WaylandEglLayerBuffer::WaylandEglLayerBuffer(const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers, WaylandEglBackend *backend)
    : m_backend(backend)
{
    gbm_device *gbmDevice = backend->backend()->gbmDevice();

    if (!modifiers.isEmpty()) {
        m_bo = gbm_bo_create_with_modifiers(gbmDevice,
                                            size.width(),
                                            size.height(),
                                            format,
                                            modifiers.constData(),
                                            modifiers.size());
    }

    if (!m_bo) {
        m_bo = gbm_bo_create(gbmDevice,
                             size.width(),
                             size.height(),
                             format,
                             GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    }

    if (!m_bo) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "Failed to allocate a buffer for an output layer";
        return;
    }

    DmaBufAttributes attributes = dmaBufAttributesForBo(m_bo);

    zwp_linux_buffer_params_v1 *params = zwp_linux_dmabuf_v1_create_params(backend->backend()->display()->linuxDmabuf()->handle());
    for (int i = 0; i < attributes.planeCount; ++i) {
        zwp_linux_buffer_params_v1_add(params,
                                       attributes.fd[i].get(),
                                       i,
                                       attributes.offset[i],
                                       attributes.pitch[i],
                                       attributes.modifier >> 32,
                                       attributes.modifier & 0xffffffff);
    }

    m_buffer = zwp_linux_buffer_params_v1_create_immed(params, size.width(), size.height(), format, 0);
    zwp_linux_buffer_params_v1_destroy(params);

    m_texture = backend->importDmaBufAsTexture(std::move(attributes));
    m_shadowTexture = std::make_unique<GLTexture>(m_texture->internalFormat(), m_texture->size());
    m_framebuffer = std::make_unique<GLFramebuffer>(m_texture.get());
    m_shadowFramebuffer = std::make_unique<GLFramebuffer>(m_shadowTexture.get());
}

WaylandEglLayerBuffer::~WaylandEglLayerBuffer()
{
    m_texture.reset();
    m_framebuffer.reset();
    m_shadowTexture.reset();
    m_shadowFramebuffer.reset();

    if (m_buffer) {
        wl_buffer_destroy(m_buffer);
    }
    if (m_bo) {
        gbm_bo_destroy(m_bo);
    }
}

wl_buffer *WaylandEglLayerBuffer::buffer() const
{
    return m_buffer;
}

GLFramebuffer *WaylandEglLayerBuffer::framebuffer() const
{
    return m_framebuffer.get();
}

int WaylandEglLayerBuffer::age() const
{
    return m_age;
}

GLFramebuffer *WaylandEglLayerBuffer::shadowFramebuffer() const
{
    return m_shadowFramebuffer.get();
}

GLTexture *WaylandEglLayerBuffer::shadowTexture() const
{
    return m_shadowTexture.get();
}

WaylandEglLayerSwapchain::WaylandEglLayerSwapchain(const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers, WaylandEglBackend *backend)
    : m_backend(backend)
    , m_size(size)
{
    for (int i = 0; i < 2; ++i) {
        m_buffers.append(std::make_shared<WaylandEglLayerBuffer>(size, format, modifiers, backend));
    }
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
    m_index = (m_index + 1) % m_buffers.count();
    return m_buffers[m_index];
}

void WaylandEglLayerSwapchain::release(std::shared_ptr<WaylandEglLayerBuffer> buffer)
{
    Q_ASSERT(m_buffers[m_index] == buffer);

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

std::optional<OutputLayerBeginFrameInfo> WaylandEglPrimaryLayer::beginFrame()
{
    if (eglMakeCurrent(m_backend->eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, m_backend->context()) == EGL_FALSE) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "Make Context Current failed";
        return std::nullopt;
    }

    const QSize nativeSize = m_waylandOutput->pixelSize();
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

    QRegion repair;
    if (m_backend->supportsBufferAge()) {
        repair = m_damageJournal.accumulate(m_buffer->age(), infiniteRegion());
    }

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer->shadowFramebuffer()),
        .repaint = repair,
    };
}

bool WaylandEglPrimaryLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    GLFramebuffer::pushFramebuffer(m_buffer->framebuffer());
    auto texture = m_buffer->shadowTexture();
    QRect outputGeometry(0, 0, texture->size().width(), texture->size().height());

    ShaderBinder shaderBinder(ShaderTrait::MapTexture);
    QMatrix4x4 projectionMatrix;
    projectionMatrix.scale(1, -1);
    projectionMatrix.ortho(outputGeometry);
    shaderBinder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, projectionMatrix);

    texture->bind();
    texture->render(outputGeometry, 1);
    texture->unbind();
    GLFramebuffer::popFramebuffer();
    // Flush rendering commands to the dmabuf.
    glFlush();

    m_damageJournal.add(damagedRegion);
    return true;
}

void WaylandEglPrimaryLayer::present()
{
    KWayland::Client::Surface *surface = m_waylandOutput->surface();
    surface->attachBuffer(m_buffer->buffer());
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

qreal WaylandEglCursorLayer::scale() const
{
    return m_scale;
}

void WaylandEglCursorLayer::setScale(qreal scale)
{
    m_scale = scale;
}

QPoint WaylandEglCursorLayer::hotspot() const
{
    return m_hotspot;
}

void WaylandEglCursorLayer::setHotspot(const QPoint &hotspot)
{
    m_hotspot = hotspot;
}

QSize WaylandEglCursorLayer::size() const
{
    return m_size;
}

void WaylandEglCursorLayer::setSize(const QSize &size)
{
    m_size = size;
}

std::optional<OutputLayerBeginFrameInfo> WaylandEglCursorLayer::beginFrame()
{
    if (eglMakeCurrent(m_backend->eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, m_backend->context()) == EGL_FALSE) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "Make Context Current failed";
        return std::nullopt;
    }

    const QSize bufferSize = m_size.expandedTo(QSize(64, 64));
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
        .renderTarget = RenderTarget(m_buffer->shadowFramebuffer()),
        .repaint = infiniteRegion(),
    };
}

bool WaylandEglCursorLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    GLFramebuffer::pushFramebuffer(m_buffer->framebuffer());
    auto texture = m_buffer->shadowTexture();
    QRect outputGeometry(0, 0, texture->size().width(), texture->size().height());

    ShaderBinder shaderBinder(ShaderTrait::MapTexture);
    QMatrix4x4 projectionMatrix;
    projectionMatrix.scale(1, -1);
    projectionMatrix.ortho(outputGeometry);
    shaderBinder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, projectionMatrix);

    texture->bind();
    texture->render(outputGeometry, 1);
    texture->unbind();
    GLFramebuffer::popFramebuffer();

    // Flush rendering commands to the dmabuf.
    glFlush();

    m_output->cursor()->update(m_buffer->buffer(), m_scale, m_hotspot);

    m_swapchain->release(m_buffer);
    return true;
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
    EGLDisplay display = m_backend->sceneEglDisplay();

    // Use eglGetPlatformDisplayEXT() to get the display pointer
    // if the implementation supports it.
    if (display == EGL_NO_DISPLAY) {
        m_havePlatformBase = hasClientExtension(QByteArrayLiteral("EGL_EXT_platform_base"));
        if (m_havePlatformBase) {
            // Make sure that the wayland platform is supported
            if (!hasClientExtension(QByteArrayLiteral("EGL_EXT_platform_wayland"))) {
                return false;
            }

            display = eglGetPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_EXT, m_backend->display()->nativeDisplay(), nullptr);
        } else {
            display = eglGetDisplay(m_backend->display()->nativeDisplay());
        }
    }

    if (display == EGL_NO_DISPLAY) {
        return false;
    }
    setEglDisplay(display);
    return initEglAPI();
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
    initBufferAge();
    initWayland();
}

bool WaylandEglBackend::initRenderingContext()
{
    initBufferConfigs();

    if (!createContext()) {
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

bool WaylandEglBackend::initBufferConfigs()
{
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE,
        EGL_WINDOW_BIT,
        EGL_RED_SIZE,
        1,
        EGL_GREEN_SIZE,
        1,
        EGL_BLUE_SIZE,
        1,
        EGL_ALPHA_SIZE,
        0,
        EGL_RENDERABLE_TYPE,
        isOpenGLES() ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_BIT,
        EGL_CONFIG_CAVEAT,
        EGL_NONE,
        EGL_NONE,
    };

    EGLint count;
    EGLConfig configs[1024];
    if (eglChooseConfig(eglDisplay(), config_attribs, configs, 1, &count) == EGL_FALSE) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "choose config failed";
        return false;
    }
    if (count != 1) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "choose config did not return a config" << count;
        return false;
    }
    setConfig(configs[0]);

    return true;
}

std::shared_ptr<KWin::GLTexture> WaylandEglBackend::textureForOutput(KWin::Output *output) const
{
    auto texture = std::make_unique<GLTexture>(GL_RGBA8, output->pixelSize());
    GLFramebuffer::pushFramebuffer(m_outputs.at(output).primaryLayer->fbo());
    GLFramebuffer renderTarget(texture.get());
    renderTarget.blitFromFramebuffer(QRect(0, texture->height(), texture->width(), -texture->height()));
    GLFramebuffer::popFramebuffer();
    return texture;
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

WaylandEglCursorLayer *WaylandEglBackend::cursorLayer(Output *output)
{
    return m_outputs[output].cursorLayer.get();
}

}
}
