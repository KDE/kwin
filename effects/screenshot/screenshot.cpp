/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2010 Martin Gräßlin <mgraesslin@kde.org>
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "screenshot.h"
#include <kwinglutils.h>
#include <kwinxrenderutils.h>
#include <KDE/KDebug>
#include <KDE/KTemporaryFile>
#include <QtDBus/QDBusConnection>
#include <QVarLengthArray>
#include <QtGui/QPainter>
#include <QMatrix4x4>
#include <xcb/xcb_image.h>

namespace KWin
{

KWIN_EFFECT(screenshot, ScreenShotEffect)
KWIN_EFFECT_SUPPORTED(screenshot, ScreenShotEffect::supported())

bool ScreenShotEffect::supported()
{
    return  effects->compositingType() == XRenderCompositing ||
            (effects->isOpenGLCompositing() && GLRenderTarget::supported());
}

ScreenShotEffect::ScreenShotEffect()
    : m_scheduledScreenshot(0)
{
    connect ( effects, SIGNAL(windowClosed(KWin::EffectWindow*)), SLOT(windowClosed(KWin::EffectWindow*)) );
    QDBusConnection::sessionBus().registerObject("/Screenshot", this, QDBusConnection::ExportScriptableContents);
    QDBusConnection::sessionBus().registerService("org.kde.kwin.Screenshot");
}

ScreenShotEffect::~ScreenShotEffect()
{
    QDBusConnection::sessionBus().unregisterObject("/Screenshot");
    QDBusConnection::sessionBus().unregisterService("org.kde.kwin.Screenshot");
}

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
static QImage xPictureToImage(xcb_render_picture_t srcPic, const QRect &geometry, xcb_image_t **xImage)
{
    xcb_pixmap_t xpix = xcb_generate_id(connection());
    xcb_create_pixmap(connection(), 32, xpix, rootWindow(), geometry.width(), geometry.height());
    XRenderPicture pic(xpix, 32);
    xcb_render_composite(connection(), XCB_RENDER_PICT_OP_SRC, srcPic, XCB_RENDER_PICTURE_NONE, pic,
                         geometry.x(), geometry.y(), 0, 0, 0, 0, geometry.width(), geometry.height());
    xcb_flush(connection());
    *xImage = xcb_image_get(connection(), xpix, 0, 0, geometry.width(), geometry.height(), ~0, XCB_IMAGE_FORMAT_Z_PIXMAP);
    QImage img((*xImage)->data, (*xImage)->width, (*xImage)->height, (*xImage)->stride, QImage::Format_ARGB32_Premultiplied);
    // TODO: byte order might need swapping
    xcb_free_pixmap(connection(), xpix);
    return img;
}
#endif

void ScreenShotEffect::postPaintScreen()
{
    effects->postPaintScreen();
    if (m_scheduledScreenshot) {
        WindowPaintData d(m_scheduledScreenshot);
        double left = 0;
        double top = 0;
        double right = m_scheduledScreenshot->width();
        double bottom = m_scheduledScreenshot->height();
        if (m_scheduledScreenshot->hasDecoration() && m_type & INCLUDE_DECORATION) {
            foreach (const WindowQuad & quad, d.quads) {
                // we need this loop to include the decoration padding
                left   = qMin(left, quad.left());
                top    = qMin(top, quad.top());
                right  = qMax(right, quad.right());
                bottom = qMax(bottom, quad.bottom());
            }
        } else if (m_scheduledScreenshot->hasDecoration()) {
            WindowQuadList newQuads;
            left = m_scheduledScreenshot->width();
            top = m_scheduledScreenshot->height();
            right = 0;
            bottom = 0;
            foreach (const WindowQuad & quad, d.quads) {
                if (quad.type() == WindowQuadContents) {
                    newQuads << quad;
                    left   = qMin(left, quad.left());
                    top    = qMin(top, quad.top());
                    right  = qMax(right, quad.right());
                    bottom = qMax(bottom, quad.bottom());
                }
            }
            d.quads = newQuads;
        }
        const int width = right - left;
        const int height = bottom - top;
        bool validTarget = true;
        QScopedPointer<GLTexture> offscreenTexture;
        QScopedPointer<GLRenderTarget> target;
        if (effects->isOpenGLCompositing()) {
            int w = width;
            int h = height;
            if (!GLTexture::NPOTTextureSupported()) {
                w = nearestPowerOfTwo(w);
                h = nearestPowerOfTwo(h);
            }
            offscreenTexture.reset(new GLTexture(w, h));
            offscreenTexture->setFilter(GL_LINEAR);
            offscreenTexture->setWrapMode(GL_CLAMP_TO_EDGE);
            target.reset(new GLRenderTarget(*offscreenTexture));
            validTarget = target->valid();
        }
        if (validTarget) {
            d.setXTranslation(-m_scheduledScreenshot->x() - left);
            d.setYTranslation(-m_scheduledScreenshot->y() - top);

            // render window into offscreen texture
            int mask = PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_TRANSLUCENT;
            QImage img;
            if (effects->isOpenGLCompositing()) {
                GLRenderTarget::pushRenderTarget(target.data());
                glClearColor(0.0, 0.0, 0.0, 0.0);
                glClear(GL_COLOR_BUFFER_BIT);
                glClearColor(0.0, 0.0, 0.0, 1.0);
                setMatrix(offscreenTexture->width(), offscreenTexture->height());
                effects->drawWindow(m_scheduledScreenshot, mask, infiniteRegion(), d);
                restoreMatrix();
                // copy content from framebuffer into image
                img = QImage(QSize(width, height), QImage::Format_ARGB32);
                glReadnPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, img.byteCount(), (GLvoid*)img.bits());
                GLRenderTarget::popRenderTarget();
                ScreenShotEffect::convertFromGLImage(img, width, height);
            }
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
            xcb_image_t *xImage = NULL;
            if (effects->compositingType() == XRenderCompositing) {
                setXRenderOffscreen(true);
                effects->drawWindow(m_scheduledScreenshot, mask, QRegion(0, 0, width, height), d);
                if (xRenderOffscreenTarget()) {
                    img = xPictureToImage(xRenderOffscreenTarget(), QRect(0, 0, width, height), &xImage);
                }
                setXRenderOffscreen(false);
            }
#endif

            if (m_type & INCLUDE_CURSOR) {
                grabPointerImage(img, m_scheduledScreenshot->x() + left, m_scheduledScreenshot->y() + top);
            }

            const int depth = img.depth();
            xcb_pixmap_t xpix = xcb_generate_id(connection());
            xcb_create_pixmap(connection(), depth, xpix, rootWindow(), img.width(), img.height());

            xcb_gcontext_t cid = xcb_generate_id(connection());
            xcb_create_gc(connection(), cid, xpix, 0, NULL);
            xcb_put_image(connection(), XCB_IMAGE_FORMAT_Z_PIXMAP, xpix, cid, img.width(), img.height(),
                        0, 0, 0, depth, img.byteCount(), img.constBits());
            xcb_free_gc(connection(), cid);
            xcb_flush(connection());
            emit screenshotCreated(xpix);
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
            if (xImage) {
                xcb_image_destroy(xImage);
            }
#endif
        }
        m_scheduledScreenshot = NULL;
    }
}

static QMatrix4x4 s_origProjection;
static QMatrix4x4 s_origModelview;

void ScreenShotEffect::setMatrix(int width, int height)
{
    QMatrix4x4 projection;
    QMatrix4x4 identity;
    projection.ortho(QRect(0, 0, width, height));
    if (effects->compositingType() == OpenGL2Compositing) {
        ShaderBinder binder(ShaderManager::GenericShader);
        GLShader *shader = binder.shader();
        s_origProjection = shader->getUniformMatrix4x4("projection");
        s_origModelview = shader->getUniformMatrix4x4("modelview");
        shader->setUniform(GLShader::ProjectionMatrix, projection);
        shader->setUniform(GLShader::ModelViewMatrix, identity);
    } else if (effects->compositingType() == OpenGL1Compositing) {
#ifdef KWIN_HAVE_OPENGL_1
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        loadMatrix(projection);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
#endif
    }
}

void ScreenShotEffect::restoreMatrix()
{
    if (effects->compositingType() == OpenGL2Compositing) {
        ShaderBinder binder(ShaderManager::GenericShader);
        GLShader *shader = binder.shader();
        shader->setUniform(GLShader::ProjectionMatrix, s_origProjection);
        shader->setUniform(GLShader::ModelViewMatrix, s_origModelview);
    } else if (effects->compositingType() == OpenGL1Compositing) {
#ifdef KWIN_HAVE_OPENGL_1
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
#endif
    }
}

void ScreenShotEffect::screenshotWindowUnderCursor(int mask)
{
    m_type = (ScreenShotType)mask;
    const QPoint cursor = effects->cursorPos();
    EffectWindowList order = effects->stackingOrder();
    EffectWindowList::const_iterator it = order.constEnd(), first = order.constBegin();
    while( it != first ) {
        m_scheduledScreenshot = *(--it);
        if (m_scheduledScreenshot->isOnCurrentDesktop() &&
            !m_scheduledScreenshot->isMinimized() && !m_scheduledScreenshot->isDeleted() &&
            m_scheduledScreenshot->geometry().contains(cursor))
            break;
        m_scheduledScreenshot = 0;
    }
    if (m_scheduledScreenshot) {
        m_scheduledScreenshot->addRepaintFull();
    }
}

void ScreenShotEffect::screenshotForWindow(qulonglong winid, int mask)
{
    m_type = (ScreenShotType) mask;
    EffectWindow* w = effects->findWindow(winid);
    if(w && !w->isMinimized() && !w->isDeleted()) {
        m_scheduledScreenshot = w;
        m_scheduledScreenshot->addRepaintFull();
    }
}

QString ScreenShotEffect::screenshotFullscreen()
{
    return blitScreenshot(QRect(0, 0, displayWidth(), displayHeight()));
}

QString ScreenShotEffect::screenshotScreen(int screen)
{
    return blitScreenshot(effects->clientArea(FullScreenArea, screen, 0));
}

QString ScreenShotEffect::screenshotArea(int x, int y, int width, int height)
{
    return blitScreenshot(QRect(x, y, width, height));
}

QString ScreenShotEffect::blitScreenshot(const QRect &geometry)
{
#ifdef KWIN_HAVE_OPENGLES
    Q_UNUSED(geometry)
    kDebug(1212) << "Framebuffer Blit not supported";
    return QString();
#else
    QImage img;
    if (effects->isOpenGLCompositing())
    {
        if (!GLRenderTarget::blitSupported()) {
            kDebug(1212) << "Framebuffer Blit not supported";
            return QString();
        }
        GLTexture tex(geometry.width(), geometry.height());
        GLRenderTarget target(tex);
        target.blitFromFramebuffer(geometry);
        // copy content from framebuffer into image
        tex.bind();
        img = QImage(geometry.size(), QImage::Format_ARGB32);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)img.bits());
        tex.unbind();
        ScreenShotEffect::convertFromGLImage(img, geometry.width(), geometry.height());
    }

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    xcb_image_t *xImage = NULL;
#endif
    if (effects->compositingType() == XRenderCompositing) {
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
        img = xPictureToImage(effects->xrenderBufferPicture(), geometry, &xImage);
#endif
    }

    KTemporaryFile temp;
    temp.setSuffix(".png");
    temp.setAutoRemove(false);
    if (!temp.open()) {
        return QString();
    }
    img.save(&temp);
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (xImage) {
        xcb_image_destroy(xImage);
    }
#endif
    temp.close();
    return temp.fileName();
#endif
}

void ScreenShotEffect::grabPointerImage(QImage& snapshot, int offsetx, int offsety)
// Uses the X11_EXTENSIONS_XFIXES_H extension to grab the pointer image, and overlays it onto the snapshot.
{
    QScopedPointer<xcb_xfixes_get_cursor_image_reply_t, QScopedPointerPodDeleter> cursor(
        xcb_xfixes_get_cursor_image_reply(connection(),
                                          xcb_xfixes_get_cursor_image_unchecked(connection()),
                                          NULL));
    if (cursor.isNull())
        return;

    QImage qcursorimg((uchar *) xcb_xfixes_get_cursor_image_cursor_image(cursor.data()), cursor->width, cursor->height,
                      QImage::Format_ARGB32_Premultiplied);

    QPainter painter(&snapshot);
    painter.drawImage(QPointF(cursor->x - cursor->xhot - offsetx, cursor->y - cursor ->yhot - offsety), qcursorimg);
}

void ScreenShotEffect::convertFromGLImage(QImage &img, int w, int h)
{
    // from QtOpenGL/qgl.cpp
    // Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
    // see http://qt.gitorious.org/qt/qt/blobs/master/src/opengl/qgl.cpp
    if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
        // OpenGL gives RGBA; Qt wants ARGB
        uint *p = (uint*)img.bits();
        uint *end = p + w * h;
        while (p < end) {
            uint a = *p << 24;
            *p = (*p >> 8) | a;
            p++;
        }
    } else {
        // OpenGL gives ABGR (i.e. RGBA backwards); Qt wants ARGB
        for (int y = 0; y < h; y++) {
            uint *q = (uint*)img.scanLine(y);
            for (int x = 0; x < w; ++x) {
                const uint pixel = *q;
                *q = ((pixel << 16) & 0xff0000) | ((pixel >> 16) & 0xff)
                     | (pixel & 0xff00ff00);

                q++;
            }
        }

    }
    img = img.mirrored();
}

bool ScreenShotEffect::isActive() const
{
    return m_scheduledScreenshot != NULL && !effects->isScreenLocked();
}

void ScreenShotEffect::windowClosed( EffectWindow* w )
{
    if (w == m_scheduledScreenshot) {
        m_scheduledScreenshot = NULL;
        screenshotWindowUnderCursor(m_type);
    }
}

} // namespace
