/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_windowed_egl_backend.h"
// kwin
#include "basiceglsurfacetexture_internal.h"
#include "basiceglsurfacetexture_wayland.h"
#include "x11_windowed_backend.h"
#include "x11_windowed_output.h"
// kwin libs
#include <kwinglplatform.h>

namespace KWin
{

X11WindowedEglPrimaryLayer::X11WindowedEglPrimaryLayer(X11WindowedEglBackend *backend, X11WindowedOutput *output, EGLSurface surface)
    : m_eglSurface(surface)
    , m_output(output)
    , m_backend(backend)
{
}

X11WindowedEglPrimaryLayer::~X11WindowedEglPrimaryLayer()
{
    eglDestroySurface(m_backend->eglDisplay(), m_eglSurface);
}

void X11WindowedEglPrimaryLayer::ensureFbo()
{
    if (!m_fbo || m_fbo->size() != m_output->pixelSize()) {
        m_fbo = std::make_unique<GLFramebuffer>(0, m_output->pixelSize());
    }
}

std::optional<OutputLayerBeginFrameInfo> X11WindowedEglPrimaryLayer::beginFrame()
{
    eglMakeCurrent(m_backend->eglDisplay(), m_eglSurface, m_eglSurface, m_backend->context());
    ensureFbo();

    QRegion repaint = m_output->exposedArea() + m_output->rect();
    m_output->clearExposedArea();

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_fbo.get()),
        .repaint = repaint,
    };
}

bool X11WindowedEglPrimaryLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    m_lastDamage = damagedRegion;
    return true;
}

EGLSurface X11WindowedEglPrimaryLayer::surface() const
{
    return m_eglSurface;
}

QRegion X11WindowedEglPrimaryLayer::lastDamage() const
{
    return m_lastDamage;
}

X11WindowedEglCursorLayer::X11WindowedEglCursorLayer(X11WindowedEglBackend *backend, X11WindowedOutput *output)
    : m_output(output)
    , m_backend(backend)
{
}

X11WindowedEglCursorLayer::~X11WindowedEglCursorLayer()
{
    eglMakeCurrent(m_backend->eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, m_backend->context());
    m_framebuffer.reset();
    m_texture.reset();
}

QPoint X11WindowedEglCursorLayer::hotspot() const
{
    return m_hotspot;
}

void X11WindowedEglCursorLayer::setHotspot(const QPoint &hotspot)
{
    m_hotspot = hotspot;
}

QSize X11WindowedEglCursorLayer::size() const
{
    return m_size;
}

void X11WindowedEglCursorLayer::setSize(const QSize &size)
{
    m_size = size;
}

std::optional<OutputLayerBeginFrameInfo> X11WindowedEglCursorLayer::beginFrame()
{
    eglMakeCurrent(m_backend->eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, m_backend->context());

    const QSize bufferSize = m_size.expandedTo(QSize(64, 64));
    if (!m_texture || m_texture->size() != bufferSize) {
        m_texture = std::make_unique<GLTexture>(GL_RGBA8, bufferSize);
        m_framebuffer = std::make_unique<GLFramebuffer>(m_texture.get());
    }

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

    m_output->cursor()->update(buffer.mirrored(false, true), m_hotspot);

    return true;
}

X11WindowedEglBackend::X11WindowedEglBackend(X11WindowedBackend *backend)
    : EglOnXBackend(backend->connection(), backend->display(), backend->rootWindow())
    , m_backend(backend)
{
}

X11WindowedEglBackend::~X11WindowedEglBackend()
{
    cleanup();
}

void X11WindowedEglBackend::init()
{
    EglOnXBackend::init();

    if (!isFailed()) {
        initWayland();
    }
}

void X11WindowedEglBackend::cleanupSurfaces()
{
    m_outputs.clear();
}

bool X11WindowedEglBackend::createSurfaces()
{
    const auto &outputs = m_backend->outputs();
    for (const auto &output : outputs) {
        X11WindowedOutput *x11Output = static_cast<X11WindowedOutput *>(output);
        EGLSurface s = createSurface(x11Output->window());
        if (s == EGL_NO_SURFACE) {
            return false;
        }
        m_outputs[output] = Layers{
            .primaryLayer = std::make_unique<X11WindowedEglPrimaryLayer>(this, x11Output, s),
            .cursorLayer = std::make_unique<X11WindowedEglCursorLayer>(this, x11Output),
        };
    }
    if (m_outputs.empty()) {
        return false;
    }
    return true;
}

void X11WindowedEglBackend::present(Output *output)
{
    const auto &renderOutput = m_outputs[output];
    presentSurface(renderOutput.primaryLayer->surface(), renderOutput.primaryLayer->lastDamage(), output->geometry());
}

void X11WindowedEglBackend::presentSurface(EGLSurface surface, const QRegion &damage, const QRect &screenGeometry)
{
    const bool fullRepaint = supportsBufferAge() || (damage == screenGeometry);

    if (fullRepaint || !havePostSubBuffer()) {
        // the entire screen changed, or we cannot do partial updates (which implies we enabled surface preservation)
        eglSwapBuffers(eglDisplay(), surface);
    } else {
        // a part of the screen changed, and we can use eglPostSubBufferNV to copy the updated area
        for (const QRect &r : damage) {
            eglPostSubBufferNV(eglDisplay(), surface, r.left(), screenGeometry.height() - r.bottom() - 1, r.width(), r.height());
        }
    }
}

OutputLayer *X11WindowedEglBackend::primaryLayer(Output *output)
{
    return m_outputs[output].primaryLayer.get();
}

X11WindowedEglCursorLayer *X11WindowedEglBackend::cursorLayer(Output *output)
{
    return m_outputs[output].cursorLayer.get();
}

std::unique_ptr<SurfaceTexture> X11WindowedEglBackend::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return std::make_unique<BasicEGLSurfaceTextureWayland>(this, pixmap);
}

std::unique_ptr<SurfaceTexture> X11WindowedEglBackend::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return std::make_unique<BasicEGLSurfaceTextureInternal>(this, pixmap);
}

} // namespace
