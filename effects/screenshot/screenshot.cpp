/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2010 Martin Gräßlin <kde@martin-graesslin.com>
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
#include <QtCore/QVarLengthArray>
#include <QtGui/QPainter>

#include <X11/extensions/Xfixes.h>
#include <QX11Info>

namespace KWin
{

KWIN_EFFECT(screenshot, ScreenShotEffect)
KWIN_EFFECT_SUPPORTED(screenshot, ScreenShotEffect::supported())

bool ScreenShotEffect::supported()
{
    return  effects->compositingType() == XRenderCompositing ||
            (effects->compositingType() == KWin::OpenGLCompositing && GLRenderTarget::supported());
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
void ScreenShotEffect::postPaintScreen()
{
    effects->postPaintScreen();
    if (m_scheduledScreenshot) {
        int w = displayWidth();
        int h = displayHeight();
        bool validTarget = true;
        GLTexture* offscreenTexture = 0;
        GLRenderTarget* target = 0;
        if (effects->compositingType() == KWin::OpenGLCompositing) {
            if (!GLTexture::NPOTTextureSupported()) {
                w = nearestPowerOfTwo(w);
                h = nearestPowerOfTwo(h);
            }
            offscreenTexture = new GLTexture(w, h);
            offscreenTexture->setFilter(GL_LINEAR);
            offscreenTexture->setWrapMode(GL_CLAMP_TO_EDGE);
            target = new GLRenderTarget(*offscreenTexture);
            validTarget = target->valid();
        }
        if (validTarget) {
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
            int width = right - left;
            int height = bottom - top;
            d.xTranslate = -m_scheduledScreenshot->x() - left;
            d.yTranslate = -m_scheduledScreenshot->y() - top;

            // render window into offscreen texture
            int mask = PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_TRANSLUCENT;
            QImage img;
            if (effects->compositingType() == KWin::OpenGLCompositing) {
                GLRenderTarget::pushRenderTarget(target);
                glClearColor(0.0, 0.0, 0.0, 0.0);
                glClear(GL_COLOR_BUFFER_BIT);
                glClearColor(0.0, 0.0, 0.0, 1.0);
                effects->drawWindow(m_scheduledScreenshot, mask, QRegion(0, 0, width, height), d);
                // copy content from framebuffer into image
                img = QImage(QSize(width, height), QImage::Format_ARGB32);
                glReadPixels(0, offscreenTexture->height() - height, width, height, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)img.bits());
                GLRenderTarget::popRenderTarget();
                ScreenShotEffect::convertFromGLImage(img, width, height);
            }
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
            if (effects->compositingType() == XRenderCompositing) {
                setXRenderOffscreen(true);
                effects->drawWindow(m_scheduledScreenshot, mask, QRegion(0, 0, width, height), d);
                if (xRenderOffscreenTarget())
                    img = xRenderOffscreenTarget()->toImage().copy(0, 0, width, height);
                setXRenderOffscreen(false);
            }
#endif

            if (m_type & INCLUDE_CURSOR) {
                grabPointerImage(img, m_scheduledScreenshot->x() + left, m_scheduledScreenshot->y() + top);
            }

            m_lastScreenshot = QPixmap::fromImage(img);
            if (m_lastScreenshot.handle() == 0) {
                Pixmap xpix = XCreatePixmap(display(), rootWindow(), m_lastScreenshot.width(),
                                            m_lastScreenshot.height(), 32);
                m_lastScreenshot = QPixmap::fromX11Pixmap(xpix, QPixmap::ExplicitlyShared);
                QPainter p(&m_lastScreenshot);
                p.setCompositionMode(QPainter::CompositionMode_Source);
                p.drawImage(QPoint(0, 0), img);
            }
            emit screenshotCreated(m_lastScreenshot.handle());
        }
        delete offscreenTexture;
        delete target;
        m_scheduledScreenshot = NULL;
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
    if (effects->compositingType() == KWin::OpenGLCompositing)
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

    if (effects->compositingType() == XRenderCompositing) {
        QPixmap buffer(geometry.size());
        if (buffer.handle() == 0) {
            Pixmap xpix = XCreatePixmap(display(), rootWindow(), geometry.width(), geometry.height(), 32);
            buffer = QPixmap::fromX11Pixmap(xpix, QPixmap::ExplicitlyShared);
        }
        XRenderComposite(display(), PictOpSrc, effects->xrenderBufferPicture(), None, buffer.x11PictureHandle(),
                                    0, 0, 0, 0, geometry.x(), geometry.y(), geometry.width(), geometry.height());
        img = buffer.toImage();
    }

    KTemporaryFile temp;
    temp.setSuffix(".png");
    temp.setAutoRemove(false);
    if (!temp.open()) {
        return QString();
    }
    img.save(&temp);
    temp.close();
    return temp.fileName();
#endif
}

void ScreenShotEffect::grabPointerImage(QImage& snapshot, int offsetx, int offsety)
// Uses the X11_EXTENSIONS_XFIXES_H extension to grab the pointer image, and overlays it onto the snapshot.
{
    XFixesCursorImage *xcursorimg = XFixesGetCursorImage(QX11Info::display());
    if (!xcursorimg)
        return;

    //Annoyingly, xfixes specifies the data to be 32bit, but places it in an unsigned long *
    //which can be 64 bit.  So we need to iterate over a 64bit structure to put it in a 32bit
    //structure.
    QVarLengthArray< quint32 > pixels(xcursorimg->width * xcursorimg->height);
    for (int i = 0; i < xcursorimg->width * xcursorimg->height; ++i)
        pixels[i] = xcursorimg->pixels[i] & 0xffffffff;

    QImage qcursorimg((uchar *) pixels.data(), xcursorimg->width, xcursorimg->height,
                      QImage::Format_ARGB32_Premultiplied);

    QPainter painter(&snapshot);
    painter.drawImage(QPointF(xcursorimg->x - xcursorimg->xhot - offsetx, xcursorimg->y - xcursorimg ->yhot - offsety), qcursorimg);

    XFree(xcursorimg);
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
    return m_scheduledScreenshot != NULL;
}

void ScreenShotEffect::windowClosed( EffectWindow* w )
{
    if (w == m_scheduledScreenshot) {
        m_scheduledScreenshot = NULL;
        screenshotWindowUnderCursor(m_type);
    }
}

} // namespace
