/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_egl_backend.h"
#include "core/gbmgraphicsbufferallocator.h"
#include "libkwineffects/kwinglutils.h"
#include "platformsupport/scenes/opengl/basiceglsurfacetexture_internal.h"
#include "platformsupport/scenes/opengl/basiceglsurfacetexture_wayland.h"
#include "utils/softwarevsyncmonitor.h"
#include "virtual_backend.h"
#include "virtual_logging.h"
#include "virtual_output.h"

#include <drm_fourcc.h>

namespace KWin
{

VirtualEglLayerBuffer::VirtualEglLayerBuffer(GbmGraphicsBuffer *buffer, VirtualEglBackend *backend)
    : m_graphicsBuffer(buffer)
{
    m_texture = backend->importDmaBufAsTexture(*buffer->dmabufAttributes());
    m_framebuffer = std::make_unique<GLFramebuffer>(m_texture.get());
}

VirtualEglLayerBuffer::~VirtualEglLayerBuffer()
{
    m_texture.reset();
    m_framebuffer.reset();
    m_graphicsBuffer->drop();
}

GbmGraphicsBuffer *VirtualEglLayerBuffer::graphicsBuffer() const
{
    return m_graphicsBuffer;
}

GLFramebuffer *VirtualEglLayerBuffer::framebuffer() const
{
    return m_framebuffer.get();
}

std::shared_ptr<GLTexture> VirtualEglLayerBuffer::texture() const
{
    return m_texture;
}

VirtualEglSwapchain::VirtualEglSwapchain(const QSize &size, uint32_t format, VirtualEglBackend *backend)
    : m_backend(backend)
    , m_size(size)
    , m_format(format)
    , m_allocator(std::make_unique<GbmGraphicsBufferAllocator>(backend->backend()->gbmDevice()))
{
}

QSize VirtualEglSwapchain::size() const
{
    return m_size;
}

std::shared_ptr<VirtualEglLayerBuffer> VirtualEglSwapchain::acquire()
{
    for (const auto &buffer : std::as_const(m_buffers)) {
        if (!buffer->graphicsBuffer()->isReferenced()) {
            return buffer;
        }
    }

    GbmGraphicsBuffer *graphicsBuffer = m_allocator->allocate(m_size, m_format);
    if (!graphicsBuffer) {
        qCWarning(KWIN_VIRTUAL) << "Failed to allocate layer swapchain buffer";
        return nullptr;
    }

    auto buffer = std::make_shared<VirtualEglLayerBuffer>(graphicsBuffer, m_backend);
    m_buffers.append(buffer);

    return buffer;
}

VirtualEglLayer::VirtualEglLayer(Output *output, VirtualEglBackend *backend)
    : m_backend(backend)
    , m_output(output)
{
}

std::shared_ptr<GLTexture> VirtualEglLayer::texture() const
{
    return m_current->texture();
}

std::optional<OutputLayerBeginFrameInfo> VirtualEglLayer::beginFrame()
{
    m_backend->makeCurrent();

    const QSize nativeSize = m_output->geometry().size() * m_output->scale();
    if (!m_swapchain || m_swapchain->size() != nativeSize) {
        m_swapchain = std::make_unique<VirtualEglSwapchain>(nativeSize, DRM_FORMAT_XRGB8888, m_backend);
    }

    m_current = m_swapchain->acquire();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_current->framebuffer()),
        .repaint = infiniteRegion(),
    };
}

bool VirtualEglLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    glFlush(); // flush pending rendering commands.
    Q_EMIT m_output->outputChange(damagedRegion);
    return true;
}

quint32 VirtualEglLayer::format() const
{
    return DRM_FORMAT_XRGB8888;
}

VirtualEglBackend::VirtualEglBackend(VirtualBackend *b)
    : AbstractEglBackend()
    , m_backend(b)
{
    // Egl is always direct rendering
    setIsDirectRendering(true);
}

VirtualEglBackend::~VirtualEglBackend()
{
    m_outputs.clear();
    cleanup();
}

VirtualBackend *VirtualEglBackend::backend() const
{
    return m_backend;
}

bool VirtualEglBackend::initializeEgl()
{
    initClientExtensions();

    if (!m_backend->sceneEglDisplayObject()) {
        for (const QByteArray &extension : {QByteArrayLiteral("EGL_EXT_platform_base"), QByteArrayLiteral("EGL_KHR_platform_gbm")}) {
            if (!hasClientExtension(extension)) {
                qCWarning(KWIN_VIRTUAL) << extension << "client extension is not supported by the platform";
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

void VirtualEglBackend::init()
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
    if (checkGLError("Init")) {
        setFailed("Error during init of EglGbmBackend");
        return;
    }

    setSupportsBufferAge(false);
    initWayland();

    const auto outputs = m_backend->outputs();
    for (Output *output : outputs) {
        addOutput(output);
    }

    connect(m_backend, &VirtualBackend::outputAdded, this, &VirtualEglBackend::addOutput);
    connect(m_backend, &VirtualBackend::outputRemoved, this, &VirtualEglBackend::removeOutput);
}

bool VirtualEglBackend::initRenderingContext()
{
    return createContext(EGL_NO_CONFIG_KHR) && makeCurrent();
}

void VirtualEglBackend::addOutput(Output *output)
{
    makeCurrent();
    m_outputs[output] = std::make_unique<VirtualEglLayer>(output, this);
}

void VirtualEglBackend::removeOutput(Output *output)
{
    makeCurrent();
    m_outputs.erase(output);
}

std::unique_ptr<SurfaceTexture> VirtualEglBackend::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return std::make_unique<BasicEGLSurfaceTextureInternal>(this, pixmap);
}

std::unique_ptr<SurfaceTexture> VirtualEglBackend::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return std::make_unique<BasicEGLSurfaceTextureWayland>(this, pixmap);
}

OutputLayer *VirtualEglBackend::primaryLayer(Output *output)
{
    return m_outputs[output].get();
}

void VirtualEglBackend::present(Output *output)
{
    static_cast<VirtualOutput *>(output)->vsyncMonitor()->arm();
}

std::shared_ptr<GLTexture> VirtualEglBackend::textureForOutput(Output *output) const
{
    auto it = m_outputs.find(output);
    if (it == m_outputs.end()) {
        return nullptr;
    }
    return it->second->texture();
}

} // namespace
