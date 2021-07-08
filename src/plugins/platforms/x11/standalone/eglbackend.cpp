/*
    SPDX-FileCopyrightText: 2010, 2012 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "eglbackend.h"
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
}

EglBackend::~EglBackend()
{
    // No completion events will be received for in-flight frames, this may lock the
    // render loop. We need to ensure that the render loop is back to its initial state
    // if the render backend is about to be destroyed.
    RenderLoopPrivate::get(kwinApp()->platform()->renderLoop())->invalidate();
}

SurfaceTextureProvider *EglBackend::createSurfaceTextureProviderX11(SurfacePixmapX11 *texture)
{
    return new EglSurfaceTextureProviderX11(this, texture);
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

void EglBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)

    // TODO: base implementation in OpenGLBackend

    // The back buffer contents are now undefined
    m_bufferAge = 0;
}

QRegion EglBackend::beginFrame(int screenId)
{
    Q_UNUSED(screenId)
    makeCurrent();

    const QSize size = screens()->size();
    glViewport(0, 0, size.width(), size.height());

    QRegion repaint;
    if (supportsBufferAge())
        repaint = accumulatedDamageHistory(m_bufferAge);

    eglWaitNative(EGL_CORE_NATIVE_ENGINE);

    return repaint;
}

void EglBackend::endFrame(int screenId, const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(screenId)

    // Start the software vsync monitor. There is no any reliable way to determine when
    // eglSwapBuffers() or eglSwapBuffersWithDamageEXT() completes.
    m_vsyncMonitor->arm();

    presentSurface(surface(), renderedRegion, screens()->geometry());

    if (overlayWindow() && overlayWindow()->window()) { // show the window only after the first pass,
        overlayWindow()->show();   // since that pass may take long
    }

    // Save the damaged region to history
    if (supportsBufferAge()) {
        addToDamageHistory(damagedRegion);
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

EglSurfaceTextureX11::EglSurfaceTextureX11(EglBackend *eglBackend, EGLImageKHR eglImage, GLTexture *texture,
                                           bool hasAlphaChannel)
    : m_backend(eglBackend)
    , m_image(eglImage)
    , m_nativeTexture(texture)
    , m_hasAlphaChannel(hasAlphaChannel)
{
}

EglSurfaceTextureX11::~EglSurfaceTextureX11()
{
    if (m_image != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(m_backend->eglDisplay(), m_image);
    }
}

void EglSurfaceTextureX11::bind()
{
    m_nativeTexture.texture->setFilter(filtering() == Linear ? GL_LINEAR : GL_NEAREST);
    m_nativeTexture.texture->setWrapMode(wrapMode() == Repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    m_nativeTexture.texture->bind();
}

void EglSurfaceTextureX11::reattach()
{
    m_nativeTexture.texture->bind();

    // This is just implemented to be consistent with the example in
    // mesa/demos/src/egl/opengles1/texture_from_pixmap.c
    eglWaitNative(EGL_CORE_NATIVE_ENGINE);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, static_cast<GLeglImageOES>(m_image));
}

bool EglSurfaceTextureX11::hasAlphaChannel() const
{
    return m_hasAlphaChannel;
}

KrkNative::KrkNativeTexture *EglSurfaceTextureX11::nativeTexture() const
{
    return const_cast<KrkNative::KrkOpenGLTexture *>(&m_nativeTexture);
}

EglSurfaceTextureProviderX11::EglSurfaceTextureProviderX11(EglBackend *backend, SurfacePixmapX11 *pixmap)
    : OpenGLSurfaceTextureProvider(backend)
    , m_pixmap(pixmap)
{
}

EglBackend *EglSurfaceTextureProviderX11::backend() const
{
    return static_cast<EglBackend *>(OpenGLSurfaceTextureProvider::backend());
}

bool EglSurfaceTextureProviderX11::create()
{
    const xcb_pixmap_t nativePixmap = m_pixmap->pixmap();
    if (nativePixmap == XCB_NONE) {
        return false;
    }

    const EGLint attribs[] = {
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        EGL_NONE
    };
    EGLImageKHR image = eglCreateImageKHR(backend()->eglDisplay(), EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR,
                                          reinterpret_cast<EGLClientBuffer>(nativePixmap), attribs);
    if (image == EGL_NO_IMAGE_KHR) {
        qCDebug(KWIN_CORE) << "failed to create egl image";
        return false;
    }

    GLTexture *texture = new GLTexture(GL_TEXTURE_2D);
    texture->setYInverted(true);
    texture->setSize(m_pixmap->size());
    texture->create();
    texture->setWrapMode(GL_CLAMP_TO_EDGE);
    texture->setFilter(GL_LINEAR);
    texture->bind();
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, static_cast<GLeglImageOES>(image));
    texture->unbind();

    m_sceneTexture.reset(new EglSurfaceTextureX11(backend(), image, texture, m_pixmap->hasAlphaChannel()));
    return true;
}

void EglSurfaceTextureProviderX11::update(const QRegion &region)
{
    if (options->isGlStrictBinding() && !region.isEmpty()) {
        auto eglTexture = static_cast<EglSurfaceTextureX11 *>(m_sceneTexture.data());
        eglTexture->reattach();
    }
}

} // namespace KWin
