/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_windowed_egl_backend.h"
#include "core/gbmgraphicsbufferallocator.h"
#include "platformsupport/scenes/opengl/basiceglsurfacetexture_wayland.h"
#include "platformsupport/scenes/opengl/eglswapchain.h"
#include "platformsupport/scenes/opengl/glrendertimequery.h"
#include "x11_windowed_backend.h"
#include "x11_windowed_logging.h"
#include "x11_windowed_output.h"

#include <drm_fourcc.h>

namespace KWin
{

X11WindowedEglPrimaryLayer::X11WindowedEglPrimaryLayer(X11WindowedEglBackend *backend, X11WindowedOutput *output)
    : m_output(output)
    , m_backend(backend)
{
}

X11WindowedEglPrimaryLayer::~X11WindowedEglPrimaryLayer()
{
}

std::optional<OutputLayerBeginFrameInfo> X11WindowedEglPrimaryLayer::beginFrame()
{
    if (!m_backend->contextObject()->makeCurrent()) {
        return std::nullopt;
    }

    const QSize bufferSize = m_output->modeSize();
    if (!m_swapchain || m_swapchain->size() != bufferSize) {
        const uint32_t format = DRM_FORMAT_XRGB8888;
        const QHash<uint32_t, QList<uint64_t>> formatTable = m_backend->backend()->driFormats();
        if (!formatTable.contains(format)) {
            return std::nullopt;
        }
        m_swapchain = EglSwapchain::create(m_backend->graphicsBufferAllocator(), m_backend->contextObject(), bufferSize, format, formatTable[format]);
        if (!m_swapchain) {
            return std::nullopt;
        }
    }

    m_buffer = m_swapchain->acquire();
    if (!m_buffer) {
        return std::nullopt;
    }

    QRegion repaint = m_output->exposedArea() + m_output->rect();
    m_output->clearExposedArea();

    if (!m_query) {
        m_query = std::make_unique<GLRenderTimeQuery>();
    }
    m_query->begin();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer->framebuffer()),
        .repaint = repaint,
    };
}

bool X11WindowedEglPrimaryLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    m_query->end();
    return true;
}

void X11WindowedEglPrimaryLayer::present()
{
    xcb_pixmap_t pixmap = m_output->importBuffer(m_buffer->buffer());
    Q_ASSERT(pixmap != XCB_PIXMAP_NONE);

    xcb_xfixes_region_t valid = 0;
    xcb_xfixes_region_t update = 0;
    uint32_t serial = 0;
    uint32_t options = 0;
    uint64_t targetMsc = 0;

    xcb_present_pixmap(m_output->backend()->connection(),
                       m_output->window(),
                       pixmap,
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

    Q_EMIT m_output->outputChange(infiniteRegion());

    m_swapchain->release(m_buffer);
}

std::shared_ptr<GLTexture> X11WindowedEglPrimaryLayer::texture() const
{
    return m_buffer->texture();
}

quint32 X11WindowedEglPrimaryLayer::format() const
{
    return DRM_FORMAT_RGBA8888;
}

std::chrono::nanoseconds X11WindowedEglPrimaryLayer::queryRenderTime() const
{
    m_backend->makeCurrent();
    return m_query->result();
}

X11WindowedEglCursorLayer::X11WindowedEglCursorLayer(X11WindowedEglBackend *backend, X11WindowedOutput *output)
    : m_output(output)
    , m_backend(backend)
{
}

X11WindowedEglCursorLayer::~X11WindowedEglCursorLayer()
{
    m_backend->contextObject()->makeCurrent();
    m_framebuffer.reset();
    m_texture.reset();
}

std::optional<OutputLayerBeginFrameInfo> X11WindowedEglCursorLayer::beginFrame()
{
    if (!m_backend->contextObject()->makeCurrent()) {
        return std::nullopt;
    }

    const auto tmp = size().expandedTo(QSize(64, 64));
    const QSize bufferSize(std::ceil(tmp.width()), std::ceil(tmp.height()));
    if (!m_texture || m_texture->size() != bufferSize) {
        m_texture = GLTexture::allocate(GL_RGBA8, bufferSize);
        if (!m_texture) {
            return std::nullopt;
        }
        m_framebuffer = std::make_unique<GLFramebuffer>(m_texture.get());
    }
    if (!m_query) {
        m_query = std::make_unique<GLRenderTimeQuery>();
    }
    m_query->begin();
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_framebuffer.get()),
        .repaint = infiniteRegion(),
    };
}

bool X11WindowedEglCursorLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    QImage buffer(m_framebuffer->size(), QImage::Format_RGBA8888_Premultiplied);

    GLFramebuffer::pushFramebuffer(m_framebuffer.get());
    glReadPixels(0, 0, buffer.width(), buffer.height(), GL_RGBA, GL_UNSIGNED_BYTE, buffer.bits());
    GLFramebuffer::popFramebuffer();

    m_output->cursor()->update(buffer.mirrored(false, true), hotspot());
    m_query->end();

    return true;
}

quint32 X11WindowedEglCursorLayer::format() const
{
    return DRM_FORMAT_RGBA8888;
}

std::chrono::nanoseconds X11WindowedEglCursorLayer::queryRenderTime() const
{
    m_backend->makeCurrent();
    return m_query->result();
}

X11WindowedEglBackend::X11WindowedEglBackend(X11WindowedBackend *backend)
    : m_allocator(std::make_unique<GbmGraphicsBufferAllocator>(backend->gbmDevice()))
    , m_backend(backend)
{
}

X11WindowedEglBackend::~X11WindowedEglBackend()
{
    cleanup();
}

X11WindowedBackend *X11WindowedEglBackend::backend() const
{
    return m_backend;
}

GraphicsBufferAllocator *X11WindowedEglBackend::graphicsBufferAllocator() const
{
    return m_allocator.get();
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

        m_backend->setEglDisplay(EglDisplay::create(eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, m_backend->gbmDevice(), nullptr)));
    }

    auto display = m_backend->sceneEglDisplayObject();
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

    return makeCurrent();
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

    initKWinGL();
    initWayland();

    const auto &outputs = m_backend->outputs();
    for (const auto &output : outputs) {
        X11WindowedOutput *x11Output = static_cast<X11WindowedOutput *>(output);
        m_outputs[output] = Layers{
            .primaryLayer = std::make_unique<X11WindowedEglPrimaryLayer>(this, x11Output),
            .cursorLayer = std::make_unique<X11WindowedEglCursorLayer>(this, x11Output),
        };
    }
}

void X11WindowedEglBackend::cleanupSurfaces()
{
    m_outputs.clear();
}

void X11WindowedEglBackend::present(Output *output, const std::shared_ptr<OutputFrame> &frame)
{
    m_outputs[output].primaryLayer->present();
    static_cast<X11WindowedOutput *>(output)->framePending(frame);
}

OutputLayer *X11WindowedEglBackend::primaryLayer(Output *output)
{
    return m_outputs[output].primaryLayer.get();
}

OutputLayer *X11WindowedEglBackend::cursorLayer(Output *output)
{
    return m_outputs[output].cursorLayer.get();
}

std::unique_ptr<SurfaceTexture> X11WindowedEglBackend::createSurfaceTextureWayland(SurfacePixmap *pixmap)
{
    return std::make_unique<BasicEGLSurfaceTextureWayland>(this, pixmap);
}

std::pair<std::shared_ptr<GLTexture>, ColorDescription> X11WindowedEglBackend::textureForOutput(Output *output) const
{
    auto it = m_outputs.find(output);
    if (it == m_outputs.end()) {
        return {nullptr, ColorDescription::sRGBf()};
    }
    return std::make_pair(it->second.primaryLayer->texture(), ColorDescription::sRGBf());
}

} // namespace

#include "moc_x11_windowed_egl_backend.cpp"
