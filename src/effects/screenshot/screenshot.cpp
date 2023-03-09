/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2010 Nokia Corporation and /or its subsidiary(-ies)
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screenshot.h"
#include "screenshotdbusinterface2.h"

#include <kwinglplatform.h>
#include <kwinglutils.h>
#include <rendertarget.h>
#include <renderviewport.h>

#include <QPainter>

namespace KWin
{

struct ScreenShotWindowData
{
    QPromise<QImage> promise;
    ScreenShotFlags flags;
    EffectWindow *window = nullptr;
};

struct ScreenShotAreaData
{
    QPromise<QImage> promise;
    ScreenShotFlags flags;
    QRect area;
    QImage result;
    QList<EffectScreen *> screens;
};

struct ScreenShotScreenData
{
    QPromise<QImage> promise;
    ScreenShotFlags flags;
    EffectScreen *screen = nullptr;
};

static void convertFromGLImage(QImage &img, int w, int h, const QMatrix4x4 &renderTargetTransformation)
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

    QMatrix4x4 matrix;
    // OpenGL textures are flipped vs QImage
    matrix.scale(1, -1);
    // apply render target transformation
    matrix *= renderTargetTransformation.inverted();
    img = img.transformed(matrix.toTransform());
}

bool ScreenShotEffect::supported()
{
    return effects->isOpenGLCompositing() && GLFramebuffer::supported();
}

ScreenShotEffect::ScreenShotEffect()
    : m_dbusInterface2(new ScreenShotDBusInterface2(this))
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
    for (const ScreenShotScreenData &data : m_screenScreenShots) {
        if (data.screen == screen && data.flags == flags) {
            return data.promise.future();
        }
    }

    ScreenShotScreenData data;
    data.screen = screen;
    data.flags = flags;

    data.promise.start();
    QFuture<QImage> future = data.promise.future();

    m_screenScreenShots.push_back(std::move(data));
    effects->addRepaint(screen->geometry());

    return future;
}

QFuture<QImage> ScreenShotEffect::scheduleScreenShot(const QRect &area, ScreenShotFlags flags)
{
    for (const ScreenShotAreaData &data : m_areaScreenShots) {
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
        for (const EffectScreen *screen : std::as_const(data.screens)) {
            if (screen->devicePixelRatio() > devicePixelRatio) {
                devicePixelRatio = screen->devicePixelRatio();
            }
        }
    }

    data.result = QImage(area.size() * devicePixelRatio, QImage::Format_ARGB32_Premultiplied);
    data.result.fill(Qt::transparent);
    data.result.setDevicePixelRatio(devicePixelRatio);

    data.promise.start();
    QFuture<QImage> future = data.promise.future();

    m_areaScreenShots.push_back(std::move(data));
    effects->addRepaint(area);

    return future;
}

QFuture<QImage> ScreenShotEffect::scheduleScreenShot(EffectWindow *window, ScreenShotFlags flags)
{
    for (const ScreenShotWindowData &data : m_windowScreenShots) {
        if (data.window == window && data.flags == flags) {
            return data.promise.future();
        }
    }

    ScreenShotWindowData data;
    data.window = window;
    data.flags = flags;

    data.promise.start();
    QFuture<QImage> future = data.promise.future();

    m_windowScreenShots.push_back(std::move(data));
    window->addRepaintFull();

    return future;
}

void ScreenShotEffect::cancelWindowScreenShots()
{
    m_windowScreenShots.clear();
}

void ScreenShotEffect::cancelAreaScreenShots()
{
    m_areaScreenShots.clear();
}

void ScreenShotEffect::cancelScreenScreenShots()
{
    m_screenScreenShots.clear();
}

void ScreenShotEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, EffectScreen *screen)
{
    m_paintedScreen = screen;
    effects->paintScreen(renderTarget, viewport, mask, region, screen);

    for (ScreenShotWindowData &data : m_windowScreenShots) {
        takeScreenShot(&data);
    }
    m_windowScreenShots.clear();

    for (int i = m_areaScreenShots.size() - 1; i >= 0; --i) {
        if (takeScreenShot(renderTarget, viewport, &m_areaScreenShots[i])) {
            m_areaScreenShots.erase(m_areaScreenShots.begin() + i);
        }
    }

    for (int i = m_screenScreenShots.size() - 1; i >= 0; --i) {
        if (takeScreenShot(renderTarget, viewport, &m_screenScreenShots[i])) {
            m_screenScreenShots.erase(m_screenScreenShots.begin() + i);
        }
    }
}

void ScreenShotEffect::takeScreenShot(ScreenShotWindowData *screenshot)
{
    EffectWindow *window = screenshot->window;

    WindowPaintData d;
    QRectF geometry = window->expandedGeometry();
    qreal devicePixelRatio = 1;
    if (window->hasDecoration() && !(screenshot->flags & ScreenShotIncludeDecoration)) {
        geometry = window->clientGeometry();
    }
    if (screenshot->flags & ScreenShotNativeResolution) {
        if (const EffectScreen *screen = window->screen()) {
            devicePixelRatio = screen->devicePixelRatio();
        }
    }
    bool validTarget = true;
    std::unique_ptr<GLTexture> offscreenTexture;
    std::unique_ptr<GLFramebuffer> target;
    if (effects->isOpenGLCompositing()) {
        offscreenTexture.reset(new GLTexture(GL_RGBA8, QSizeF(geometry.size() * devicePixelRatio).toSize()));
        offscreenTexture->setFilter(GL_LINEAR);
        offscreenTexture->setWrapMode(GL_CLAMP_TO_EDGE);
        target.reset(new GLFramebuffer(offscreenTexture.get()));
        validTarget = target->valid();
    }
    if (validTarget) {
        d.setXTranslation(-geometry.x());
        d.setYTranslation(-geometry.y());

        // render window into offscreen texture
        int mask = PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_TRANSLUCENT;
        QImage img;
        if (effects->isOpenGLCompositing()) {
            RenderTarget renderTarget(target.get());
            RenderViewport viewport(geometry, devicePixelRatio, renderTarget);
            GLFramebuffer::pushFramebuffer(target.get());
            glClearColor(0.0, 0.0, 0.0, 0.0);
            glClear(GL_COLOR_BUFFER_BIT);
            glClearColor(0.0, 0.0, 0.0, 1.0);

            QMatrix4x4 projection;
            projection.ortho(QRect(0, 0, geometry.width() * devicePixelRatio, geometry.height() * devicePixelRatio));
            d.setProjectionMatrix(projection);

            effects->drawWindow(renderTarget, viewport, window, mask, infiniteRegion(), d);

            // copy content from framebuffer into image
            img = QImage(offscreenTexture->size(), QImage::Format_ARGB32);
            img.setDevicePixelRatio(devicePixelRatio);
            glReadnPixels(0, 0, img.width(), img.height(), GL_RGBA, GL_UNSIGNED_BYTE, img.sizeInBytes(),
                          static_cast<GLvoid *>(img.bits()));
            GLFramebuffer::popFramebuffer();
            convertFromGLImage(img, img.width(), img.height(), renderTarget.transformation());
        }

        if (screenshot->flags & ScreenShotIncludeCursor) {
            grabPointerImage(img, geometry.x(), geometry.y());
        }

        screenshot->promise.addResult(img);
        screenshot->promise.finish();
    }
}

bool ScreenShotEffect::takeScreenShot(const RenderTarget &renderTarget, const RenderViewport &viewport, ScreenShotAreaData *screenshot)
{
    if (!effects->waylandDisplay()) {
        // On X11, all screens are painted simultaneously and there is no native HiDPI support.
        QImage snapshot = blitScreenshot(renderTarget, viewport, screenshot->area);
        if (screenshot->flags & ScreenShotIncludeCursor) {
            grabPointerImage(snapshot, screenshot->area.x(), screenshot->area.y());
        }
        screenshot->promise.addResult(snapshot);
        screenshot->promise.finish();
        return true;
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

        const QImage snapshot = blitScreenshot(renderTarget, viewport, sourceRect, sourceDevicePixelRatio);
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
            screenshot->promise.addResult(screenshot->result);
            screenshot->promise.finish();
            return true;
        }
    }

    return false;
}

bool ScreenShotEffect::takeScreenShot(const RenderTarget &renderTarget, const RenderViewport &viewport, ScreenShotScreenData *screenshot)
{
    if (m_paintedScreen && screenshot->screen != m_paintedScreen) {
        return false;
    }

    qreal devicePixelRatio = 1.0;
    if (screenshot->flags & ScreenShotNativeResolution) {
        devicePixelRatio = screenshot->screen->devicePixelRatio();
    }

    QImage snapshot = blitScreenshot(renderTarget, viewport, screenshot->screen->geometry(), devicePixelRatio);
    if (screenshot->flags & ScreenShotIncludeCursor) {
        const int xOffset = screenshot->screen->geometry().x();
        const int yOffset = screenshot->screen->geometry().y();
        grabPointerImage(snapshot, xOffset, yOffset);
    }

    screenshot->promise.addResult(snapshot);
    screenshot->promise.finish();

    return true;
}

QImage ScreenShotEffect::blitScreenshot(const RenderTarget &renderTarget, const RenderViewport &viewport, const QRect &geometry, qreal devicePixelRatio) const
{
    QImage image;

    if (effects->isOpenGLCompositing()) {
        const auto screenGeometry = m_paintedScreen ? m_paintedScreen->geometry() : effects->virtualScreenGeometry();
        const QSize nativeSize = renderTarget.applyTransformation(geometry, screenGeometry).size() * devicePixelRatio;
        image = QImage(nativeSize, QImage::Format_ARGB32);

        if (GLFramebuffer::blitSupported() && !GLPlatform::instance()->isGLES()) {
            GLTexture texture(GL_RGBA8, nativeSize.width(), nativeSize.height());
            GLFramebuffer target(&texture);
            target.blitFromFramebuffer(viewport.mapToRenderTarget(geometry));
            // copy content from framebuffer into image
            texture.bind();
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                          static_cast<GLvoid *>(image.bits()));
            texture.unbind();
        } else {
            glReadPixels(0, 0, nativeSize.width(), nativeSize.height(), GL_RGBA,
                         GL_UNSIGNED_BYTE, static_cast<GLvoid *>(image.bits()));
        }
        convertFromGLImage(image, nativeSize.width(), nativeSize.height(), renderTarget.transformation());
    }

    image.setDevicePixelRatio(devicePixelRatio);
    return image;
}

void ScreenShotEffect::grabPointerImage(QImage &snapshot, int xOffset, int yOffset) const
{
    if (effects->isCursorHidden()) {
        return;
    }

    const PlatformCursorImage cursor = effects->cursorImage();
    if (cursor.image().isNull()) {
        return;
    }

    QPainter painter(&snapshot);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(effects->cursorPos() - cursor.hotSpot() - QPoint(xOffset, yOffset), cursor.image());
}

bool ScreenShotEffect::isActive() const
{
    return (!m_windowScreenShots.empty() || !m_areaScreenShots.empty() || !m_screenScreenShots.empty())
        && !effects->isScreenLocked();
}

int ScreenShotEffect::requestedEffectChainPosition() const
{
    return 0;
}

void ScreenShotEffect::handleScreenAdded()
{
    cancelAreaScreenShots();
}

void ScreenShotEffect::handleScreenRemoved(EffectScreen *screen)
{
    cancelAreaScreenShots();

    std::erase_if(m_screenScreenShots, [screen](const auto &screenshot) {
        return screenshot.screen == screen;
    });
}

void ScreenShotEffect::handleWindowClosed(EffectWindow *window)
{
    std::erase_if(m_windowScreenShots, [window](const auto &screenshot) {
        return screenshot.window == window;
    });
}

} // namespace KWin
