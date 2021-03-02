/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2010 Nokia Corporation and /or its subsidiary(-ies)
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screenshot.h"
#include "screenshotdbusinterface1.h"
#include "screenshotdbusinterface2.h"

#include <kwinglplatform.h>
#include <kwinglutils.h>

#include <QPainter>

#if defined(KWIN_HAVE_XRENDER_COMPOSITING)
#include <kwinxrenderutils.h>
#include <xcb/xcb_image.h>
#endif

namespace KWin
{

struct ScreenShotWindowData
{
    QFutureInterface<QImage> promise;
    ScreenShotFlags flags;
    EffectWindow *window = nullptr;
};

struct ScreenShotAreaData
{
    QFutureInterface<QImage> promise;
    ScreenShotFlags flags;
    QRect area;
    QImage result;
    QList<EffectScreen *> screens;
};

struct ScreenShotScreenData
{
    QFutureInterface<QImage> promise;
    ScreenShotFlags flags;
    EffectScreen *screen = nullptr;
};

static void convertFromGLImage(QImage &img, int w, int h)
{
    // from QtOpenGL/qgl.cpp
    // SPDX-FileCopyrightText: 2010 Nokia Corporation and /or its subsidiary(-ies)
    // see https://github.com/qt/qtbase/blob/dev/src/opengl/qgl.cpp
    if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
        // OpenGL gives RGBA; Qt wants ARGB
        uint *p = reinterpret_cast<uint *>(img.bits());
        uint *end = p + w * h;
        while (p < end) {
            uint a = *p << 24;
            *p = (*p >> 8) | a;
            p++;
        }
    } else {
        // OpenGL gives ABGR (i.e. RGBA backwards); Qt wants ARGB
        for (int y = 0; y < h; y++) {
            uint *q = reinterpret_cast<uint *>(img.scanLine(y));
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

#if defined(KWIN_HAVE_XRENDER_COMPOSITING)
static void xImageCleanup(void *data)
{
    xcb_image_destroy(static_cast<xcb_image_t *>(data));
}

static QImage xPictureToImage(xcb_render_picture_t srcPic, const QRect &geometry)
{
    xcb_connection_t *c = effects->xcbConnection();
    xcb_pixmap_t xpix = xcb_generate_id(c);
    xcb_create_pixmap(c, 32, xpix, effects->x11RootWindow(), geometry.width(), geometry.height());
    XRenderPicture pic(xpix, 32);
    xcb_render_composite(c, XCB_RENDER_PICT_OP_SRC, srcPic, XCB_RENDER_PICTURE_NONE, pic,
                         geometry.x(), geometry.y(), 0, 0, 0, 0, geometry.width(), geometry.height());
    xcb_flush(c);
    xcb_image_t *xImage = xcb_image_get(c, xpix, 0, 0, geometry.width(), geometry.height(),
                                        ~0, XCB_IMAGE_FORMAT_Z_PIXMAP);
    QImage img(xImage->data, xImage->width, xImage->height, xImage->stride,
               QImage::Format_ARGB32_Premultiplied, xImageCleanup, xImage);
    // TODO: byte order might need swapping
    xcb_free_pixmap(c, xpix);
    return img.copy();
}
#endif

bool ScreenShotEffect::supported()
{
    return effects->compositingType() == XRenderCompositing ||
           (effects->isOpenGLCompositing() && GLRenderTarget::supported());
}

ScreenShotEffect::ScreenShotEffect()
    : m_dbusInterface1(new ScreenShotDBusInterface1(this))
    , m_dbusInterface2(new ScreenShotDBusInterface2(this))
{
    connect(effects, &EffectsHandler::screenAdded, this, &ScreenShotEffect::handleScreenAdded);
    connect(effects, &EffectsHandler::screenRemoved, this, &ScreenShotEffect::handleScreenRemoved);
    connect(effects, &EffectsHandler::windowClosed, this, &ScreenShotEffect::handleWindowClosed);
}

ScreenShotEffect::~ScreenShotEffect()
{
    cancelWindowScreenShots();
    cancelAreaScreenShots();
    cancelScreenScreenShots();
}

QFuture<QImage> ScreenShotEffect::scheduleScreenShot(EffectScreen *screen, ScreenShotFlags flags)
{
    for (ScreenShotScreenData &data : m_screenScreenShots) {
        if (data.screen == screen && data.flags == flags) {
            return data.promise.future();
        }
    }

    ScreenShotScreenData data;
    data.screen = screen;
    data.flags = flags;

    m_screenScreenShots.append(data);
    effects->addRepaint(screen->geometry());

    data.promise.reportStarted();
    return data.promise.future();
}

QFuture<QImage> ScreenShotEffect::scheduleScreenShot(const QRect &area, ScreenShotFlags flags)
{
    for (ScreenShotAreaData &data : m_areaScreenShots) {
        if (data.area == area && data.flags == flags) {
            return data.promise.future();
        }
    }

    ScreenShotAreaData data;
    data.area = area;
    data.flags = flags;

    const QList<EffectScreen *> screens = effects->screens();
    for (EffectScreen *screen : screens) {
        if (screen->geometry().intersects(area)) {
            data.screens.append(screen);
        }
    }

    qreal devicePixelRatio = 1.0;
    if (flags & ScreenShotNativeResolution) {
        for (const EffectScreen *screen : qAsConst(data.screens)) {
            if (screen->devicePixelRatio() > devicePixelRatio) {
                devicePixelRatio = screen->devicePixelRatio();
            }
        }
    }

    data.result = QImage(area.size() * devicePixelRatio, QImage::Format_ARGB32_Premultiplied);
    data.result.fill(Qt::transparent);
    data.result.setDevicePixelRatio(devicePixelRatio);

    m_areaScreenShots.append(data);
    effects->addRepaint(area);

    data.promise.reportStarted();
    return data.promise.future();
}

QFuture<QImage> ScreenShotEffect::scheduleScreenShot(EffectWindow *window, ScreenShotFlags flags)
{
    for (ScreenShotWindowData &data : m_windowScreenShots) {
        if (data.window == window && data.flags == flags) {
            return data.promise.future();
        }
    }

    ScreenShotWindowData data;
    data.window = window;
    data.flags = flags;

    m_windowScreenShots.append(data);
    window->addRepaintFull();

    data.promise.reportStarted();
    return data.promise.future();
}

void ScreenShotEffect::cancelWindowScreenShots()
{
    while (!m_windowScreenShots.isEmpty()) {
        ScreenShotWindowData screenshot = m_windowScreenShots.takeLast();
        screenshot.promise.reportCanceled();
    }
}

void ScreenShotEffect::cancelAreaScreenShots()
{
    while (!m_areaScreenShots.isEmpty()) {
        ScreenShotAreaData screenshot = m_areaScreenShots.takeLast();
        screenshot.promise.reportCanceled();
    }
}

void ScreenShotEffect::cancelScreenScreenShots()
{
    while (!m_screenScreenShots.isEmpty()) {
        ScreenShotScreenData screenshot = m_screenScreenShots.takeLast();
        screenshot.promise.reportCanceled();
    }
}

void ScreenShotEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    m_paintedScreen = data.screen();
    effects->paintScreen(mask, region, data);
}

void ScreenShotEffect::takeScreenShot(ScreenShotWindowData *screenshot)
{
    EffectWindow *window = screenshot->window;

    WindowPaintData d(window);
    double left = 0;
    double top = 0;
    double right = window->width();
    double bottom = window->height();
    if (window->hasDecoration() && (screenshot->flags & ScreenShotIncludeDecoration)) {
        for (const WindowQuad &quad : qAsConst(d.quads)) {
            // we need this loop to include the decoration padding
            left = qMin(left, quad.left());
            top = qMin(top, quad.top());
            right = qMax(right, quad.right());
            bottom = qMax(bottom, quad.bottom());
        }
    } else if (window->hasDecoration()) {
        WindowQuadList newQuads;
        left = window->width();
        top = window->height();
        right = 0;
        bottom = 0;
        for (const WindowQuad &quad : qAsConst(d.quads)) {
            if (quad.type() == WindowQuadContents) {
                newQuads << quad;
                left = qMin(left, quad.left());
                top = qMin(top, quad.top());
                right = qMax(right, quad.right());
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
        d.setXTranslation(-window->x() - left);
        d.setYTranslation(-window->y() - top);

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

            effects->drawWindow(window, mask, infiniteRegion(), d);

            // copy content from framebuffer into image
            img = QImage(QSize(width, height), QImage::Format_ARGB32);
            glReadnPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, img.sizeInBytes(),
                          static_cast<GLvoid *>(img.bits()));
            GLRenderTarget::popRenderTarget();
            convertFromGLImage(img, width, height);
        }
#if defined(KWIN_HAVE_XRENDER_COMPOSITING)
        if (effects->compositingType() == XRenderCompositing) {
            setXRenderOffscreen(true);
            effects->drawWindow(window, mask, QRegion(0, 0, width, height), d);
            if (xRenderOffscreenTarget()) {
                img = xPictureToImage(xRenderOffscreenTarget(), QRect(0, 0, width, height));
            }
            setXRenderOffscreen(false);
        }
#endif

        if (screenshot->flags & ScreenShotIncludeCursor) {
            grabPointerImage(img, window->x() + left, window->y() + top);
        }

        screenshot->promise.reportResult(img);
        screenshot->promise.reportFinished();
    } else {
        screenshot->promise.reportCanceled();
    }
}

bool ScreenShotEffect::takeScreenShot(ScreenShotAreaData *screenshot)
{
    if (!m_paintedScreen) {
        // On X11, all screens are painted simultaneously and there is no native HiDPI support.
        QImage snapshot = blitScreenshot(screenshot->area);
        if (screenshot->flags & ScreenShotIncludeCursor) {
            grabPointerImage(snapshot, screenshot->area.x(), screenshot->area.y());
        }
        screenshot->promise.reportResult(snapshot);
        screenshot->promise.reportFinished();
    } else {
        if (!screenshot->screens.contains(m_paintedScreen)) {
            return false;
        }
        screenshot->screens.removeOne(m_paintedScreen);

        const QRect sourceRect = screenshot->area & m_paintedScreen->geometry();
        qreal sourceDevicePixelRatio = 1.0;
        if (screenshot->flags & ScreenShotNativeResolution) {
            sourceDevicePixelRatio = m_paintedScreen->devicePixelRatio();
        }

        const QImage snapshot = blitScreenshot(sourceRect, sourceDevicePixelRatio);
        const QRect nativeArea(screenshot->area.topLeft(),
                               screenshot->area.size() * screenshot->result.devicePixelRatio());

        QPainter painter(&screenshot->result);
        painter.setWindow(nativeArea);
        painter.drawImage(sourceRect, snapshot);
        painter.end();

        if (screenshot->screens.isEmpty()) {
            if (screenshot->flags & ScreenShotIncludeCursor) {
                grabPointerImage(screenshot->result, screenshot->area.x(), screenshot->area.y());
            }
            screenshot->promise.reportResult(screenshot->result);
            screenshot->promise.reportFinished();
        }
    }

    return screenshot->promise.isFinished();
}

bool ScreenShotEffect::takeScreenShot(ScreenShotScreenData *screenshot)
{
    if (!m_paintedScreen || screenshot->screen == m_paintedScreen) {
        qreal devicePixelRatio = 1.0;
        if (screenshot->flags & ScreenShotNativeResolution) {
            devicePixelRatio = screenshot->screen->devicePixelRatio();
        }

        QImage snapshot = blitScreenshot(screenshot->screen->geometry(), devicePixelRatio);
        if (screenshot->flags & ScreenShotIncludeCursor) {
            const int xOffset = screenshot->screen->geometry().width();
            const int yOffset = screenshot->screen->geometry().height();
            grabPointerImage(snapshot, xOffset, yOffset);
        }

        screenshot->promise.reportResult(snapshot);
        screenshot->promise.reportFinished();
    }

    return screenshot->promise.isFinished();
}

void ScreenShotEffect::postPaintScreen()
{
    effects->postPaintScreen();

    while (!m_windowScreenShots.isEmpty()) {
        ScreenShotWindowData screenshot = m_windowScreenShots.takeLast();
        takeScreenShot(&screenshot);
    }

    for (int i = m_areaScreenShots.count() - 1; i >= 0; --i) {
        if (takeScreenShot(&m_areaScreenShots[i])) {
            m_areaScreenShots.removeAt(i);
        }
    }

    for (int i = m_screenScreenShots.count() - 1; i >= 0; --i) {
        if (takeScreenShot(&m_screenScreenShots[i])) {
            m_screenScreenShots.removeAt(i);
        }
    }
}

QImage ScreenShotEffect::blitScreenshot(const QRect &geometry, qreal devicePixelRatio) const
{
    QImage image;

    if (effects->isOpenGLCompositing()) {
        const QSize nativeSize = geometry.size() * devicePixelRatio;

        if (GLRenderTarget::blitSupported() && !GLPlatform::instance()->isGLES()) {
            image = QImage(nativeSize.width(), nativeSize.height(), QImage::Format_ARGB32);
            GLTexture texture(GL_RGBA8, nativeSize.width(), nativeSize.height());
            GLRenderTarget target(texture);
            target.blitFromFramebuffer(geometry);
            // copy content from framebuffer into image
            texture.bind();
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                          static_cast<GLvoid *>(image.bits()));
            texture.unbind();
        } else {
            image = QImage(nativeSize.width(), nativeSize.height(), QImage::Format_ARGB32);
            glReadPixels(0, 0, nativeSize.width(), nativeSize.height(), GL_RGBA,
                         GL_UNSIGNED_BYTE, static_cast<GLvoid *>(image.bits()));
        }
        convertFromGLImage(image, nativeSize.width(), nativeSize.height());
    }

#if defined(KWIN_HAVE_XRENDER_COMPOSITING)
    if (effects->compositingType() == XRenderCompositing) {
        image = xPictureToImage(effects->xrenderBufferPicture(), geometry);
    }
#endif

    image.setDevicePixelRatio(devicePixelRatio);
    return image;
}

void ScreenShotEffect::grabPointerImage(QImage &snapshot, int xOffset, int yOffset) const
{
    const PlatformCursorImage cursor = effects->cursorImage();
    if (cursor.image().isNull()) {
        return;
    }

    QPainter painter(&snapshot);
    painter.drawImage(effects->cursorPos() - cursor.hotSpot() - QPoint(xOffset, yOffset), cursor.image());
}

bool ScreenShotEffect::isActive() const
{
    return (!m_windowScreenShots.isEmpty() || !m_areaScreenShots.isEmpty() || !m_screenScreenShots.isEmpty())
            && !effects->isScreenLocked();
}

int ScreenShotEffect::requestedEffectChainPosition() const
{
    return 50;
}

void ScreenShotEffect::handleScreenAdded()
{
    cancelAreaScreenShots();
}

void ScreenShotEffect::handleScreenRemoved(EffectScreen *screen)
{
    cancelAreaScreenShots();

    for (int i = m_screenScreenShots.count() - 1; i >= 0; --i) {
        if (m_screenScreenShots[i].screen == screen) {
            m_screenScreenShots[i].promise.reportCanceled();
            m_screenScreenShots.removeAt(i);
        }
    }
}

void ScreenShotEffect::handleWindowClosed(EffectWindow *window)
{
    for (int i = m_windowScreenShots.count() - 1; i >= 0; --i) {
        if (m_windowScreenShots[i].window == window) {
            m_windowScreenShots[i].promise.reportCanceled();
            m_windowScreenShots.removeAt(i);
        }
    }
}

} // namespace KWin
