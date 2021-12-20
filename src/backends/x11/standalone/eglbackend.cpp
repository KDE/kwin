/*
    SPDX-FileCopyrightText: 2010, 2012 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "eglbackend.h"
#include "kwinglplatform.h"
#include "logging.h"
#include "options.h"
#include "overlaywindow.h"
#include "platform.h"
#include "renderloop_p.h"
#include "scene.h"
#include "screens.h"
#include "softwarevsyncmonitor.h"
#include "surfaceitem_x11.h"
#include "x11_platform.h"

#include <QOpenGLContext>
#include <QtPlatformHeaders/QEGLNativeContext>

namespace KWin
{

EglBackend::EglBackend(Display *display, X11StandalonePlatform *backend)
    : EglOnXBackend(display)
    , m_backend(backend)
{
    // There is no any way to determine when a buffer swap completes with EGL. Fallback
    // to software vblank events. Could we use the Present extension to get notified when
    // the overlay window is actually presented on the screen?
    m_vsyncMonitor = SoftwareVsyncMonitor::create(this);
    connect(backend->renderLoop(), &RenderLoop::refreshRateChanged, this, [this, backend]() {
        m_vsyncMonitor->setRefreshRate(backend->renderLoop()->refreshRate());
    });
    m_vsyncMonitor->setRefreshRate(backend->renderLoop()->refreshRate());

    connect(m_vsyncMonitor, &VsyncMonitor::vblankOccurred, this, &EglBackend::vblank);
    connect(screens(), &Screens::sizeChanged, this, &EglBackend::screenGeometryChanged);
}

EglBackend::~EglBackend()
{
    // No completion events will be received for in-flight frames, this may lock the
    // render loop. We need to ensure that the render loop is back to its initial state
    // if the render backend is about to be destroyed.
    RenderLoopPrivate::get(kwinApp()->platform()->renderLoop())->invalidate();
}

SurfaceTexture *EglBackend::createSurfaceTextureX11(SurfacePixmapX11 *texture)
{
    return new EglSurfaceTextureX11(this, texture);
}

void EglBackend::init()
{
    QOpenGLContext *qtShareContext = QOpenGLContext::globalShareContext();
    EGLDisplay shareDisplay = EGL_NO_DISPLAY;
    EGLContext shareContext = EGL_NO_CONTEXT;
    if (qtShareContext) {
        qDebug(KWIN_X11STANDALONE) << "Global share context format:" << qtShareContext->format();
        const QVariant nativeHandle = qtShareContext->nativeHandle();
        if (!nativeHandle.canConvert<QEGLNativeContext>()) {
            setFailed(QStringLiteral("Invalid QOpenGLContext::globalShareContext()"));
            return;
        } else {
            QEGLNativeContext handle = qvariant_cast<QEGLNativeContext>(nativeHandle);
            shareContext = handle.context();
            shareDisplay = handle.display();
        }
    }
    if (shareContext == EGL_NO_CONTEXT) {
        setFailed(QStringLiteral("QOpenGLContext::globalShareContext() is required"));
        return;
    }

    kwinApp()->platform()->setSceneEglDisplay(shareDisplay);
    kwinApp()->platform()->setSceneEglGlobalShareContext(shareContext);
    EglOnXBackend::init();
}

void EglBackend::screenGeometryChanged()
{
    overlayWindow()->resize(screens()->size());

    // The back buffer contents are now undefined
    m_bufferAge = 0;
}

QRegion EglBackend::beginFrame(AbstractOutput *output)
{
    Q_UNUSED(output)
    makeCurrent();

    const QSize size = screens()->size();
    glViewport(0, 0, size.width(), size.height());

    QRegion repaint;
    if (supportsBufferAge()) {
        repaint = m_damageJournal.accumulate(m_bufferAge, screens()->geometry());
    }

    eglWaitNative(EGL_CORE_NATIVE_ENGINE);

    return repaint;
}

void EglBackend::endFrame(AbstractOutput *output, const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(output)

    // Start the software vsync monitor. There is no any reliable way to determine when
    // eglSwapBuffers() or eglSwapBuffersWithDamageEXT() completes.
    m_vsyncMonitor->arm();

    QRegion effectiveRenderedRegion = renderedRegion;
    if (!GLPlatform::instance()->isGLES()) {
        const QRegion displayRegion(screens()->geometry());
        if (!supportsBufferAge() && options->glPreferBufferSwap() == Options::CopyFrontBuffer && renderedRegion != displayRegion) {
            glReadBuffer(GL_FRONT);
            copyPixels(displayRegion - renderedRegion);
            glReadBuffer(GL_BACK);
            effectiveRenderedRegion = displayRegion;
        }
    }

    presentSurface(surface(), effectiveRenderedRegion, screens()->geometry());

    if (overlayWindow() && overlayWindow()->window()) { // show the window only after the first pass,
        overlayWindow()->show();   // since that pass may take long
    }

    // Save the damaged region to history
    if (supportsBufferAge()) {
        m_damageJournal.add(damagedRegion);
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
    auto texture = new EglPixmapTexture(static_cast<EglBackend *>(m_backend));
    if (texture->create(m_pixmap)) {
        m_texture.reset(texture);
    }
    return !m_texture.isNull();
}

void EglSurfaceTextureX11::update(const QRegion &region)
{
    Q_UNUSED(region)
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
        EGL_NONE
    };
    m_image = eglCreateImageKHR(m_backend->eglDisplay(), EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR,
                                reinterpret_cast<EGLClientBuffer>(nativePixmap), attribs);

    if (EGL_NO_IMAGE_KHR == m_image) {
        qCDebug(KWIN_CORE) << "failed to create egl image";
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
