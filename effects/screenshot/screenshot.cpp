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
#include <kwinglplatform.h>
#include <kwinglutils.h>
#include <kwinxrenderutils.h>
#include <QtCore/QTemporaryFile>
#include <QtCore/QDir>
#include <QtDBus/QDBusConnection>
#include <QVarLengthArray>
#include <QtGui/QPainter>
#include <QMatrix4x4>
#include <xcb/xcb_image.h>

#include <KLocalizedString>
#include <KNotification>

namespace KWin
{

bool ScreenShotEffect::supported()
{
    return  effects->compositingType() == XRenderCompositing ||
            (effects->isOpenGLCompositing() && GLRenderTarget::supported());
}

ScreenShotEffect::ScreenShotEffect()
    : m_scheduledScreenshot(0)
    , m_replyConnection(QDBusConnection::sessionBus())
{
    connect ( effects, SIGNAL(windowClosed(KWin::EffectWindow*)), SLOT(windowClosed(KWin::EffectWindow*)) );
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/Screenshot"), this, QDBusConnection::ExportScriptableContents);
    QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.kwin.Screenshot"));
}

ScreenShotEffect::~ScreenShotEffect()
{
    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/Screenshot"));
    QDBusConnection::sessionBus().unregisterService(QStringLiteral("org.kde.kwin.Screenshot"));
}

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
static QImage xPictureToImage(xcb_render_picture_t srcPic, const QRect &geometry, xcb_image_t **xImage)
{
    xcb_connection_t *c = effects->xcbConnection();
    xcb_pixmap_t xpix = xcb_generate_id(c);
    xcb_create_pixmap(c, 32, xpix, effects->x11RootWindow(), geometry.width(), geometry.height());
    XRenderPicture pic(xpix, 32);
    xcb_render_composite(c, XCB_RENDER_PICT_OP_SRC, srcPic, XCB_RENDER_PICTURE_NONE, pic,
                         geometry.x(), geometry.y(), 0, 0, 0, 0, geometry.width(), geometry.height());
    xcb_flush(c);
    *xImage = xcb_image_get(c, xpix, 0, 0, geometry.width(), geometry.height(), ~0, XCB_IMAGE_FORMAT_Z_PIXMAP);
    QImage img((*xImage)->data, (*xImage)->width, (*xImage)->height, (*xImage)->stride, QImage::Format_ARGB32_Premultiplied);
    // TODO: byte order might need swapping
    xcb_free_pixmap(c, xpix);
    return img.copy();
}
#endif

void ScreenShotEffect::paintScreen(int mask, QRegion region, ScreenPaintData &data)
{
    m_cachedOutputGeometry = data.outputGeometry();
    effects->paintScreen(mask, region, data);

    if (m_infoFrame) {
        m_infoFrame->render(region);
    }
}

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
            offscreenTexture.reset(new GLTexture(GL_RGBA8, width, height));
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

                QMatrix4x4 projection;
                projection.ortho(QRect(0, 0, offscreenTexture->width(), offscreenTexture->height()));
                d.setProjectionMatrix(projection);

                effects->drawWindow(m_scheduledScreenshot, mask, infiniteRegion(), d);

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

            if (m_windowMode == WindowMode::Xpixmap) {
                const int depth = img.depth();
                xcb_pixmap_t xpix = xcb_generate_id(xcbConnection());
                xcb_create_pixmap(xcbConnection(), depth, xpix, x11RootWindow(), img.width(), img.height());

                xcb_gcontext_t cid = xcb_generate_id(xcbConnection());
                xcb_create_gc(xcbConnection(), cid, xpix, 0, NULL);
                xcb_put_image(xcbConnection(), XCB_IMAGE_FORMAT_Z_PIXMAP, xpix, cid, img.width(), img.height(),
                            0, 0, 0, depth, img.byteCount(), img.constBits());
                xcb_free_gc(xcbConnection(), cid);
                xcb_flush(xcbConnection());
                emit screenshotCreated(xpix);
                m_windowMode = WindowMode::NoCapture;
            } else if (m_windowMode == WindowMode::File) {
                sendReplyImage(img);
            }
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
            if (xImage) {
                xcb_image_destroy(xImage);
            }
#endif
        }
        m_scheduledScreenshot = NULL;
    }

    if (!m_scheduledGeometry.isNull()) {
        if (!m_cachedOutputGeometry.isNull()) {
            // special handling for per-output geometry rendering
            const QRect intersection = m_scheduledGeometry.intersected(m_cachedOutputGeometry);
            if (intersection.isEmpty()) {
                // doesn't intersect, not going onto this screenshot
                return;
            }
            const QImage img = blitScreenshot(intersection);
            if (img.size() == m_scheduledGeometry.size()) {
                // we are done
                sendReplyImage(img);
                return;
            }
            if (m_multipleOutputsImage.isNull()) {
                m_multipleOutputsImage = QImage(m_scheduledGeometry.size(), QImage::Format_ARGB32);
                m_multipleOutputsImage.fill(Qt::transparent);
            }
            QPainter p;
            p.begin(&m_multipleOutputsImage);
            p.drawImage(intersection.topLeft() - m_scheduledGeometry.topLeft(), img);
            p.end();
            m_multipleOutputsRendered = m_multipleOutputsRendered.united(intersection);
            if (m_multipleOutputsRendered.boundingRect() == m_scheduledGeometry) {
                sendReplyImage(m_multipleOutputsImage);
            }

        } else {
            const QImage img = blitScreenshot(m_scheduledGeometry);
            sendReplyImage(img);
        }
    }
}

void ScreenShotEffect::sendReplyImage(const QImage &img)
{
    m_replyConnection.send(m_replyMessage.createReply(saveTempImage(img)));
    m_scheduledGeometry = QRect();
    m_multipleOutputsImage = QImage();
    m_multipleOutputsRendered = QRegion();
    m_captureCursor = false;
    m_windowMode = WindowMode::NoCapture;
}

QString ScreenShotEffect::saveTempImage(const QImage &img)
{
    if (img.isNull()) {
        return QString();
    }
    QTemporaryFile temp(QDir::tempPath() + QDir::separator() + QLatin1String("kwin_screenshot_XXXXXX.png"));
    temp.setAutoRemove(false);
    if (!temp.open()) {
        return QString();
    }
    img.save(&temp);
    temp.close();
    KNotification::event(KNotification::Notification,
                        i18nc("Notification caption that a screenshot got saved to file", "Screenshot"),
                        i18nc("Notification with path to screenshot file", "Screenshot saved to %1", temp.fileName()),
                        QStringLiteral("spectacle"));
    return temp.fileName();
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
        m_windowMode = WindowMode::Xpixmap;
        m_scheduledScreenshot = w;
        m_scheduledScreenshot->addRepaintFull();
    }
}

QString ScreenShotEffect::interactive(int mask)
{
    if (!calledFromDBus()) {
        return QString();
    }
    if (!m_scheduledGeometry.isNull() || m_windowMode != WindowMode::NoCapture) {
        sendErrorReply(QDBusError::Failed, "A screenshot is already been taken");
        return QString();
    }
    m_type = (ScreenShotType) mask;
    m_windowMode = WindowMode::File;
    m_replyConnection = connection();
    m_replyMessage = message();
    setDelayedReply(true);
    effects->startInteractiveWindowSelection(
        [this] (EffectWindow *w) {
            m_infoFrame.reset();
            if (!w) {
                m_replyConnection.send(m_replyMessage.createErrorReply(QDBusError::Failed, "Screenshot got cancelled"));
                m_windowMode = WindowMode::NoCapture;
                return;
            } else {
                m_scheduledScreenshot = w;
                m_scheduledScreenshot->addRepaintFull();
            }
    });


    if (m_infoFrame.isNull()) {
        m_infoFrame.reset(effects->effectFrame(EffectFrameStyled, false));
        QFont font;
        font.setBold(true);
        m_infoFrame->setFont(font);
        QRect area = effects->clientArea(ScreenArea, effects->activeScreen(), effects->currentDesktop());
        m_infoFrame->setPosition(QPoint(area.x() + area.width() / 2, area.y() + area.height() / 3));
        m_infoFrame->setText(i18n("Select window to screen shot with left click or enter.\nEscape or right click to cancel."));
        effects->addRepaintFull();
    }
    return QString();
}

QString ScreenShotEffect::screenshotFullscreen(bool captureCursor)
{
    if (!calledFromDBus()) {
        return QString();
    }
    if (!m_scheduledGeometry.isNull()) {
        sendErrorReply(QDBusError::Failed, "A screenshot is already been taken");
        return QString();
    }
    m_replyConnection = connection();
    m_replyMessage = message();
    setDelayedReply(true);
    m_scheduledGeometry = effects->virtualScreenGeometry();
    m_captureCursor = captureCursor;
    effects->addRepaintFull();
    return QString();
}

QString ScreenShotEffect::screenshotScreen(int screen, bool captureCursor)
{
    if (!calledFromDBus()) {
        return QString();
    }
    if (!m_scheduledGeometry.isNull()) {
        sendErrorReply(QDBusError::Failed, "A screenshot is already been taken");
        return QString();
    }
    m_scheduledGeometry = effects->clientArea(FullScreenArea, screen, 0);
    if (m_scheduledGeometry.isNull()) {
        sendErrorReply(QDBusError::Failed, "Invalid screen requested");
        return QString();
    }
    m_captureCursor = captureCursor;
    m_replyConnection = connection();
    m_replyMessage = message();
    setDelayedReply(true);
    effects->addRepaint(m_scheduledGeometry);
    return QString();
}

QString ScreenShotEffect::screenshotArea(int x, int y, int width, int height, bool captureCursor)
{
    if (!calledFromDBus()) {
        return QString();
    }
    if (!m_scheduledGeometry.isNull()) {
        sendErrorReply(QDBusError::Failed, "A screenshot is already been taken");
        return QString();
    }
    m_scheduledGeometry = QRect(x, y, width, height);
    if (m_scheduledGeometry.isNull() || m_scheduledGeometry.isEmpty()) {
        m_scheduledGeometry = QRect();
        sendErrorReply(QDBusError::Failed, "Invalid area requested");
        return QString();
    }
    m_captureCursor = captureCursor;
    m_replyConnection = connection();
    m_replyMessage = message();
    setDelayedReply(true);
    effects->addRepaint(m_scheduledGeometry);
    return QString();
}

QImage ScreenShotEffect::blitScreenshot(const QRect &geometry)
{
    QImage img;
    if (effects->isOpenGLCompositing())
    {
        if (!GLRenderTarget::blitSupported()) {
            qCDebug(KWINEFFECTS) << "Framebuffer Blit not supported";
            return img;
        }
        GLTexture tex(GL_RGBA8, geometry.width(), geometry.height());
        GLRenderTarget target(tex);
        target.blitFromFramebuffer(geometry);
        // copy content from framebuffer into image
        tex.bind();
        img = QImage(geometry.size(), QImage::Format_ARGB32);
        if (GLPlatform::instance()->isGLES()) {
            glReadPixels(0, 0, img.width(), img.height(), GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)img.bits());
        } else {
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)img.bits());
        }
        tex.unbind();
        ScreenShotEffect::convertFromGLImage(img, geometry.width(), geometry.height());
    }

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (effects->compositingType() == XRenderCompositing) {
    xcb_image_t *xImage = NULL;
        img = xPictureToImage(effects->xrenderBufferPicture(), geometry, &xImage);
        if (xImage) {
            xcb_image_destroy(xImage);
        }
    }
#endif

    if (m_captureCursor) {
        grabPointerImage(img, geometry.x(), geometry.y());
    }

    return img;
}

void ScreenShotEffect::grabPointerImage(QImage& snapshot, int offsetx, int offsety)
{
    const auto cursor = effects->cursorImage();
    if (cursor.image().isNull())
        return;

    QPainter painter(&snapshot);
    painter.drawImage(effects->cursorPos() - cursor.hotSpot() - QPoint(offsetx, offsety), cursor.image());
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
    return (m_scheduledScreenshot != NULL || !m_scheduledGeometry.isNull() || !m_infoFrame.isNull()) && !effects->isScreenLocked();
}

void ScreenShotEffect::windowClosed( EffectWindow* w )
{
    if (w == m_scheduledScreenshot) {
        m_scheduledScreenshot = NULL;
        screenshotWindowUnderCursor(m_type);
    }
}

} // namespace
