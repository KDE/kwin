/*
    SPDX-FileCopyrightText: 2010, 2012 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "eglbackend.h"
#include "options.h"
#include "overlaywindow.h"
#include "scene.h"
#include "screens.h"

namespace KWin
{

EglBackend::EglBackend(Display *display)
    : EglOnXBackend(display)
{
}

SceneOpenGLTexturePrivate *EglBackend::createBackendTexture(SceneOpenGLTexture *texture)
{
    return new EglTexture(texture, this);
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
    QRegion repaint;
    if (supportsBufferAge())
        repaint = accumulatedDamageHistory(m_bufferAge);

    eglWaitNative(EGL_CORE_NATIVE_ENGINE);

    return repaint;
}

void EglBackend::endFrame(int screenId, const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(screenId)

    if (damagedRegion.isEmpty()) {

        // If the damaged region of a window is fully occluded, the only
        // rendering done, if any, will have been to repair a reused back
        // buffer, making it identical to the front buffer.
        //
        // In this case we won't post the back buffer. Instead we'll just
        // set the buffer age to 1, so the repaired regions won't be
        // rendered again in the next frame.
        if (!renderedRegion.isEmpty())
            glFlush();

        m_bufferAge = 1;
        return;
    }

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
    if (damage.isEmpty()) {
        return;
    }
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

/************************************************
 * EglTexture
 ************************************************/

EglTexture::EglTexture(KWin::SceneOpenGLTexture *texture, EglBackend *backend)
    : AbstractEglTexture(texture, backend)
    , m_backend(backend)
{
}

EglTexture::~EglTexture()
{
}

bool EglTexture::loadTexture(WindowPixmap *pixmap)
{
    const xcb_pixmap_t nativePixmap = pixmap->pixmap();
    if (nativePixmap == XCB_NONE) {
        return false;
    }

    glGenTextures(1, &m_texture);
    auto q = texture();
    q->setWrapMode(GL_CLAMP_TO_EDGE);
    q->setFilter(GL_LINEAR);
    q->bind();
    const EGLint attribs[] = {
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        EGL_NONE
    };
    setImage(eglCreateImageKHR(m_backend->eglDisplay(), EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR,
                               (EGLClientBuffer)nativePixmap, attribs));

    if (EGL_NO_IMAGE_KHR == image()) {
        qCDebug(KWIN_CORE) << "failed to create egl image";
        q->unbind();
        q->discard();
        return false;
    }
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)image());
    q->unbind();
    q->setYInverted(true);
    m_size = pixmap->toplevel()->bufferGeometry().size();
    updateMatrix();
    return true;
}

void EglTexture::onDamage()
{
    if (options->isGlStrictBinding()) {
        // This is just implemented to be consistent with
        // the example in mesa/demos/src/egl/opengles1/texture_from_pixmap.c
        eglWaitNative(EGL_CORE_NATIVE_ENGINE);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES) image());
    }
    GLTexturePrivate::onDamage();
}

} // namespace KWin
