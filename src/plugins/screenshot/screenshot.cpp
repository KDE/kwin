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

#include "compositor.h"
#include "core/output.h"
#include "core/pixelgrid.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effect.h"
#include "opengl/eglbackend.h"
#include "opengl/glplatform.h"
#include "opengl/glutils.h"
#include "scene/decorationitem.h"
#include "scene/item.h"
#include "scene/itemrenderer.h"
#include "scene/shadowitem.h"
#include "scene/surfaceitem.h"
#include "scene/windowitem.h"
#include "scene/workspacescene.h"
#include "screenshotlayer.h"
#include "window.h"
#include "workspace.h"

#include <QPainter>

namespace KWin
{

static void convertFromGLImage(QImage &img, int w, int h, const OutputTransform &renderTargetTransformation)
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
    // apply render target transformation
    matrix *= renderTargetTransformation.inverted().toMatrix();
    // OpenGL textures are flipped vs QImage
    matrix.scale(1, -1);
    img = img.transformed(matrix.toTransform());
}

ScreenShotManager::ScreenShotManager()
    : m_dbusInterface2(new ScreenShotDBusInterface2(this))
{
}

ScreenShotManager::~ScreenShotManager()
{
}

// TODO share code with the screencast plugin?

std::optional<QImage> ScreenShotManager::takeScreenShot(LogicalOutput *screen, ScreenShotFlags flags, std::optional<pid_t> pidToHide)
{
    const auto eglBackend = dynamic_cast<EglBackend *>(Compositor::self()->backend());
    if (!eglBackend) {
        return std::nullopt;
    }
    const auto context = eglBackend->openglContext();
    if (!context || !context->makeCurrent()) {
        return std::nullopt;
    }

    qreal scale = 1.0;
    if (flags & ScreenShotNativeResolution) {
        scale = screen->scale();
    }
    const QSize nativeSize = (screen->geometryF().size() * scale).toSize();

    const auto offscreenTexture = GLTexture::allocate(GL_RGBA8, nativeSize);
    if (!offscreenTexture) {
        return std::nullopt;
    }
    offscreenTexture->setFilter(GL_LINEAR);
    offscreenTexture->setWrapMode(GL_CLAMP_TO_EDGE);
    const auto target = std::make_unique<GLFramebuffer>(offscreenTexture.get());
    if (!target->valid()) {
        return std::nullopt;
    }

    ScreenshotLayer layer(screen, target.get());
    if (!layer.preparePresentationTest()) {
        return std::nullopt;
    }
    const auto beginInfo = layer.beginFrame();
    if (!beginInfo) {
        return std::nullopt;
    }
    SceneView sceneView(Compositor::self()->scene(), screen, nullptr, &layer);
    std::unique_ptr<ItemTreeView> cursorView;
    if (!(flags & ScreenShotIncludeCursor)) {
        cursorView = std::make_unique<ItemTreeView>(&sceneView, Compositor::self()->scene()->cursorItem(), workspace()->outputs().front(), nullptr, nullptr);
        cursorView->setExclusive(true);
    }
    if (pidToHide.has_value()) {
        sceneView.addWindowFilter([pid = *pidToHide](Window *window) {
            return window->pid() == pid;
        });
    }
    const QRect fullDamage = QRect(QPoint(), target->size());
    sceneView.setViewport(screen->geometryF());
    sceneView.setScale(scale);
    sceneView.prePaint();
    sceneView.paint(beginInfo->renderTarget, QPoint(), fullDamage);
    sceneView.postPaint();
    if (!layer.endFrame(fullDamage, fullDamage, nullptr)) {
        return std::nullopt;
    }

    GLFramebuffer::pushFramebuffer(target.get());
    QImage snapshot = QImage(offscreenTexture->size(), QImage::Format_ARGB32_Premultiplied);
    context->glReadnPixels(0, 0, snapshot.width(), snapshot.height(), GL_RGBA, GL_UNSIGNED_BYTE, snapshot.sizeInBytes(), static_cast<GLvoid *>(snapshot.bits()));
    convertFromGLImage(snapshot, snapshot.width(), snapshot.height(), OutputTransform::Normal);
    GLFramebuffer::popFramebuffer();

    snapshot.setDevicePixelRatio(scale);
    return snapshot;
}

std::optional<QImage> ScreenShotManager::takeScreenShot(const QRect &area, ScreenShotFlags flags, std::optional<pid_t> pidToHide)
{
    const auto eglBackend = dynamic_cast<EglBackend *>(Compositor::self()->backend());
    if (!eglBackend) {
        return std::nullopt;
    }
    const auto context = eglBackend->openglContext();
    if (!context || !context->makeCurrent()) {
        return std::nullopt;
    }

    qreal scale = 1.0;
    if (flags & ScreenShotNativeResolution) {
        const auto outputs = workspace()->outputs();
        for (LogicalOutput *output : outputs) {
            scale = std::max(scale, output->scale());
        }
    }
    const QSize nativeSize = area.size() * scale;

    const auto offscreenTexture = GLTexture::allocate(GL_RGBA8, nativeSize);
    if (!offscreenTexture) {
        return std::nullopt;
    }
    offscreenTexture->setFilter(GL_LINEAR);
    offscreenTexture->setWrapMode(GL_CLAMP_TO_EDGE);
    const auto target = std::make_unique<GLFramebuffer>(offscreenTexture.get());
    if (!target->valid()) {
        return std::nullopt;
    }

    ScreenshotLayer layer(workspace()->outputs().front(), target.get());
    if (!layer.preparePresentationTest()) {
        return std::nullopt;
    }
    const auto beginInfo = layer.beginFrame();
    if (!beginInfo) {
        return std::nullopt;
    }
    SceneView sceneView(Compositor::self()->scene(), workspace()->outputs().front(), nullptr, &layer);
    std::unique_ptr<ItemTreeView> cursorView;
    if (!(flags & ScreenShotIncludeCursor)) {
        cursorView = std::make_unique<ItemTreeView>(&sceneView, Compositor::self()->scene()->cursorItem(), workspace()->outputs().front(), nullptr, nullptr);
        cursorView->setExclusive(true);
    }
    if (pidToHide.has_value()) {
        sceneView.addWindowFilter([pid = *pidToHide](Window *window) {
            return window->pid() == pid;
        });
    }
    const QRect fullDamage = QRect(QPoint(), target->size());
    sceneView.setViewport(area);
    sceneView.setScale(scale);
    sceneView.prePaint();
    sceneView.paint(beginInfo->renderTarget, QPoint(), fullDamage);
    sceneView.postPaint();
    if (!layer.endFrame(fullDamage, fullDamage, nullptr)) {
        return std::nullopt;
    }

    GLFramebuffer::pushFramebuffer(target.get());
    QImage snapshot = QImage(offscreenTexture->size(), QImage::Format_ARGB32_Premultiplied);
    context->glReadnPixels(0, 0, snapshot.width(), snapshot.height(), GL_RGBA, GL_UNSIGNED_BYTE, snapshot.sizeInBytes(), static_cast<GLvoid *>(snapshot.bits()));
    convertFromGLImage(snapshot, snapshot.width(), snapshot.height(), OutputTransform::Normal);
    GLFramebuffer::popFramebuffer();

    snapshot.setDevicePixelRatio(scale);
    return snapshot;
}

std::optional<QImage> ScreenShotManager::takeScreenShot(Window *window, ScreenShotFlags flags)
{
    const auto eglBackend = dynamic_cast<EglBackend *>(Compositor::self()->backend());
    if (!eglBackend) {
        return std::nullopt;
    }
    const auto context = eglBackend->openglContext();
    if (!context || !context->makeCurrent()) {
        return std::nullopt;
    }

    const qreal scale = window->targetScale();
    QRectF geometry = window->visibleGeometry();
    if (window->windowItem()->decorationItem() && !(flags & ScreenShotIncludeDecoration)) {
        geometry = window->clientGeometry();
    } else if (!(flags & ScreenShotIncludeShadow)) {
        geometry = window->frameGeometry();
    }
    const QSize nativeSize = (geometry.size() * scale).toSize();
    const auto offscreenTexture = GLTexture::allocate(GL_RGBA8, nativeSize);
    if (!offscreenTexture) {
        return std::nullopt;
    }

    GLFramebuffer offscreenTarget(offscreenTexture.get());

    RenderTarget renderTarget(&offscreenTarget);
    RenderViewport viewport(geometry, scale, renderTarget, QPoint());

    WorkspaceScene *scene = Compositor::self()->scene();

    scene->renderer()->beginFrame(renderTarget, viewport);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    scene->renderer()->renderItem(renderTarget, viewport, window->windowItem(), Scene::PAINT_WINDOW_TRANSFORMED, infiniteRegion(), WindowPaintData{}, [flags, w = window->windowItem()](Item *item) {
        const bool deco = flags & ScreenShotFlag::ScreenShotIncludeDecoration;
        const bool shadow = deco && (flags & ScreenShotFlag::ScreenShotIncludeShadow);
        return (!deco && item == w->decorationItem())
            || (!shadow && item == w->shadowItem());
    }, {});
    if ((flags & ScreenShotFlag::ScreenShotIncludeCursor) && scene->cursorItem()->isVisible()) {
        scene->renderer()->renderItem(renderTarget, viewport, scene->cursorItem(), 0, infiniteRegion(), WindowPaintData{}, {}, {});
    }
    scene->renderer()->endFrame();

    GLFramebuffer::pushFramebuffer(&offscreenTarget);
    QImage snapshot = QImage(offscreenTexture->size(), QImage::Format_ARGB32_Premultiplied);
    context->glReadnPixels(0, 0, snapshot.width(), snapshot.height(), GL_RGBA, GL_UNSIGNED_BYTE, snapshot.sizeInBytes(), static_cast<GLvoid *>(snapshot.bits()));
    convertFromGLImage(snapshot, snapshot.width(), snapshot.height(), OutputTransform::Normal);
    GLFramebuffer::popFramebuffer();

    snapshot.setDevicePixelRatio(scale);
    return snapshot;
}

} // namespace KWin

#include "moc_screenshot.cpp"
