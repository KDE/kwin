/*
    SPDX-FileCopyrightText: 2010, 2012 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "x11_standalone_egl_backend.h"
#include "core/outputbackend.h"
#include "core/overlaywindow.h"
#include "core/renderloop_p.h"
#include "kwinglplatform.h"
#include "options.h"
#include "scene/surfaceitem_x11.h"
#include "scene/workspacescene.h"
#include "softwarevsyncmonitor.h"
#include "workspace.h"
#include "x11_standalone_backend.h"
#include "x11_standalone_logging.h"
#include "x11_standalone_overlaywindow.h"

#include <QOpenGLContext>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtPlatformHeaders/QEGLNativeContext>
#endif

namespace KWin
{

EglLayer::EglLayer(EglBackend *backend)
    : m_backend(backend)
{
}

std::optional<OutputLayerBeginFrameInfo> EglLayer::beginFrame()
{
    return m_backend->beginFrame();
}

bool EglLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    m_backend->endFrame(renderedRegion, damagedRegion);
    return true;
}

EglBackend::EglBackend(Display *display, X11StandaloneBackend *backend)
    : EglOnXBackend(kwinApp()->x11Connection(), display, kwinApp()->x11RootWindow())
    , m_backend(backend)
    , m_overlayWindow(std::make_unique<OverlayWindowX11>())
    , m_layer(std::make_unique<EglLayer>(this))
{
    // There is no any way to determine when a buffer swap completes with EGL. Fallback
    // to software vblank events. Could we use the Present extension to get notified when
    // the overlay window is actually presented on the screen?
    m_vsyncMonitor = SoftwareVsyncMonitor::create();
    connect(backend->renderLoop(), &RenderLoop::refreshRateChanged, this, [this, backend]() {
        m_vsyncMonitor->setRefreshRate(backend->renderLoop()->refreshRate());
    });
    m_vsyncMonitor->setRefreshRate(backend->renderLoop()->refreshRate());

    connect(m_vsyncMonitor.get(), &VsyncMonitor::vblankOccurred, this, &EglBackend::vblank);
    connect(workspace(), &Workspace::geometryChanged, this, &EglBackend::screenGeometryChanged);
}

EglBackend::~EglBackend()
{
    // No completion events will be received for in-flight frames, this may lock the
    // render loop. We need to ensure that the render loop is back to its initial state
    // if the render backend is about to be destroyed.
    RenderLoopPrivate::get(m_backend->renderLoop())->invalidate();

    if (isFailed() && m_overlayWindow) {
        m_overlayWindow->destroy();
    }
    cleanup();

    if (m_overlayWindow && m_overlayWindow->window()) {
        m_overlayWindow->destroy();
    }
}

std::unique_ptr<SurfaceTexture> EglBackend::createSurfaceTextureX11(SurfacePixmapX11 *texture)
{
    return std::make_unique<EglSurfaceTextureX11>(this, texture);
}

void EglBackend::init()
{
    QOpenGLContext *qtShareContext = QOpenGLContext::globalShareContext();
    EGLDisplay shareDisplay = EGL_NO_DISPLAY;
    EGLContext shareContext = EGL_NO_CONTEXT;
    if (qtShareContext) {
        qDebug(KWIN_X11STANDALONE) << "Global share context format:" << qtShareContext->format();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        const QVariant nativeHandle = qtShareContext->nativeHandle();
        if (!nativeHandle.canConvert<QEGLNativeContext>()) {
            setFailed(QStringLiteral("Invalid QOpenGLContext::globalShareContext()"));
            return;
        } else {
            QEGLNativeContext handle = qvariant_cast<QEGLNativeContext>(nativeHandle);
            shareContext = handle.context();
            shareDisplay = handle.display();
        }
#else
        const auto nativeHandle = qtShareContext->nativeInterface<QNativeInterface::QEGLContext>();
        if (nativeHandle) {
            shareContext = nativeHandle->nativeContext();
            shareDisplay = nativeHandle->display();
        } else {
            setFailed(QStringLiteral("Invalid QOpenGLContext::globalShareContext()"));
            return;
        }
#endif
    }
    if (shareContext == EGL_NO_CONTEXT) {
        setFailed(QStringLiteral("QOpenGLContext::globalShareContext() is required"));
        return;
    }

    m_fbo = std::make_unique<GLFramebuffer>(0, workspace()->geometry().size());

    kwinApp()->outputBackend()->setSceneEglDisplay(shareDisplay);
    kwinApp()->outputBackend()->setSceneEglGlobalShareContext(shareContext);
    EglOnXBackend::init();
}

bool EglBackend::createSurfaces()
{
    if (!m_overlayWindow) {
        return false;
    }

    if (!m_overlayWindow->create()) {
        qCCritical(KWIN_X11STANDALONE) << "Could not get overlay window";
        return false;
    } else {
        m_overlayWindow->setup(XCB_WINDOW_NONE);
    }

    EGLSurface surface = createSurface(m_overlayWindow->window());
    if (surface == EGL_NO_SURFACE) {
        return false;
    }
    setSurface(surface);
    return true;
}

void EglBackend::screenGeometryChanged()
{
    overlayWindow()->resize(workspace()->geometry().size());

    // The back buffer contents are now undefined
    m_bufferAge = 0;
    m_fbo = std::make_unique<GLFramebuffer>(0, workspace()->geometry().size());
}

OutputLayerBeginFrameInfo EglBackend::beginFrame()
{
    makeCurrent();

    QRegion repaint;
    if (supportsBufferAge()) {
        repaint = m_damageJournal.accumulate(m_bufferAge, infiniteRegion());
    }

    eglWaitNative(EGL_CORE_NATIVE_ENGINE);

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_fbo.get()),
        .repaint = repaint,
    };
}

void EglBackend::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    // Save the damaged region to history
    if (supportsBufferAge()) {
        m_damageJournal.add(damagedRegion);
    }
    m_lastRenderedRegion = renderedRegion;
}

void EglBackend::present(Output *output)
{
    // Start the software vsync monitor. There is no any reliable way to determine when
    // eglSwapBuffers() or eglSwapBuffersWithDamageEXT() completes.
    m_vsyncMonitor->arm();

    QRegion effectiveRenderedRegion = m_lastRenderedRegion;
    if (!GLPlatform::instance()->isGLES()) {
        const QRect displayRect = workspace()->geometry();
        if (!supportsBufferAge() && options->glPreferBufferSwap() == Options::CopyFrontBuffer && m_lastRenderedRegion != displayRect) {
            glReadBuffer(GL_FRONT);
            copyPixels(QRegion(displayRect) - m_lastRenderedRegion, displayRect.size());
            glReadBuffer(GL_BACK);
            effectiveRenderedRegion = displayRect;
        }
    }

    presentSurface(surface(), effectiveRenderedRegion, workspace()->geometry());

    if (overlayWindow() && overlayWindow()->window()) { // show the window only after the first pass,
        overlayWindow()->show(); // since that pass may take long
    }
}

void EglBackend::presentSurface(EGLSurface surface, const QRegion &damage, const QRect &screenGeometry)
{
    const bool fullRepaint = supportsBufferAge() || (damage == screenGeometry);

    if (fullRepaint || !havePostSubBuffer()) {
        // the entire screen changed, or we cannot do partial updates (which implies we enabled surface preservation)
        eglSwapBuffers(eglDisplay(), surface);
        if (supportsBufferAge()) {
            eglQuerySurface(eglDisplay(), surface, EGL_BUFFER_AGE_EXT, &m_bufferAge);
        }
    } else {
        // a part of the screen changed, and we can use eglPostSubBufferNV to copy the updated area
        for (const QRect &r : damage) {
            eglPostSubBufferNV(eglDisplay(), surface, r.left(), screenGeometry.height() - r.bottom() - 1, r.width(), r.height());
        }
    }
}

OverlayWindow *EglBackend::overlayWindow() const
{
    return m_overlayWindow.get();
}

OutputLayer *EglBackend::primaryLayer(Output *output)
{
    return m_layer.get();
}

void EglBackend::vblank(std::chrono::nanoseconds timestamp)
{
    RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(m_backend->renderLoop());
    renderLoopPrivate->notifyFrameCompleted(timestamp);
}

EglSurfaceTextureX11::EglSurfaceTextureX11(EglBackend *backend, SurfacePixmapX11 *texture)
    : OpenGLSurfaceTextureX11(backend, texture)
{
}

bool EglSurfaceTextureX11::create()
{
    auto texture = std::make_unique<EglPixmapTexture>(static_cast<EglBackend *>(m_backend));
    if (texture->create(m_pixmap)) {
        m_texture = std::move(texture);
        return true;
    } else {
        return false;
    }
}

void EglSurfaceTextureX11::update(const QRegion &region)
{
    // mipmaps need to be updated
    m_texture->setDirty();
}

EglPixmapTexture::EglPixmapTexture(EglBackend *backend)
    : GLTexture(*new EglPixmapTexturePrivate(this, backend))
{
}

bool EglPixmapTexture::create(SurfacePixmapX11 *texture)
{
    Q_D(EglPixmapTexture);
    return d->create(texture);
}

EglPixmapTexturePrivate::EglPixmapTexturePrivate(EglPixmapTexture *texture, EglBackend *backend)
    : q(texture)
    , m_backend(backend)
{
    m_target = GL_TEXTURE_2D;
}

EglPixmapTexturePrivate::~EglPixmapTexturePrivate()
{
    if (m_image != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(m_backend->eglDisplay(), m_image);
    }
}

bool EglPixmapTexturePrivate::create(SurfacePixmapX11 *pixmap)
{
    const xcb_pixmap_t nativePixmap = pixmap->pixmap();
    if (nativePixmap == XCB_NONE) {
        return false;
    }

    glGenTextures(1, &m_texture);
    q->setWrapMode(GL_CLAMP_TO_EDGE);
    q->setFilter(GL_LINEAR);
    q->bind();
    const EGLint attribs[] = {
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        EGL_NONE};
    m_image = eglCreateImageKHR(m_backend->eglDisplay(),
                                EGL_NO_CONTEXT,
                                EGL_NATIVE_PIXMAP_KHR,
                                reinterpret_cast<EGLClientBuffer>(static_cast<uintptr_t>(nativePixmap)),
                                attribs);

    if (EGL_NO_IMAGE_KHR == m_image) {
        qCDebug(KWIN_X11STANDALONE) << "failed to create egl image";
        q->unbind();
        return false;
    }
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, static_cast<GLeglImageOES>(m_image));
    q->unbind();
    q->setYInverted(true);
    m_size = pixmap->size();
    updateMatrix();
    return true;
}

void EglPixmapTexturePrivate::onDamage()
{
    if (options->isGlStrictBinding()) {
        // This is just implemented to be consistent with
        // the example in mesa/demos/src/egl/opengles1/texture_from_pixmap.c
        eglWaitNative(EGL_CORE_NATIVE_ENGINE);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, static_cast<GLeglImageOES>(m_image));
    }
    GLTexturePrivate::onDamage();
}

} // namespace KWin
