/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_windowed_egl_backend.h"
#include "core/gbmgraphicsbufferallocator.h"
#include "opengl/eglnativefence.h"
#include "opengl/eglswapchain.h"
#include "opengl/glrendertimequery.h"
#include "x11_windowed_backend.h"
#include "x11_windowed_logging.h"
#include "x11_windowed_output.h"

#include <drm_fourcc.h>

namespace KWin
{

X11WindowedEglPrimaryLayer::X11WindowedEglPrimaryLayer(X11WindowedEglBackend *backend, X11WindowedOutput *output)
    : OutputLayer(output, OutputLayerType::Primary)
    , m_output(output)
    , m_backend(backend)
{
}

X11WindowedEglPrimaryLayer::~X11WindowedEglPrimaryLayer()
{
}

std::optional<OutputLayerBeginFrameInfo> X11WindowedEglPrimaryLayer::doBeginFrame()
{
    if (!m_backend->openglContext()->makeCurrent()) {
        return std::nullopt;
    }

    const QSize bufferSize = m_output->modeSize();
    if (!m_swapchain || m_swapchain->size() != bufferSize) {
        const uint32_t format = DRM_FORMAT_XRGB8888;
        const QHash<uint32_t, QList<uint64_t>> formatTable = m_backend->backend()->driFormats();
        if (!formatTable.contains(format)) {
            return std::nullopt;
        }
        m_swapchain = EglSwapchain::create(m_backend->drmDevice()->allocator(), m_backend->openglContext(), bufferSize, format, formatTable[format]);
        if (!m_swapchain) {
            return std::nullopt;
        }
    }

    m_buffer = m_swapchain->acquire();
    if (!m_buffer) {
        return std::nullopt;
    }

    QRegion repaint = infiniteRegion();
    m_output->clearExposedArea();

    m_query = std::make_unique<GLRenderTimeQuery>(m_backend->openglContextRef());
    m_query->begin();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer->framebuffer()),
        .repaint = repaint,
    };
}

bool X11WindowedEglPrimaryLayer::doEndFrame(const QRegion &renderedDeviceRegion, const QRegion &damagedDeviceRegion, OutputFrame *frame)
{
    m_query->end();
    frame->addRenderTimeQuery(std::move(m_query));
    EGLNativeFence releaseFence{m_backend->eglDisplayObject()};
    m_swapchain->release(m_buffer, releaseFence.fileDescriptor().duplicate());
    m_output->setPrimaryBuffer(m_buffer->buffer());
    return true;
}

DrmDevice *X11WindowedEglPrimaryLayer::scanoutDevice() const
{
    return m_backend->drmDevice();
}

QHash<uint32_t, QList<uint64_t>> X11WindowedEglPrimaryLayer::supportedDrmFormats() const
{
    return m_backend->backend()->driFormats();
}

void X11WindowedEglPrimaryLayer::releaseBuffers()
{
    m_buffer.reset();
    m_swapchain.reset();
}

X11WindowedEglCursorLayer::X11WindowedEglCursorLayer(X11WindowedEglBackend *backend, X11WindowedOutput *output)
    : OutputLayer(output, OutputLayerType::CursorOnly)
    , m_backend(backend)
{
}

X11WindowedEglCursorLayer::~X11WindowedEglCursorLayer()
{
    m_backend->openglContext()->makeCurrent();
    m_framebuffer.reset();
    m_texture.reset();
}

std::optional<OutputLayerBeginFrameInfo> X11WindowedEglCursorLayer::doBeginFrame()
{
    if (!m_backend->openglContext()->makeCurrent()) {
        return std::nullopt;
    }

    const auto bufferSize = targetRect().size();
    if (!m_texture || m_texture->size() != bufferSize) {
        m_texture = GLTexture::allocate(GL_RGBA8, bufferSize);
        if (!m_texture) {
            return std::nullopt;
        }
        m_framebuffer = std::make_unique<GLFramebuffer>(m_texture.get());
    }
    if (!m_query) {
        m_query = std::make_unique<GLRenderTimeQuery>(m_backend->openglContextRef());
    }
    m_query->begin();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_framebuffer.get()),
        .repaint = infiniteRegion(),
    };
}

bool X11WindowedEglCursorLayer::doEndFrame(const QRegion &renderedDeviceRegion, const QRegion &damagedDeviceRegion, OutputFrame *frame)
{
    EglContext *context = m_backend->openglContext();
    QImage buffer(m_framebuffer->size(), QImage::Format_RGBA8888_Premultiplied);

    GLFramebuffer::pushFramebuffer(m_framebuffer.get());
    context->glReadnPixels(0, 0, buffer.width(), buffer.height(), GL_RGBA, GL_UNSIGNED_BYTE, buffer.sizeInBytes(), buffer.bits());
    GLFramebuffer::popFramebuffer();

    static_cast<X11WindowedOutput *>(m_output.get())->cursor()->update(buffer.mirrored(false, true), hotspot());
    m_query->end();
    if (frame) {
        frame->addRenderTimeQuery(std::move(m_query));
    }

    return true;
}

DrmDevice *X11WindowedEglCursorLayer::scanoutDevice() const
{
    return m_backend->drmDevice();
}

QHash<uint32_t, QList<uint64_t>> X11WindowedEglCursorLayer::supportedDrmFormats() const
{
    return m_backend->supportedFormats();
}

void X11WindowedEglCursorLayer::releaseBuffers()
{
}

X11WindowedEglBackend::X11WindowedEglBackend(X11WindowedBackend *backend)
    : m_backend(backend)
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

DrmDevice *X11WindowedEglBackend::drmDevice() const
{
    return m_backend->drmDevice();
}

bool X11WindowedEglBackend::initializeEgl()
{
    initClientExtensions();

    if (!m_backend->sceneEglDisplayObject()) {
        for (const QByteArray &extension : {QByteArrayLiteral("EGL_EXT_platform_base"), QByteArrayLiteral("EGL_KHR_platform_gbm")}) {
            if (!hasClientExtension(extension)) {
                qCWarning(KWIN_X11WINDOWED) << extension << "client extension is not supported by the platform";
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

bool X11WindowedEglBackend::initRenderingContext()
{
    if (!createContext(EGL_NO_CONFIG_KHR)) {
        return false;
    }

    return openglContext()->makeCurrent();
}

void X11WindowedEglBackend::init()
{
    qputenv("EGL_PLATFORM", "x11");

    if (!initializeEgl()) {
        setFailed(QStringLiteral("Could not initialize egl"));
        return;
    }
    if (!initRenderingContext()) {
        setFailed(QStringLiteral("Could not initialize rendering context"));
        return;
    }

    initWayland();

    const auto &outputs = m_backend->outputs();
    for (const auto &output : outputs) {
        X11WindowedOutput *x11Output = static_cast<X11WindowedOutput *>(output);
        std::vector<std::unique_ptr<OutputLayer>> layers;
        layers.push_back(std::make_unique<X11WindowedEglPrimaryLayer>(this, x11Output));
        layers.push_back(std::make_unique<X11WindowedEglCursorLayer>(this, x11Output));
        x11Output->setOutputLayers(std::move(layers));
    }
}

QList<OutputLayer *> X11WindowedEglBackend::compatibleOutputLayers(BackendOutput *output)
{
    return static_cast<X11WindowedOutput *>(output)->outputLayers();
}

} // namespace

#include "moc_x11_windowed_egl_backend.cpp"
